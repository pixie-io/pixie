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

StatusOr<carnotpb::TransferResultChunkRequest> RequestWithMetadata(
    plan::GRPCSinkOperator* plan_node, ExecState* exec_state) {
  carnotpb::TransferResultChunkRequest req;
  // Set the metadata for the RowBatch (where it should go).
  req.set_address(plan_node->address());

  if (plan_node->has_grpc_source_id()) {
    req.mutable_query_result()->set_grpc_source_id(plan_node->grpc_source_id());
  } else if (plan_node->has_table_name()) {
    req.mutable_query_result()->set_table_name(plan_node->table_name());
  } else {
    return error::Internal("GRPCSink has neither source ID nor table name set.");
  }

  ToProto(exec_state->query_id(), req.mutable_query_id());
  return req;
}

Status GRPCSinkNode::OptionallyCheckConnection(ExecState* exec_state) {
  if (sent_eos_) {
    return Status::OK();
  }

  auto time_now = std::chrono::system_clock::now();
  auto since_last_flush =
      std::chrono::duration_cast<std::chrono::milliseconds>(time_now - last_send_time_);
  bool recheck_connection = since_last_flush > connection_check_timeout_;
  if (!recheck_connection) {
    return Status::OK();
  }

  PL_ASSIGN_OR_RETURN(auto req, RequestWithMetadata(plan_node_.get(), exec_state));
  PL_ASSIGN_OR_RETURN(auto rb,
                      RowBatch::WithZeroRows(*input_descriptor_, /* eow */ false, /* eos */ false));
  PL_RETURN_IF_ERROR(rb->ToProto(req.mutable_query_result()->mutable_row_batch()));

  if (!writer_->Write(req)) {
    return error::Cancelled(
        "GRPCSinkNode $0 of query $1 could not write result to address: $2, stream closed by "
        "server",
        exec_state->query_id().str(), plan_node_->id(), plan_node_->address());
  }

  last_send_time_ = time_now;
  return Status::OK();
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

  writer_ = stub_->TransferResultChunk(&context_, &response_);

  PL_ASSIGN_OR_RETURN(auto initial_request, RequestWithMetadata(plan_node_.get(), exec_state));
  initial_request.mutable_query_result()->set_initiate_result_stream(true);

  if (!writer_->Write(initial_request)) {
    cancelled_ = true;
    return error::Cancelled(
        "GRPCSinkNode $0 error: unable to write stream initialization TransferResultChunkRequest "
        "to remote address $1 for query $2",
        plan_node_->id(), plan_node_->address(), exec_state->query_id().str());
  }

  last_send_time_ = std::chrono::system_clock::now();
  return Status::OK();
}

Status GRPCSinkNode::CloseWriter(ExecState* exec_state) {
  if (writer_ == nullptr) {
    return Status::OK();
  }
  writer_->WritesDone();
  auto s = writer_->Finish();
  if (!s.ok()) {
    LOG(ERROR) << absl::Substitute(
        "GRPCSinkNode $0 in query $1: Error calling Finish on stream, message: $2",
        plan_node_->id(), exec_state->query_id().str(), s.error_message());
  }
  return Status::OK();
}

Status GRPCSinkNode::CloseImpl(ExecState* exec_state) {
  if (sent_eos_) {
    return Status::OK();
  }

  if (writer_ != nullptr && !cancelled_) {
    LOG(INFO) << absl::Substitute("Closing GRPCSinkNode $0 in query $1 before receiving EOS",
                                  plan_node_->id(), exec_state->query_id().str());
    PL_RETURN_IF_ERROR(CloseWriter(exec_state));
  }

  return Status::OK();
}

Status GRPCSinkNode::ConsumeNextImpl(ExecState* exec_state, const RowBatch& rb, size_t) {
  PL_ASSIGN_OR_RETURN(auto req, RequestWithMetadata(plan_node_.get(), exec_state));

  // Serialize the RowBatch.
  PL_RETURN_IF_ERROR(rb.ToProto(req.mutable_query_result()->mutable_row_batch()));

  if (!writer_->Write(req)) {
    cancelled_ = true;
    return error::Cancelled(
        "GRPCSinkNode $0 of query $1 could not write result to address: $2, stream closed by "
        "server",
        exec_state->query_id().str(), plan_node_->id(), plan_node_->address());
  }
  last_send_time_ = std::chrono::system_clock::now();

  if (!rb.eos()) {
    return Status::OK();
  }

  PL_RETURN_IF_ERROR(CloseWriter(exec_state));

  return response_.success()
             ? Status::OK()
             : error::Internal(absl::Substitute(
                   "GRPCSinkNode $0 encountered error sending stream to address $1, message: $2",
                   plan_node_->id(), plan_node_->address(), response_.message()));
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
