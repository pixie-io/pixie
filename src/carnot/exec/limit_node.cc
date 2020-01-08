#include "src/carnot/exec/limit_node.h"

#include <arrow/array.h>
#include <string>
#include <vector>

#include <absl/strings/substitute.h>

#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"

namespace pl {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;

std::string LimitNode::DebugStringImpl() {
  return absl::Substitute("Exec::LimitNode<$0>", plan_node_->DebugString());
}

Status LimitNode::InitImpl(const plan::Operator& plan_node, const RowDescriptor& output_descriptor,
                           const std::vector<RowDescriptor>& input_descriptors) {
  CHECK(plan_node.op_type() == planpb::OperatorType::LIMIT_OPERATOR);
  const auto* limit_plan_node = static_cast<const plan::LimitOperator*>(&plan_node);
  // copy the plan node to local object;
  plan_node_ = std::make_unique<plan::LimitOperator>(*limit_plan_node);
  // NOTE: We expect output and input descriptors to match.
  DCHECK(output_descriptor == input_descriptors[0]);
  output_descriptor_ = std::make_unique<RowDescriptor>(output_descriptor);

  return Status::OK();
}
Status LimitNode::PrepareImpl(ExecState* /*exec_state*/) { return Status::OK(); }

Status LimitNode::OpenImpl(ExecState* /*exec_state*/) { return Status::OK(); }

Status LimitNode::CloseImpl(ExecState* /*exec_state*/) { return Status::OK(); }

Status LimitNode::ConsumeNextImpl(ExecState* exec_state, const RowBatch& rb, size_t) {
  size_t record_limit = plan_node_->record_limit();
  // Check if the entire row batch will fit.
  if (record_limit > (records_processed_ + rb.num_rows())) {
    // If so we just need to transfer it.
    records_processed_ += rb.num_rows();
    return SendRowBatchToChildren(exec_state, rb);
  }

  // We need to send over a slice of the input data.
  size_t remainder_records = record_limit - records_processed_;
  RowBatch output_rb(*output_descriptor_, remainder_records);
  size_t num_cols = rb.num_columns();
  for (size_t col_idx = 0; col_idx < num_cols; ++col_idx) {
    auto col = rb.ColumnAt(col_idx);
    PL_RETURN_IF_ERROR(output_rb.AddColumn(col->Slice(0, remainder_records)));
  }
  output_rb.set_eow(true);
  output_rb.set_eos(true);
  records_processed_ += remainder_records;

  // Terminate execution.
  exec_state->StopLimitReached();

  return SendRowBatchToChildren(exec_state, output_rb);
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
