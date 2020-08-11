#include "src/carnot/exec/grpc_source_node.h"

#include <string>
#include <utility>
#include <vector>

#include <absl/strings/substitute.h>

#include "src/carnot/planpb/plan.pb.h"

namespace pl {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;

std::string GRPCSourceNode::DebugStringImpl() {
  return absl::Substitute("Exec::GRPCSourceNode: <id: $0, output: $1>", plan_node_->id(),
                          output_descriptor_->DebugString());
}

Status GRPCSourceNode::InitImpl(const plan::Operator& plan_node) {
  CHECK(plan_node.op_type() == planpb::OperatorType::GRPC_SOURCE_OPERATOR);
  const auto* source_plan_node = static_cast<const plan::GRPCSourceOperator*>(&plan_node);
  plan_node_ = std::make_unique<plan::GRPCSourceOperator>(*source_plan_node);
  return Status::OK();
}
Status GRPCSourceNode::PrepareImpl(ExecState*) { return Status::OK(); }

Status GRPCSourceNode::OpenImpl(ExecState*) { return Status::OK(); }

Status GRPCSourceNode::CloseImpl(ExecState*) { return Status::OK(); }

Status GRPCSourceNode::GenerateNextImpl(ExecState* exec_state) {
  PL_RETURN_IF_ERROR(PopRowBatch());
  PL_RETURN_IF_ERROR(SendRowBatchToChildren(exec_state, *rb_));
  return Status::OK();
}

Status GRPCSourceNode::EnqueueRowBatch(
    std::unique_ptr<carnotpb::TransferResultChunkRequest> row_batch) {
  if (!row_batch_queue_.enqueue(std::move(row_batch))) {
    return error::Internal("Failed to enqueue RowBatch");
  }
  return Status::OK();
}

Status GRPCSourceNode::PopRowBatch() {
  DCHECK(NextBatchReady());
  std::unique_ptr<carnotpb::TransferResultChunkRequest> rb_request;
  bool got_one = row_batch_queue_.try_dequeue(rb_request);
  if (!got_one) {
    return error::Internal(
        "Called GRPCSourceNode::OptionallyPopRowBatch but there was no available row batch in the "
        "queue.");
  }
  if (!rb_request->has_row_batch_result()) {
    return error::Internal(
        "GRPCSourceNode::PopRowBatch expected TransferResultChunkRequest to have RowBatch "
        "message.");
  }

  PL_ASSIGN_OR_RETURN(rb_, RowBatch::FromProto(rb_request->row_batch_result().row_batch()));
  return Status::OK();
}

bool GRPCSourceNode::NextBatchReady() {
  return HasBatchesRemaining() && row_batch_queue_.size_approx() > 0;
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
