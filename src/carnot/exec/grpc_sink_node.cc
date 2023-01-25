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

#include "src/carnot/exec/grpc_sink_node.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <absl/strings/substitute.h>

#include "src/carnot/carnotpb/carnot.pb.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/macros.h"
#include "src/common/uuid/uuid_utils.h"
#include "src/table_store/table_store.h"

namespace px {
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
  if (sent_eos_ || cancelled_) {
    return Status::OK();
  }

  auto time_now = std::chrono::system_clock::now();
  auto since_last_flush =
      std::chrono::duration_cast<std::chrono::milliseconds>(time_now - last_send_time_);
  bool recheck_connection = since_last_flush > connection_check_timeout_;
  if (!recheck_connection) {
    return Status::OK();
  }

  PX_ASSIGN_OR_RETURN(auto req, RequestWithMetadata(plan_node_.get(), exec_state));
  PX_ASSIGN_OR_RETURN(auto rb,
                      RowBatch::WithZeroRows(*input_descriptor_, /* eow */ false, /* eos */ false));
  PX_RETURN_IF_ERROR(rb->ToProto(req.mutable_query_result()->mutable_row_batch()));

  PX_RETURN_IF_ERROR(TryWriteRequest(exec_state, req));
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

Status GRPCSinkNode::StartConnection(ExecState* exec_state) {
  return StartConnectionWithRetries(exec_state, kGRPCRetries);
}

Status GRPCSinkNode::StartConnectionWithRetries(ExecState* exec_state, size_t n_retries) {
  if (n_retries == 0) {
    cancelled_ = true;
    return error::Cancelled(
        "GRPCSinkNode $0 error: unable to write TransferResultChunkRequest on stream start"
        "to remote address $1 for query $2",
        plan_node_->id(), plan_node_->address(), exec_state->query_id().str());
  }

  stub_ = exec_state->ResultSinkServiceStub(plan_node_->address(), plan_node_->ssl_targetname());

  context_ = std::make_unique<grpc::ClientContext>();
  // When we are sending the results to an external service, such as the query broker,
  // add authentication to the client context.
  if (plan_node_->has_table_name()) {
    // Adding auth to GRPC client.
    exec_state->AddAuthToGRPCClientContext(context_.get());
  }

  response_.Clear();
  writer_ = stub_->TransferResultChunk(context_.get(), &response_);

  PX_ASSIGN_OR_RETURN(auto req, RequestWithMetadata(plan_node_.get(), exec_state));
  // If this is not the first connection we've made then we send a 0-row rb instead of an
  // initiate_result_stream request.
  PX_ASSIGN_OR_RETURN(auto rb,
                      RowBatch::WithZeroRows(*input_descriptor_, /* eow */ false, /* eos */ false));
  PX_RETURN_IF_ERROR(rb->ToProto(req.mutable_query_result()->mutable_row_batch()));

  if (!writer_->Write(req)) {
    return StartConnectionWithRetries(exec_state, n_retries - 1);
  }

  last_send_time_ = std::chrono::system_clock::now();
  return Status::OK();
}

Status GRPCSinkNode::CancelledByServer(ExecState* exec_state) {
  cancelled_ = true;
  return error::Cancelled(
      "GRPCSinkNode $0 of query $1 could not write result to address: $2, stream closed by "
      "server",
      plan_node_->id(), exec_state->query_id().str(), plan_node_->address());
}

Status GRPCSinkNode::TryWriteRequest(ExecState* exec_state,
                                     const carnotpb::TransferResultChunkRequest& req) {
  if (writer_->Write(req)) {
    last_send_time_ = std::chrono::system_clock::now();
    return Status::OK();
  }

  // We need to determine if the server sent a response (i.e. server closed connection) or if the
  // connection just died.
  writer_->WritesDone();
  auto s = writer_->Finish();
  // If the Finish call was successful, then the server closed the connection and sent a response,
  // in which case we shouldn't try to reconnect. If there's an error from the server side
  // other than a RST_STREAM, we also shouldn't retry.
  if (s.ok() || s.error_code() != grpc::StatusCode::INTERNAL ||
      !absl::StrContains(s.error_message(), "RST_STREAM")) {
    return CancelledByServer(exec_state);
  }
  // Otherwise, the connection was probably cancelled due to a timeout or other transient failure,
  // so we can try to restart the connection.
  PX_RETURN_IF_ERROR(StartConnection(exec_state));

  // Try again to write the request on the new connection.
  if (!writer_->Write(req)) {
    return CancelledByServer(exec_state);
  }
  last_send_time_ = std::chrono::system_clock::now();
  return Status::OK();
}

Status GRPCSinkNode::OpenImpl(ExecState* exec_state) { return StartConnection(exec_state); }

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
  if (sent_eos_ || cancelled_) {
    return Status::OK();
  }

  if (writer_ != nullptr) {
    LOG(INFO) << absl::Substitute("Closing GRPCSinkNode $0 in query $1 before receiving EOS",
                                  plan_node_->id(), exec_state->query_id().str());
    PX_RETURN_IF_ERROR(CloseWriter(exec_state));
  }

  return Status::OK();
}

static inline bool GetRowSizes(const RowBatch& rb, std::vector<int64_t>* string_col_row_sizes,
                               int64_t* other_cols_row_size) {
  bool has_string_col = false;

  for (int col_idx = 0; col_idx < rb.num_columns(); ++col_idx) {
    auto col_type = rb.desc().type(col_idx);
    if (col_type != types::DataType::STRING) {
      *other_cols_row_size += types::ArrowTypeToBytes(types::ToArrowType(col_type));
    } else {
      has_string_col = true;
      for (int64_t row_idx = 0; row_idx < rb.num_rows(); ++row_idx) {
        (*string_col_row_sizes)[row_idx] +=
            sizeof(char) * std::static_pointer_cast<arrow::StringArray>(rb.ColumnAt(col_idx))
                               ->value_length(row_idx);
      }
    }
  }
  return has_string_col;
}

std::vector<int64_t> GRPCSinkNode::SplitBatchSizes(bool has_string_col,
                                                   const std::vector<int64_t>& string_col_row_sizes,
                                                   int64_t other_col_row_size) const {
  int64_t desired_batch_size_bytes = static_cast<int64_t>(max_batch_size_ * batch_size_factor_);
  std::vector<int64_t> new_batches_num_rows;
  if (has_string_col) {
    int64_t batch_bytes = 0;
    int64_t batch_num_rows = 0;
    for (const auto& [idx, row_string_bytes] : Enumerate(string_col_row_sizes)) {
      auto row_bytes = row_string_bytes + other_col_row_size;
      if (batch_num_rows > 0 && batch_bytes + row_bytes > desired_batch_size_bytes) {
        new_batches_num_rows.push_back(batch_num_rows);
        batch_bytes = 0;
        batch_num_rows = 0;
      }
      batch_bytes += row_bytes;
      batch_num_rows += 1;
    }
    new_batches_num_rows.push_back(batch_num_rows);
  } else {
    // If there are no string columns, we can split into evenly sized batches apart from the last
    // batch. The above logic would still work when there are no string columns, but this logic
    // avoids the O(num_rows) loop.
    auto total_rows = string_col_row_sizes.size();
    int64_t num_rows_per_batch = desired_batch_size_bytes / other_col_row_size;
    if (num_rows_per_batch == 0) {
      // If the row size is bigger than the desired batch size, then we just use 1 row per batch.
      num_rows_per_batch = 1;
    }
    int64_t num_batches = (total_rows / num_rows_per_batch);
    new_batches_num_rows.insert(new_batches_num_rows.end(), num_batches, num_rows_per_batch);

    auto leftover_rows = total_rows - (num_batches * num_rows_per_batch);
    if (leftover_rows > 0) {
      new_batches_num_rows.push_back(leftover_rows);
    }
  }
  return new_batches_num_rows;
}

Status GRPCSinkNode::SplitAndSendBatch(ExecState* exec_state, const RowBatch& rb,
                                       size_t parent_idx) {
  // Calculate the individual row sizes for all the string columns.
  std::vector<int64_t> string_col_row_sizes(rb.num_rows(), 0);
  // All other columns share the same size across all rows.
  int64_t other_cols_row_size = 0;
  auto has_string_col = GetRowSizes(rb, &string_col_row_sizes, &other_cols_row_size);

  std::vector<int64_t> new_batches_num_rows =
      SplitBatchSizes(has_string_col, string_col_row_sizes, other_cols_row_size);

  int64_t batch_idx = 0;
  // Run the first N - 1 batches because the last batch needs to set eos/eow to the original row
  // batches eos/eow.
  for (size_t idx = 0; idx < new_batches_num_rows.size() - 1; ++idx) {
    auto num_rows = new_batches_num_rows[idx];
    PX_ASSIGN_OR_RETURN(std::unique_ptr<RowBatch> output_rb, rb.Slice(batch_idx, num_rows));
    PX_RETURN_IF_ERROR(ConsumeNextImplNoSplit(exec_state, *output_rb, parent_idx));
    batch_idx += num_rows;
  }

  // Handle the final batch.
  PX_ASSIGN_OR_RETURN(std::unique_ptr<RowBatch> output_rb,
                      rb.Slice(batch_idx, rb.num_rows() - batch_idx));
  output_rb->set_eos(rb.eos());
  output_rb->set_eow(rb.eow());
  return ConsumeNextImplNoSplit(exec_state, *output_rb, parent_idx);
}

Status GRPCSinkNode::ConsumeNextImpl(ExecState* exec_state, const RowBatch& rb, size_t parent_idx) {
  if (rb.NumBytes() > (max_batch_size_ * batch_size_factor_)) {
    return SplitAndSendBatch(exec_state, rb, parent_idx);
  }
  return ConsumeNextImplNoSplit(exec_state, rb, parent_idx);
}

Status GRPCSinkNode::ConsumeNextImplNoSplit(ExecState* exec_state, const RowBatch& rb, size_t) {
  PX_ASSIGN_OR_RETURN(auto req, RequestWithMetadata(plan_node_.get(), exec_state));
  // Serialize the RowBatch.
  PX_RETURN_IF_ERROR(rb.ToProto(req.mutable_query_result()->mutable_row_batch()));

  PX_RETURN_IF_ERROR(TryWriteRequest(exec_state, req));

  if (!rb.eos()) {
    return Status::OK();
  }

  PX_RETURN_IF_ERROR(CloseWriter(exec_state));
  sent_eos_ = true;

  return response_.success()
             ? Status::OK()
             : error::Internal(absl::Substitute(
                   "GRPCSinkNode $0 encountered error sending stream to address $1, message: $2",
                   plan_node_->id(), plan_node_->address(), response_.message()));
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
