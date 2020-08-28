#include "src/carnot/exec/grpc_sink_node.h"

#include <string>
#include <vector>

#include <absl/strings/substitute.h>

#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/macros.h"
#include "src/common/uuid/uuid_utils.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;

std::string GRPCSinkNode::DebugStringImpl() {
  std::string destination;
  if (plan_node_->has_table_name()) {
    destination = absl::Substitute("table_name: $0", plan_node_->table_name());
  } else if (plan_node_->has_grpc_source_id()) {
    destination = absl::Substitute("source_id: $0", plan_node_->grpc_source_id());
  }
  return absl::Substitute("Exec::GRPCSinkNode: {address: $0, $1, output: $2}",
                          plan_node_->address(), destination, input_descriptor_->DebugString());
}

Status GRPCSinkNode::InitImpl(const plan::Operator& plan_node) {
  CHECK(plan_node.op_type() == planpb::OperatorType::GRPC_SINK_OPERATOR);
  if (input_descriptors_.size() != 1) {
    return error::InvalidArgument("GRPCSink operator expects a single input relation, got $0",
                                  input_descriptors_.size());
  }
  input_descriptor_ = std::make_unique<RowDescriptor>(input_descriptors_[0]);
  const auto* sink_plan_node = static_cast<const plan::GRPCSinkOperator*>(&plan_node);
  plan_node_ = std::make_unique<plan::GRPCSinkOperator>(*sink_plan_node);
  return Status::OK();
}

Status GRPCSinkNode::PrepareImpl(ExecState*) { return Status::OK(); }

Status GRPCSinkNode::OpenImpl(ExecState* exec_state) {
  stub_ = exec_state->ResultSinkServiceStub(plan_node_->address(), plan_node_->ssl_targetname());
  // When we are sending the results to an external service, such as the query broker,
  // add authentication to the client context.
  if (plan_node_->has_table_name()) {
    // Adding auth to GRPC client.
    exec_state->AddAuthToGRPCClientContext(&context_);
  }
  return Status::OK();
}

Status GRPCSinkNode::CloseWriter() {
  if (writer_ == nullptr) {
    return Status::OK();
  }
  writer_->WritesDone();
  auto s = writer_->Finish();
  if (!s.ok()) {
    LOG(ERROR) << absl::Substitute("GRPCSink node: Error calling Finish on stream, message: $0",
                                   s.error_message());
  }
  return Status::OK();
}

Status GRPCSinkNode::CloseImpl(ExecState*) {
  if (sent_eos_) {
    return Status::OK();
  }

  if (writer_ != nullptr) {
    PL_RETURN_IF_ERROR(CloseWriter());
    return error::Internal("Closing GRPCSinkNode without receiving EOS.");
  }

  return Status::OK();
}

Status GRPCSinkNode::ConsumeNextImpl(ExecState* exec_state, const RowBatch& rb, size_t) {
  if (writer_ == nullptr) {
    writer_ = stub_->TransferResultChunk(&context_, &response_);
  }

  carnotpb::TransferResultChunkRequest req;
  // Set the metadata for the RowBatch (where it should go).
  req.set_address(plan_node_->address());

  if (plan_node_->has_grpc_source_id()) {
    req.mutable_row_batch_result()->set_grpc_source_id(plan_node_->grpc_source_id());
  } else if (plan_node_->has_table_name()) {
    req.mutable_row_batch_result()->set_table_name(plan_node_->table_name());
  } else {
    return error::Internal("GRPCSink has neither source ID nor table name set.");
  }

  ToProto(exec_state->query_id(), req.mutable_query_id());
  // Serialize the RowBatch.
  PL_RETURN_IF_ERROR(rb.ToProto(req.mutable_row_batch_result()->mutable_row_batch()));

  if (!writer_->Write(req)) {
    return error::Cancelled(
        "GRPCSink could not write result to address: $0, stream closed by server",
        plan_node_->address());
  }

  if (!rb.eos()) {
    return Status::OK();
  }

  sent_eos_ = true;
  PL_RETURN_IF_ERROR(CloseWriter());

  return response_.success()
             ? Status::OK()
             : error::Internal(absl::Substitute(
                   "GRPCSinkNode: error sending stream to address $0, error message: $1",
                   plan_node_->address(), response_.message()));
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
