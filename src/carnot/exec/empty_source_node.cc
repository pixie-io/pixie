#include "src/carnot/exec/empty_source_node.h"

#include <limits>
#include <string>
#include <vector>

#include <absl/strings/substitute.h>

#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace exec {
using table_store::schema::RowBatch;

std::string EmptySourceNode::DebugStringImpl() {
  return absl::Substitute("Exec::EmptySourceNode: <output: $0>", output_descriptor_->DebugString());
}

Status EmptySourceNode::InitImpl(const plan::Operator& plan_node) {
  CHECK(plan_node.op_type() == planpb::OperatorType::EMPTY_SOURCE_OPERATOR);
  const auto* source_plan_node = static_cast<const plan::EmptySourceOperator*>(&plan_node);
  // copy the plan node to local object;
  plan_node_ = std::make_unique<plan::EmptySourceOperator>(*source_plan_node);

  return Status::OK();
}

Status EmptySourceNode::PrepareImpl(ExecState*) { return Status::OK(); }

Status EmptySourceNode::OpenImpl(ExecState*) { return Status::OK(); }

Status EmptySourceNode::CloseImpl(ExecState*) { return Status::OK(); }

Status EmptySourceNode::GenerateNextImpl(ExecState* exec_state) {
  PL_ASSIGN_OR_RETURN(auto row_batch,
                      RowBatch::WithZeroRows(*output_descriptor_, /* eow */ true, /* eos */ true));
  PL_RETURN_IF_ERROR(SendRowBatchToChildren(exec_state, *row_batch));
  return Status::OK();
}

bool EmptySourceNode::NextBatchReady() {
  // NextBatchReady only when the batch is nonempty.
  return !sent_eos_;
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
