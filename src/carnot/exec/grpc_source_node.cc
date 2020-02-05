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
  PL_RETURN_IF_ERROR(OptionallyPopRowBatch());
  PL_RETURN_IF_ERROR(SendRowBatchToChildren(exec_state, *next_up_));
  next_up_ = nullptr;
  return Status::OK();
}

Status GRPCSourceNode::EnqueueRowBatch(std::unique_ptr<carnotpb::RowBatchRequest> row_batch) {
  if (!row_batch_queue_.enqueue(std::move(row_batch))) {
    return error::Internal("Failed to enqueue RowBatch");
  }
  return Status::OK();
}

Status GRPCSourceNode::OptionallyPopRowBatch() {
  if (next_up_ != nullptr) {
    return Status::OK();
  }
  std::unique_ptr<carnotpb::RowBatchRequest> rb_request;
  bool got_one = row_batch_queue_.wait_dequeue_timed(rb_request, grpc_timeout_us_.count());
  if (!got_one) {
    PL_ASSIGN_OR_RETURN(next_up_, RowBatch::WithZeroRows(*output_descriptor_, true, true));
  } else {
    PL_ASSIGN_OR_RETURN(next_up_, RowBatch::FromProto(rb_request->row_batch()));
  }
  return Status::OK();
}

bool GRPCSourceNode::NextBatchReady() {
  return HasBatchesRemaining();
  // TODO(nserrino PL-1318): Uncomment this and replace above once the grpc router can poke the exec
  // graph. return next_up_ != nullptr || row_batch_queue_.size_approx() > 0;
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
