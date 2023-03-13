/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/carnot/exec/union_node.h"

#include <arrow/memory_pool.h>
#include <arrow/status.h>
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include <absl/strings/str_join.h>
#include <absl/strings/substitute.h>

#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/type_utils.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;

std::string UnionNode::DebugStringImpl() {
  return absl::Substitute("Exec::UnionNode<$0>", absl::StrJoin(plan_node_->column_names(), ","));
}

Status UnionNode::InitImpl(const plan::Operator& plan_node) {
  CHECK(plan_node.op_type() == planpb::OperatorType::UNION_OPERATOR);
  const auto* union_plan_node = static_cast<const plan::UnionOperator*>(&plan_node);
  plan_node_ = std::make_unique<plan::UnionOperator>(*union_plan_node);
  output_rows_per_batch_ =
      plan_node_->rows_per_batch() == 0 ? kDefaultUnionRowBatchSize : plan_node_->rows_per_batch();
  num_parents_ = input_descriptors_.size();

  return Status::OK();
}

Status UnionNode::InitializeColumnBuilders() {
  for (size_t i = 0; i < output_descriptor_->size(); ++i) {
    column_builders_[i] =
        MakeArrowBuilder(output_descriptor_->type(i), arrow::default_memory_pool());
    PX_RETURN_IF_ERROR(column_builders_[i]->Reserve(output_rows_per_batch_));
  }
  return Status::OK();
}

Status UnionNode::PrepareImpl(ExecState*) {
  size_t num_output_cols = output_descriptor_->size();

  flushed_parent_eoses_.resize(num_parents_);

  if (plan_node_->order_by_time()) {
    // Set up parent stream state.
    parent_row_batches_.resize(num_parents_);
    row_cursors_.resize(num_parents_);
    time_columns_.resize(num_parents_);
    data_columns_.resize(num_parents_, std::vector<arrow::Array*>(num_output_cols));

    column_builders_.resize(num_output_cols);
    PX_RETURN_IF_ERROR(InitializeColumnBuilders());
  }

  return Status::OK();
}

Status UnionNode::OpenImpl(ExecState*) { return Status::OK(); }

Status UnionNode::CloseImpl(ExecState*) { return Status::OK(); }

bool UnionNode::InputsComplete() {
  for (bool parent_eos : flushed_parent_eoses_) {
    if (!parent_eos) {
      return false;
    }
  }
  return true;
}

std::shared_ptr<arrow::Array> UnionNode::GetInputColumn(const RowBatch& rb, size_t parent_index,
                                                        size_t column_index) {
  DCHECK(output_descriptor_->size() == plan_node_->column_mapping(parent_index).size());
  auto input_index = plan_node_->column_mapping(parent_index)[column_index];
  return rb.ColumnAt(input_index);
}

types::Time64NSValue UnionNode::GetTimeAtParentCursor(size_t parent_index) const {
  DCHECK(!flushed_parent_eoses_[parent_index]);
  DCHECK(time_columns_[parent_index] != nullptr);
  return types::GetValueFromArrowArray<types::TIME64NS>(time_columns_[parent_index],
                                                        row_cursors_[parent_index]);
}

Status UnionNode::AppendRow(size_t parent) {
  auto row = row_cursors_[parent];
  for (size_t i = 0; i < output_descriptor_->size(); ++i) {
    auto input_col = data_columns_[parent][i];
#define TYPE_CASE(_dt_)                                    \
  PX_RETURN_IF_ERROR(table_store::schema::CopyValue<_dt_>( \
      column_builders_[i].get(), types::GetValueFromArrowArray<_dt_>(input_col, row)));
    PX_SWITCH_FOREACH_DATATYPE(output_descriptor_->type(i), TYPE_CASE);
#undef TYPE_CASE
  }
  return Status::OK();
}

// Flush the row batch if we have waited too long between row batches.
Status UnionNode::OptionallyFlushRowBatchIfTimeout(ExecState* exec_state) {
  if (!enable_data_flush_timeout_) {
    return Status::OK();
  }

  auto time_now = std::chrono::system_clock::now();
  auto since_last_flush =
      std::chrono::duration_cast<std::chrono::milliseconds>(time_now - last_data_flush_time_);
  bool timed_out = since_last_flush > data_flush_timeout_;

  if (!timed_out) {
    return Status::OK();
  }

  DCHECK_GT(column_builders_.size(), 0U);
  if (column_builders_[0]->length()) {
    return FlushBatch(exec_state);
  }
  return Status::OK();
}

// Flush the row batch if we have reached a certain number of records.
Status UnionNode::OptionallyFlushRowBatchIfMaxRowsOrEOS(ExecState* exec_state) {
  bool eos = InputsComplete();
  int64_t output_rows = column_builders_[0]->length();

  if (output_rows < static_cast<int64_t>(output_rows_per_batch_) && !eos) {
    return Status::OK();
  }

  return FlushBatch(exec_state);
}

Status UnionNode::FlushBatch(ExecState* exec_state) {
  DCHECK(!sent_eos_);

  bool eos = InputsComplete();
  PX_ASSIGN_OR_RETURN(auto rb, RowBatch::FromColumnBuilders(*output_descriptor_, /*eow*/ eos,
                                                            /*eos*/ eos, &column_builders_));
  PX_RETURN_IF_ERROR(InitializeColumnBuilders());
  last_data_flush_time_ = std::chrono::system_clock::now();
  return SendRowBatchToChildren(exec_state, *rb);
}

Status UnionNode::MergeData(ExecState* exec_state) {
  while (!sent_eos_) {
    // Get the smallest time out of all of the current streams.
    std::vector<size_t> parent_streams;
    parent_streams.reserve(row_cursors_.size());

    for (size_t parent = 0; parent < row_cursors_.size(); ++parent) {
      if (flushed_parent_eoses_[parent]) {
        continue;
      }
      // If we lack necessary data, we can't merge anymore.
      if (!parent_row_batches_[parent].size()) {
        return Status::OK();
      }
      parent_streams.push_back(parent);
    }

    // If we have reached end of stream for all of our inputs, flush the queue.
    if (!parent_streams.size()) {
      return OptionallyFlushRowBatchIfMaxRowsOrEOS(exec_state);
    }

    std::sort(parent_streams.begin(), parent_streams.end(),
              [this](size_t parent_a, size_t parent_b) {
                auto time_a = GetTimeAtParentCursor(parent_a);
                auto time_b = GetTimeAtParentCursor(parent_b);
                return time_a < time_b || (time_a == time_b && parent_a < parent_b);
              });

    bool has_limit = parent_streams.size() > 1;
    size_t parent = parent_streams[0];

    while (parent_row_batches_[parent].size()) {
      if (has_limit) {
        size_t next_parent = parent_streams[1];
        // If this time is greater than another parent stream's earliest time,
        // or if they are the same but the first parent is at a smaller index, stop merging.
        // This way rows are always stable with respect to input parent index.
        if (GetTimeAtParentCursor(parent) > GetTimeAtParentCursor(next_parent) ||
            (GetTimeAtParentCursor(parent) == GetTimeAtParentCursor(next_parent) &&
             parent > next_parent)) {
          break;
        }
      }

      PX_RETURN_IF_ERROR(AppendRow(parent));

      // Mark whether or not we hit the eos for this stream, and whether the row batch needs to be
      // popped.
      const auto& rb = parent_row_batches_[parent][0];
      bool pop_row_batch = ++row_cursors_[parent] == static_cast<size_t>(rb.num_rows());
      if (pop_row_batch && rb.eos()) {
        flushed_parent_eoses_[parent] = true;
      }

      if (pop_row_batch) {
        // Delete the top row batch from our buffer and update the cursor.
        parent_row_batches_[parent].erase(parent_row_batches_[parent].begin());
        row_cursors_[parent] = 0;
        CacheNextRowBatch(parent);
      }

      // Flush the current RowBatch if necessary.
      PX_RETURN_IF_ERROR(OptionallyFlushRowBatchIfMaxRowsOrEOS(exec_state));
    }
  }
  return Status::OK();
}

void UnionNode::CacheNextRowBatch(size_t parent) {
  // Purge all of the 0-row RowBatches.
  while (parent_row_batches_[parent].size() && parent_row_batches_[parent][0].num_rows() == 0) {
    if (parent_row_batches_[parent][0].eos()) {
      flushed_parent_eoses_[parent] = true;
    }
    parent_row_batches_[parent].erase(parent_row_batches_[parent].begin());
  }
  if (!parent_row_batches_[parent].size()) {
    return;
  }
  const auto& next_rb = parent_row_batches_[parent][0];
  time_columns_[parent] = next_rb.ColumnAt(plan_node_->time_column_index(parent)).get();

  for (size_t i = 0; i < output_descriptor_->size(); ++i) {
    data_columns_[parent][i] = GetInputColumn(next_rb, parent, i).get();
  }
}

Status UnionNode::ConsumeNextOrdered(ExecState* exec_state, const RowBatch& rb,
                                     size_t parent_index) {
  parent_row_batches_[parent_index].push_back(rb);
  CacheNextRowBatch(parent_index);
  PX_RETURN_IF_ERROR(MergeData(exec_state));
  return OptionallyFlushRowBatchIfTimeout(exec_state);
}

Status UnionNode::ConsumeNextUnordered(ExecState* exec_state, const RowBatch& rb,
                                       size_t parent_index) {
  if (rb.eos()) {
    flushed_parent_eoses_[parent_index] = true;
  }
  RowBatch output_rb(*output_descriptor_, rb.num_rows());
  for (size_t i = 0; i < output_descriptor_->size(); ++i) {
    PX_RETURN_IF_ERROR(output_rb.AddColumn(GetInputColumn(rb, parent_index, i)));
  }

  output_rb.set_eow(InputsComplete());
  output_rb.set_eos(InputsComplete());
  PX_RETURN_IF_ERROR(SendRowBatchToChildren(exec_state, output_rb));
  return Status::OK();
}

Status UnionNode::ConsumeNextImpl(ExecState* exec_state, const RowBatch& rb, size_t parent_index) {
  if (plan_node_->order_by_time()) {
    return ConsumeNextOrdered(exec_state, rb, parent_index);
  }
  return ConsumeNextUnordered(exec_state, rb, parent_index);
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
