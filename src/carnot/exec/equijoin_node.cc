#include <algorithm>
#include <string>
#include <unordered_set>

#include "src/carnot/exec/equijoin_node.h"

namespace pl {
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

Status EquijoinNode::InitImpl(
    const plan::Operator& plan_node, const table_store::schema::RowDescriptor& output_descriptor,
    const std::vector<table_store::schema::RowDescriptor>& input_descriptors) {
  CHECK(plan_node.op_type() == planpb::OperatorType::JOIN_OPERATOR);
  if (input_descriptors.size() != 2) {
    return error::InvalidArgument("Join operator expects a two input relations, got $0",
                                  input_descriptors.size());
  }
  const auto* join_plan_node = static_cast<const plan::JoinOperator*>(&plan_node);
  plan_node_ = std::make_unique<plan::JoinOperator>(*join_plan_node);
  output_descriptor_ = std::make_unique<RowDescriptor>(output_descriptor);
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

    CHECK_EQ(input_descriptors[0].type(left_index), input_descriptors[1].type(right_index));
    key_data_types_.emplace_back(input_descriptors[0].type(left_index));

    build_spec_.key_indices.emplace_back(
        probe_table_ == EquijoinNode::JoinInputTable::kLeftTable ? right_index : left_index);
    probe_spec_.key_indices.emplace_back(
        probe_table_ == EquijoinNode::JoinInputTable::kLeftTable ? left_index : right_index);
  }

  const auto& output_cols = plan_node_->output_columns();
  for (size_t i = 0; i < output_cols.size(); ++i) {
    auto parent_index = output_cols[i].parent_index();
    auto input_column_index = output_cols[i].column_index();
    auto dt = input_descriptors[parent_index].type(input_column_index);

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
    PL_RETURN_IF_ERROR(column_builders_[i]->Reserve(output_rows_per_batch_));
  }
  return Status::OK();
}

Status EquijoinNode::PrepareImpl(ExecState* /*exec_state*/) {
  column_builders_.resize(output_descriptor_->size());
  PL_RETURN_IF_ERROR(InitializeColumnBuilders());

  return Status::OK();
}

Status EquijoinNode::OpenImpl(ExecState* /*exec_state*/) { return Status::OK(); }

Status EquijoinNode::CloseImpl(ExecState* /*exec_state*/) { return Status::OK(); }

Status EquijoinNode::ConsumeNextImpl(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                                     size_t parent_index) {
  PL_UNUSED(rb);
  PL_UNUSED(parent_index);
  PL_UNUSED(exec_state);
  return error::Internal("ConsumeNextImpl is not implemented yet");
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
