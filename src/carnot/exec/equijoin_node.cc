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

#include "src/carnot/exec/equijoin_node.h"

#include <arrow/array.h>
#include <arrow/memory_pool.h>
#include <arrow/status.h>
#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include <absl/strings/str_join.h>
#include <absl/strings/substitute.h>

#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/udf/udf_wrapper.h"
#include "src/common/base/base.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/type_utils.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;

std::string EquijoinNode::DebugStringImpl() {
  return absl::Substitute("Exec::JoinNode<$0>", absl::StrJoin(plan_node_->column_names(), ","));
}

bool EquijoinNode::IsProbeTable(size_t parent_index) {
  return (parent_index == 0) == (probe_table_ == EquijoinNode::JoinInputTable::kLeftTable);
}

Status EquijoinNode::InitImpl(const plan::Operator& plan_node) {
  CHECK(plan_node.op_type() == planpb::OperatorType::JOIN_OPERATOR);
  if (input_descriptors_.size() != 2) {
    return error::InvalidArgument("Join operator expects a two input relations, got $0",
                                  input_descriptors_.size());
  }
  const auto* join_plan_node = static_cast<const plan::JoinOperator*>(&plan_node);
  plan_node_ = std::make_unique<plan::JoinOperator>(*join_plan_node);
  output_rows_per_batch_ =
      plan_node_->rows_per_batch() == 0 ? kDefaultJoinRowBatchSize : plan_node_->rows_per_batch();

  if (plan_node_->order_by_time() && plan_node_->time_column().parent_index() == 0) {
    // Make the probe table the left table when we need to preserve the order of the left table in
    // the output.
    probe_table_ = EquijoinNode::JoinInputTable::kLeftTable;
  } else {
    probe_table_ = EquijoinNode::JoinInputTable::kRightTable;
  }

  switch (plan_node_->type()) {
    case planpb::JoinOperator::INNER:
      build_spec_.emit_unmatched_rows = false;
      probe_spec_.emit_unmatched_rows = false;
      break;
    case planpb::JoinOperator::LEFT_OUTER:
      build_spec_.emit_unmatched_rows = probe_table_ != EquijoinNode::JoinInputTable::kLeftTable;
      probe_spec_.emit_unmatched_rows = probe_table_ == EquijoinNode::JoinInputTable::kLeftTable;
      break;
    case planpb::JoinOperator::FULL_OUTER:
      build_spec_.emit_unmatched_rows = true;
      probe_spec_.emit_unmatched_rows = true;
      break;
    default:
      return error::Internal(absl::Substitute("EquijoinNode: Unknown Join Type $0",
                                              static_cast<int>(plan_node_->type())));
  }

  for (const auto& eq_condition : plan_node_->equality_conditions()) {
    int64_t left_index = eq_condition.left_column_index();
    int64_t right_index = eq_condition.right_column_index();

    CHECK_EQ(input_descriptors_[0].type(left_index), input_descriptors_[1].type(right_index));
    key_data_types_.emplace_back(input_descriptors_[0].type(left_index));

    build_spec_.key_indices.emplace_back(
        probe_table_ == EquijoinNode::JoinInputTable::kLeftTable ? right_index : left_index);
    probe_spec_.key_indices.emplace_back(
        probe_table_ == EquijoinNode::JoinInputTable::kLeftTable ? left_index : right_index);
  }

  const auto& output_cols = plan_node_->output_columns();
  for (size_t i = 0; i < output_cols.size(); ++i) {
    auto parent_index = output_cols[i].parent_index();
    auto input_column_index = output_cols[i].column_index();
    auto dt = input_descriptors_[parent_index].type(input_column_index);

    TableSpec& selected_spec = IsProbeTable(parent_index) ? probe_spec_ : build_spec_;
    selected_spec.input_col_indices.emplace_back(input_column_index);
    selected_spec.input_col_types.emplace_back(dt);
    selected_spec.output_col_indices.emplace_back(i);
  }

  return Status::OK();
}

Status EquijoinNode::InitializeColumnBuilders() {
  for (size_t i = 0; i < output_descriptor_->size(); ++i) {
    column_builders_[i] =
        MakeArrowBuilder(output_descriptor_->type(i), arrow::default_memory_pool());
    PX_RETURN_IF_ERROR(column_builders_[i]->Reserve(output_rows_per_batch_));
  }
  return Status::OK();
}

Status EquijoinNode::PrepareImpl(ExecState* /*exec_state*/) {
  column_builders_.resize(output_descriptor_->size());
  PX_RETURN_IF_ERROR(InitializeColumnBuilders());

  return Status::OK();
}

Status EquijoinNode::OpenImpl(ExecState* /*exec_state*/) { return Status::OK(); }

Status EquijoinNode::CloseImpl(ExecState* /*exec_state*/) {
  join_keys_chunk_.clear();
  build_buffer_.clear();
  probed_keys_.clear();
  key_values_pool_.Clear();
  return Status::OK();
}

template <types::DataType DT>
void ExtractIntoRowTuples(std::vector<RowTuple*>* row_tuples, arrow::Array* input_col,
                          int rt_col_idx) {
  auto num_rows = input_col->length();
  for (auto row_idx = 0; row_idx < num_rows; ++row_idx) {
    ExtractIntoRowTuple<DT>((*row_tuples)[row_idx], input_col, rt_col_idx, row_idx);
  }
}

Status EquijoinNode::ExtractJoinKeysForBatch(const table_store::schema::RowBatch& rb,
                                             bool is_probe) {
  // Reset the row tuples
  for (auto& rt : join_keys_chunk_) {
    if (rt == nullptr) {
      rt = key_values_pool_.Add(new RowTuple(&key_data_types_));
    } else {
      rt->Reset();
    }
  }

  // Grow the join_keys_chunk_ to be the size of the RowBatch.
  size_t num_rows = rb.num_rows();
  if (join_keys_chunk_.size() < num_rows) {
    int prev_size = join_keys_chunk_.size();
    join_keys_chunk_.reserve(num_rows);
    for (size_t idx = prev_size; idx < num_rows; ++idx) {
      auto tuple_ptr = key_values_pool_.Add(new RowTuple(&key_data_types_));
      join_keys_chunk_.emplace_back(tuple_ptr);
    }
  }

  const TableSpec& spec = is_probe ? probe_spec_ : build_spec_;

  // Scan through all the group args in column order and extract the entire column.
  for (size_t tuple_col_idx = 0; tuple_col_idx < spec.key_indices.size(); ++tuple_col_idx) {
    auto input_col_idx = spec.key_indices[tuple_col_idx];
    auto dt = key_data_types_[tuple_col_idx];
    auto col = rb.ColumnAt(input_col_idx).get();

#define TYPE_CASE(_dt_) ExtractIntoRowTuples<_dt_>(&join_keys_chunk_, col, tuple_col_idx);
    PX_SWITCH_FOREACH_DATATYPE(dt, TYPE_CASE);
#undef TYPE_CASE
  }

  return Status::OK();
}

std::vector<types::SharedColumnWrapper>* CreateWrapper(ObjectPool* pool,
                                                       const std::vector<types::DataType>& types) {
  auto ptr = pool->Add(new std::vector<types::SharedColumnWrapper>(types.size()));
  for (size_t col_idx = 0; col_idx < types.size(); ++col_idx) {
    (*ptr)[col_idx] = types::ColumnWrapper::Make(types[col_idx], 0);
  }
  return ptr;
}

Status EquijoinNode::HashRowBatch(const table_store::schema::RowBatch& rb) {
  if (rb.num_rows() > static_cast<int64_t>(build_wrappers_chunk_.size())) {
    build_wrappers_chunk_.resize(rb.num_rows());
  }
  for (auto row_idx = 0; row_idx < rb.num_rows(); ++row_idx) {
    if (build_wrappers_chunk_[row_idx] == nullptr) {
      build_wrappers_chunk_[row_idx] =
          CreateWrapper(&column_values_pool_, build_spec_.input_col_types);
    }
  }

  // Make sure the map has constructed the necessary column wrappers for all of the tuples.
  for (auto row_idx = 0; row_idx < rb.num_rows(); ++row_idx) {
    auto& rt = join_keys_chunk_[row_idx];
    auto& current = build_buffer_[rt];
    auto wrappers_ptr = current != nullptr ? current : build_wrappers_chunk_[row_idx];

    // Now extract the values into the corresponding column wrappers.
    for (size_t i = 0; i < build_spec_.input_col_indices.size(); ++i) {
      const auto& rb_col_idx = build_spec_.input_col_indices[i];
      auto arr = rb.ColumnAt(rb_col_idx).get();
      const auto& dt = build_spec_.input_col_types[i];

#define TYPE_CASE(_dt_) \
  types::ExtractValueToColumnWrapper<_dt_>(wrappers_ptr->at(i).get(), arr, row_idx);
      PX_SWITCH_FOREACH_DATATYPE(dt, TYPE_CASE);
#undef TYPE_CASE
    }
    // Keep track of the number of rows that the build buffer matches for each key.
    build_buffer_rows_[rt]++;

    if (current == nullptr) {
      std::swap(build_wrappers_chunk_[row_idx], current);
      // Reset the new tuples that we added
      join_keys_chunk_[row_idx] = nullptr;
    }
  }

  return Status::OK();
}

template <types::DataType DT>
Status AppendValuesFromWrapper(arrow::ArrayBuilder* output_builder,
                               types::SharedColumnWrapper input_wrapper, size_t start_idx,
                               size_t num_rows) {
  using ValueType = typename types::DataTypeTraits<DT>::value_type;
  for (size_t row_idx = start_idx; row_idx < start_idx + num_rows; ++row_idx) {
    PX_RETURN_IF_ERROR(table_store::schema::CopyValue<DT>(
        output_builder, udf::UnWrap(input_wrapper->Get<ValueType>(row_idx))));
  }
  return Status::OK();
}

template <types::DataType DT>
Status AppendColumnDefaultValue(arrow::ArrayBuilder* output_builder, size_t num_times) {
  using ValueType = typename types::DataTypeTraits<DT>::value_type;
  ValueType zeroval;
  for (size_t i = 0; i < num_times; ++i) {
    PX_RETURN_IF_ERROR(table_store::schema::CopyValue<DT>(output_builder, udf::UnWrap(zeroval)));
  }
  return Status::OK();
}

// Create a new output row batch from the column builders, and flush the pending row batch.
// We hold on to a pending row batch because it is difficult to know a priori whether a given
// output batch will be eos/eow.
Status EquijoinNode::NextOutputBatch(ExecState* exec_state) {
  // We will set eos/eow markers later, once we are sure whether this is the final batch.
  PX_ASSIGN_OR_RETURN(auto output_batch, RowBatch::FromColumnBuilders(*output_descriptor_, false,
                                                                      false, &column_builders_));
  if (pending_output_batch_ != nullptr) {
    PX_RETURN_IF_ERROR(SendRowBatchToChildren(exec_state, *pending_output_batch_));
  }
  pending_output_batch_.swap(output_batch);

  return InitializeColumnBuilders();
}

Status EquijoinNode::FlushChunkedRows(ExecState* exec_state) {
  for (size_t col = 0; col < build_spec_.output_col_indices.size(); ++col) {
    for (const auto& chunk : chunks_) {
      auto output_idx = build_spec_.output_col_indices[col];
      auto builder = column_builders_.at(output_idx).get();

      if (chunk.wrappers_ptr == nullptr) {
#define TYPE_CASE(_dt_) PX_RETURN_IF_ERROR(AppendColumnDefaultValue<_dt_>(builder, chunk.num_rows))
        PX_SWITCH_FOREACH_DATATYPE(output_descriptor_->type(output_idx), TYPE_CASE);
#undef TYPE_CASE
      } else {
#define TYPE_CASE(_dt_)                                                                  \
  PX_RETURN_IF_ERROR(AppendValuesFromWrapper<_dt_>(builder, chunk.wrappers_ptr->at(col), \
                                                   chunk.bb_row_idx, chunk.num_rows))
        PX_SWITCH_FOREACH_DATATYPE(output_descriptor_->type(output_idx), TYPE_CASE);
#undef TYPE_CASE
      }
    }
  }

  for (size_t col = 0; col < probe_spec_.output_col_indices.size(); ++col) {
    for (const auto& chunk : chunks_) {
      auto output_idx = probe_spec_.output_col_indices[col];
      auto builder = column_builders_.at(output_idx).get();

      if (chunk.rb == nullptr) {
#define TYPE_CASE(_dt_) PX_RETURN_IF_ERROR(AppendColumnDefaultValue<_dt_>(builder, chunk.num_rows))
        PX_SWITCH_FOREACH_DATATYPE(output_descriptor_->type(output_idx), TYPE_CASE);
#undef TYPE_CASE
      } else {
        auto src_idx = probe_spec_.input_col_indices[col];
        auto builder = column_builders_[output_idx].get();
        auto input_col = chunk.rb->ColumnAt(src_idx).get();

#define TYPE_CASE(_dt_)                                                             \
  PX_RETURN_IF_ERROR(table_store::schema::CopyValueRepeated<_dt_>(                  \
      builder, types::GetValueFromArrowArray<_dt_>(input_col, chunk.probe_row_idx), \
      chunk.num_rows))
        PX_SWITCH_FOREACH_DATATYPE(output_descriptor_->type(output_idx), TYPE_CASE);
#undef TYPE_CASE
      }
    }
  }
  std::vector<EquijoinNode::OutputChunk> new_chunks(0);
  std::swap(chunks_, new_chunks);
  queued_rows_ = 0;
  return NextOutputBatch(exec_state);
}

Status EquijoinNode::MatchBuildValuesAndFlush(ExecState* exec_state,
                                              std::vector<types::SharedColumnWrapper>* wrapper,
                                              std::shared_ptr<RowBatch> probe_rb,
                                              int64_t probe_rb_row, int64_t matching_bb_rows) {
  int64_t bb_rows_left = matching_bb_rows;

  while (bb_rows_left > 0) {
    auto available = output_rows_per_batch_ - (column_builders_[0]->length() + queued_rows_);
    auto chunk_rows = std::min(bb_rows_left, available);
    OutputChunk c{probe_rb, wrapper, chunk_rows, matching_bb_rows - bb_rows_left, probe_rb_row};
    chunks_.emplace_back(c);
    queued_rows_ += chunk_rows;
    bb_rows_left -= chunk_rows;

    if (queued_rows_ == output_rows_per_batch_) {
      PX_RETURN_IF_ERROR(FlushChunkedRows(exec_state));
    }
  }

  return Status::OK();
}

Status EquijoinNode::DoProbe(ExecState* exec_state, const table_store::schema::RowBatch& rb) {
  if (rb.eos()) {
    probe_eos_ = true;
  }

  PX_RETURN_IF_ERROR(ExtractJoinKeysForBatch(rb, true));

  if (rb.num_rows() > static_cast<int64_t>(probe_wrappers_chunk_.size())) {
    probe_wrappers_chunk_.resize(rb.num_rows());
  }

  for (auto row_idx = 0; row_idx < rb.num_rows(); ++row_idx) {
    auto it = build_buffer_.find(join_keys_chunk_[row_idx]);
    if (it != build_buffer_.end()) {
      probe_wrappers_chunk_[row_idx] = it->second;
      probed_keys_.insert(it->first);
    } else {
      probe_wrappers_chunk_[row_idx] = nullptr;
    }
  }

  auto rb_ptr = std::make_shared<RowBatch>(rb);

  for (auto row_idx = 0; row_idx < rb.num_rows(); ++row_idx) {
    if (queued_rows_ >= output_rows_per_batch_ - column_builders_[0]->length()) {
      PX_RETURN_IF_ERROR(FlushChunkedRows(exec_state));
    }

    if (probe_wrappers_chunk_[row_idx] == nullptr) {
      if (probe_spec_.emit_unmatched_rows) {
        OutputChunk c{rb_ptr, nullptr, 1, 0, row_idx};
        chunks_.emplace_back(c);
        queued_rows_ += 1;
      }
      continue;
    }

    PX_RETURN_IF_ERROR(MatchBuildValuesAndFlush(exec_state, probe_wrappers_chunk_[row_idx], rb_ptr,
                                                row_idx,
                                                build_buffer_rows_[join_keys_chunk_[row_idx]]));
  }

  if (probe_eos_ && queued_rows_ > 0) {
    PX_RETURN_IF_ERROR(FlushChunkedRows(exec_state));
  }

  return Status::OK();
}

Status EquijoinNode::EmitUnmatchedBuildRows(ExecState* exec_state) {
  for (auto it = build_buffer_.begin(); it != build_buffer_.end(); ++it) {
    if (probed_keys_.find(it->first) != probed_keys_.end()) {
      continue;
    }
    PX_RETURN_IF_ERROR(MatchBuildValuesAndFlush(exec_state, it->second, nullptr, 0,
                                                build_buffer_rows_[it->first]));
  }

  if (queued_rows_ > 0) {
    PX_RETURN_IF_ERROR(FlushChunkedRows(exec_state));
  }
  return Status::OK();
}

Status EquijoinNode::ConsumeBuildBatch(ExecState* exec_state,
                                       const table_store::schema::RowBatch& rb) {
  if (rb.eos()) {
    build_eos_ = true;
  }

  PX_RETURN_IF_ERROR(ExtractJoinKeysForBatch(rb, false));
  PX_RETURN_IF_ERROR(HashRowBatch(rb));

  if (build_eos_) {
    while (probe_batches_.size()) {
      PX_RETURN_IF_ERROR(DoProbe(exec_state, probe_batches_.front()));
      probe_batches_.pop();
    }
  }
  return Status::OK();
}

Status EquijoinNode::ConsumeProbeBatch(ExecState* exec_state,
                                       const table_store::schema::RowBatch& rb) {
  if (!build_eos_) {
    probe_batches_.push(rb);
    return Status::OK();
  }
  return DoProbe(exec_state, rb);
}

Status EquijoinNode::ConsumeNextImpl(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                                     size_t parent_index) {
  if (IsProbeTable(parent_index)) {
    DCHECK(!probe_eos_);
    PX_RETURN_IF_ERROR(ConsumeProbeBatch(exec_state, rb));
  } else {
    DCHECK(!build_eos_);
    PX_RETURN_IF_ERROR(ConsumeBuildBatch(exec_state, rb));
  }

  if (build_eos_ && probe_eos_) {
    if (build_spec_.emit_unmatched_rows) {
      PX_RETURN_IF_ERROR(EmitUnmatchedBuildRows(exec_state));
    }

    if (column_builders_[0]->length()) {
      PX_RETURN_IF_ERROR(NextOutputBatch(exec_state));
    }

    // Now send the last row batch and we know it is EOS/EOW.
    if (pending_output_batch_ == nullptr) {
      // This should only happen when no join keys match up.
      PX_ASSIGN_OR_RETURN(
          pending_output_batch_,
          RowBatch::WithZeroRows(*output_descriptor_, /* eow */ true, /* eos */ true));
    } else {
      pending_output_batch_->set_eos(true);
      pending_output_batch_->set_eow(true);
    }
    PX_RETURN_IF_ERROR(SendRowBatchToChildren(exec_state, *pending_output_batch_));
  }

  return Status::OK();
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
