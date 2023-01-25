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

#include "src/carnot/exec/grpc_source_node.h"

#include <string>
#include <utility>
#include <vector>

#include <absl/strings/substitute.h>

#include "src/carnot/planpb/plan.pb.h"

namespace px {
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
  PX_RETURN_IF_ERROR(PopRowBatch());
  PX_RETURN_IF_ERROR(SendRowBatchToChildren(exec_state, *rb_));
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
  if (!rb_request->has_query_result() || !rb_request->query_result().has_row_batch()) {
    return error::Internal(
        "GRPCSourceNode::PopRowBatch expected TransferResultChunkRequest to have RowBatch "
        "message.");
  }

  PX_ASSIGN_OR_RETURN(rb_, RowBatch::FromProto(rb_request->query_result().row_batch()));
  return Status::OK();
}

bool GRPCSourceNode::NextBatchReady() {
  return HasBatchesRemaining() && row_batch_queue_.size_approx() > 0;
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
