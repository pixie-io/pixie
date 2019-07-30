#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_join.h"
#include "absl/strings/substitute.h"
#include "src/carnot/exec/union_node.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/shared/types/arrow_adapter.h"

namespace pl {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;

std::string UnionNode::DebugStringImpl() {
  return absl::Substitute("Exec::UnionNode<$0>", absl::StrJoin(plan_node_->column_names(), ","));
}

Status UnionNode::InitImpl(const plan::Operator& plan_node, const RowDescriptor& output_descriptor,
                           const std::vector<RowDescriptor>& input_descriptors) {
  CHECK(plan_node.op_type() == planpb::OperatorType::UNION_OPERATOR);
  const auto* union_plan_node = static_cast<const plan::UnionOperator*>(&plan_node);
  plan_node_ = std::make_unique<plan::UnionOperator>(*union_plan_node);
  output_descriptor_ = std::make_unique<RowDescriptor>(output_descriptor);
  output_rows_per_batch_ =
      plan_node_->rows_per_batch() == 0 ? kDefaultUnionRowBatchSize : plan_node_->rows_per_batch();
  num_parents_ = input_descriptors.size();

  return Status::OK();
}

Status UnionNode::InitializeColumnBuilders() {
  for (size_t i = 0; i < output_descriptor_->size(); ++i) {
    column_builders_[i] =
        MakeArrowBuilder(output_descriptor_->type(i), arrow::default_memory_pool());
    PL_RETURN_IF_ERROR(column_builders_[i]->Reserve(output_rows_per_batch_));
  }
  return Status::OK();
}

Status UnionNode::PrepareImpl(ExecState*) {
  size_t num_output_cols = output_descriptor_->size();

  parent_eoses_.resize(num_parents_);

  if (plan_node_->order_by_time()) {
    // Set up parent stream state.
    parent_row_batches_.resize(num_parents_);
    started_streams_.resize(num_parents_);
    row_cursors_.resize(num_parents_);
    time_columns_.resize(num_parents_);
    data_columns_.resize(num_parents_, std::vector<arrow::Array*>(num_output_cols));

    column_builders_.resize(num_output_cols);
    PL_RETURN_IF_ERROR(InitializeColumnBuilders());
  }

  return Status::OK();
}

Status UnionNode::OpenImpl(ExecState*) { return Status::OK(); }

Status UnionNode::CloseImpl(ExecState*) { return Status::OK(); }

bool UnionNode::InputsComplete() {
  for (bool parent_eos : parent_eoses_) {
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

bool UnionNode::ReadyToMerge() {
  for (size_t parent = 0; parent < row_cursors_.size(); ++parent) {
    if (parent_eoses_[parent]) {
      continue;
    }
    // Return if we still need more data from one of the input streams.
    if (!started_streams_[parent] || !parent_row_batches_[parent].size()) {
      return false;
    }
  }
  return true;
}

types::Time64NSValue UnionNode::GetTimeAtParentCursor(size_t parent_index) const {
  DCHECK(!parent_eoses_[parent_index]);
  DCHECK(time_columns_[parent_index] != nullptr);
  return types::Time64NSValue(types::GetValueFromArrowArray<types::INT64>(
      time_columns_[parent_index], row_cursors_[parent_index]));
}

template <types::DataType T>
Status CopyValue(const arrow::Array* input_col, size_t row,
                 arrow::ArrayBuilder* output_col_builder) {
  auto* typed_col_builder =
      static_cast<typename types::DataTypeTraits<T>::arrow_builder_type*>(output_col_builder);
  auto res = types::GetValueFromArrowArray<T>(input_col, row);

  if constexpr (std::is_same_v<arrow::StringBuilder,
                               typename types::DataTypeTraits<T>::arrow_builder_type>) {
    int64_t size = res.size() + typed_col_builder->value_data_length();
    if (size >= typed_col_builder->value_data_capacity()) {
      PL_RETURN_IF_ERROR(typed_col_builder->ReserveData(std::lrint(1.5 * size)));
    }
  }

  typed_col_builder->UnsafeAppend(res);
  return Status::OK();
}

Status UnionNode::AppendRow(size_t parent) {
  for (size_t i = 0; i < output_descriptor_->size(); ++i) {
#define TYPE_CASE(_dt_) \
  PL_RETURN_IF_ERROR(   \
      CopyValue<_dt_>(data_columns_[parent][i], row_cursors_[parent], column_builders_[i].get()));
    PL_SWITCH_FOREACH_DATATYPE(output_descriptor_->type(i), TYPE_CASE);
#undef TYPE_CASE
  }
  return Status::OK();
}

Status UnionNode::FlushRowBatch(ExecState* exec_state) {
  bool eos = InputsComplete();
  int64_t output_rows = column_builders_[0]->length();
  if (output_rows < static_cast<int64_t>(output_rows_per_batch_) && !eos) {
    return Status::OK();
  }

  RowBatch output_rb(*output_descriptor_, output_rows);
  output_rb.set_eow(eos);
  output_rb.set_eos(eos);

  for (auto& column_builder : column_builders_) {
    std::shared_ptr<arrow::Array> output_array;
    PL_RETURN_IF_ERROR(column_builder->Finish(&output_array));
    PL_RETURN_IF_ERROR(output_rb.AddColumn(output_array));
  }

  PL_RETURN_IF_ERROR(InitializeColumnBuilders());
  return SendRowBatchToChildren(exec_state, output_rb);
}

Status UnionNode::MergeData(ExecState* exec_state) {
  while (ReadyToMerge() && !InputsComplete()) {
    // Get the smallest time out of all of the current streams.
    std::vector<size_t> parent_streams;
    parent_streams.reserve(row_cursors_.size());

    for (size_t parent = 0; parent < row_cursors_.size(); ++parent) {
      if (!parent_eoses_[parent]) {
        parent_streams.push_back(parent);
      }
    }

    std::sort(parent_streams.begin(), parent_streams.end(),
              [this](size_t parent_a, size_t parent_b) {
                return GetTimeAtParentCursor(parent_a) < GetTimeAtParentCursor(parent_b);
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
      PL_RETURN_IF_ERROR(AppendRow(parent));

      const auto& rb = parent_row_batches_[parent][0];
      if (++row_cursors_[parent] == static_cast<size_t>(rb.num_rows())) {
        // Delete the row batch from our buffer and update the cursor.
        parent_row_batches_[parent].erase(parent_row_batches_[parent].begin());
        row_cursors_[parent] = 0;
        if (rb.eos()) {
          parent_eoses_[parent] = true;
        }
        CacheNextRowBatch(parent);
      }

      PL_RETURN_IF_ERROR(FlushRowBatch(exec_state));
    }
  }
  return Status::OK();
}

void UnionNode::CacheNextRowBatch(size_t parent) {
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
  if (!started_streams_[parent_index]) {
    started_streams_[parent_index] = true;
  }
  parent_row_batches_[parent_index].push_back(rb);
  CacheNextRowBatch(parent_index);
  return MergeData(exec_state);
}

Status UnionNode::ConsumeNextUnordered(ExecState* exec_state, const RowBatch& rb,
                                       size_t parent_index) {
  if (rb.eos()) {
    parent_eoses_[parent_index] = true;
  }
  RowBatch output_rb(*output_descriptor_, rb.num_rows());
  for (size_t i = 0; i < output_descriptor_->size(); ++i) {
    PL_RETURN_IF_ERROR(output_rb.AddColumn(GetInputColumn(rb, parent_index, i)));
  }

  output_rb.set_eow(InputsComplete());
  output_rb.set_eos(InputsComplete());
  PL_RETURN_IF_ERROR(SendRowBatchToChildren(exec_state, output_rb));
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
}  // namespace pl
