#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <absl/strings/str_format.h>
#include <absl/strings/str_join.h>
#include "src/carnot/exec/grpc_source_node.h"
#include "src/carnot/planpb/plan.pb.h"
namespace pl {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;

std::string GRPCSourceNode::DebugStringImpl() {
  return absl::Substitute("Exec::GRPCSourceNode: <id: $0, output: $1>", plan_node_->id(),
                          output_descriptor_->DebugString());
}

Status GRPCSourceNode::InitImpl(const plan::Operator& plan_node,
                                const table_store::schema::RowDescriptor& output_descriptor,
                                const std::vector<table_store::schema::RowDescriptor>&) {
  CHECK(plan_node.op_type() == planpb::OperatorType::GRPC_SOURCE_OPERATOR);
  const auto* source_plan_node = static_cast<const plan::GRPCSourceOperator*>(&plan_node);
  plan_node_ = std::make_unique<plan::GRPCSourceOperator>(*source_plan_node);
  output_descriptor_ = std::make_unique<table_store::schema::RowDescriptor>(output_descriptor);
  return Status::OK();
}
Status GRPCSourceNode::PrepareImpl(ExecState*) { return Status::OK(); }

Status GRPCSourceNode::OpenImpl(ExecState*) { return Status::OK(); }

Status GRPCSourceNode::CloseImpl(ExecState*) { return Status::OK(); }

Status GRPCSourceNode::GenerateNextImpl(ExecState* exec_state) {
  if (sent_eos_) {
    return Status::OK();
  }
  if (!NextBatchReady()) {
    return Status::OK();
  }
  PL_RETURN_IF_ERROR(OptionallyPopRowBatch());

  PL_RETURN_IF_ERROR(SendRowBatchToChildren(exec_state, *next_up_));
  if (next_up_->eos()) {
    sent_eos_ = true;
  }
  next_up_ = nullptr;

  return Status::OK();
}

Status GRPCSourceNode::EnqueueRowBatch(std::unique_ptr<carnotpb::RowBatchRequest> row_batch) {
  if (!row_batch_queue_.enqueue(std::move(row_batch))) {
    return error::Internal("Failed to enqueue RowBatch");
  }
  return Status::OK();
}

bool GRPCSourceNode::HasBatchesRemaining() { return !sent_eos_; }

Status GRPCSourceNode::OptionallyPopRowBatch() {
  if (next_up_ != nullptr) {
    return Status::OK();
  }
  std::unique_ptr<carnotpb::RowBatchRequest> rb_request;
  bool got_one = row_batch_queue_.wait_dequeue_timed(rb_request, grpc_timeout_us_.count());
  if (!got_one) {
    next_up_ = std::make_unique<RowBatch>(*output_descriptor_, 0);
    next_up_->set_eow(true);
    next_up_->set_eos(true);
  } else {
    PL_ASSIGN_OR_RETURN(next_up_, RowBatch::FromProto(rb_request->row_batch()));
  }
  return Status::OK();
}

bool GRPCSourceNode::NextBatchReady() {
  return next_up_ != nullptr || row_batch_queue_.size_approx() > 0;
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
