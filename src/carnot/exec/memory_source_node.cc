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

#include "src/carnot/exec/memory_source_node.h"

#include <limits>
#include <string>
#include <vector>

#include <absl/strings/substitute.h>

#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace exec {

std::string MemorySourceNode::DebugStringImpl() {
  return absl::Substitute("Exec::MemorySourceNode: <name: $0, output: $1>", plan_node_->TableName(),
                          output_descriptor_->DebugString());
}

Status MemorySourceNode::InitImpl(const plan::Operator& plan_node) {
  CHECK(plan_node.op_type() == planpb::OperatorType::MEMORY_SOURCE_OPERATOR);
  const auto* source_plan_node = static_cast<const plan::MemorySourceOperator*>(&plan_node);
  // copy the plan node to local object;
  plan_node_ = std::make_unique<plan::MemorySourceOperator>(*source_plan_node);

  return Status::OK();
}

Status MemorySourceNode::PrepareImpl(ExecState*) { return Status::OK(); }

Status MemorySourceNode::OpenImpl(ExecState* exec_state) {
  table_ = exec_state->table_store()->GetTable(plan_node_->TableName(), plan_node_->Tablet());
  DCHECK(table_ != nullptr);

  infinite_stream_ = plan_node_->infinite_stream();

  if (table_ == nullptr) {
    return error::NotFound("Table '$0' not found", plan_node_->TableName());
  }

  if (plan_node_->HasStartTime()) {
    PL_ASSIGN_OR_RETURN(current_batch_, table_->FindBatchSliceGreaterThanOrEqual(
                                            plan_node_->start_time(), exec_state->exec_mem_pool()));
  } else {
    current_batch_ = table_->FirstBatch();
  }

  if (plan_node_->HasStopTime()) {
    PL_ASSIGN_OR_RETURN(stop_, table_->FindStopPositionForTime(plan_node_->stop_time(),
                                                               exec_state->exec_mem_pool()));
  } else {
    // Determine table_end at Open() time because Stirling may be pushing to the table
    stop_ = table_->End();
  }
  current_batch_ = table_->SliceIfPastStop(current_batch_, stop_);

  return Status::OK();
}

Status MemorySourceNode::CloseImpl(ExecState*) {
  stats()->AddExtraInfo("infinite_stream", infinite_stream_ ? "true" : "false");
  return Status::OK();
}

StatusOr<std::unique_ptr<RowBatch>> MemorySourceNode::GetNextRowBatch(ExecState* exec_state) {
  DCHECK(table_ != nullptr);

  if (infinite_stream_ && wait_for_valid_next_) {
    // If it's an infinite_stream that has read out all the current data in the table, we have to
    // keep around the last batch the infinite stream output and keep checking if the next batch
    // after that is valid so that when stirling writes more data we are able to access it.
    stop_ = table_->End();
    auto next_batch = table_->NextBatch(current_batch_, stop_);
    if (!next_batch.IsValid()) {
      return RowBatch::WithZeroRows(*output_descriptor_, /* eow */ false, /* eos */ false);
    }
    current_batch_ = next_batch;
    wait_for_valid_next_ = false;
  }

  if (!current_batch_.IsValid()) {
    return RowBatch::WithZeroRows(*output_descriptor_, /* eow */ !infinite_stream_,
                                  /* eos */ !infinite_stream_);
  }

  PL_ASSIGN_OR_RETURN(
      auto row_batch,
      table_->GetRowBatchSlice(current_batch_, plan_node_->Columns(), exec_state->exec_mem_pool()));

  rows_processed_ += row_batch->num_rows();
  bytes_processed_ += row_batch->NumBytes();
  auto next_batch = table_->NextBatch(current_batch_, stop_);
  if (infinite_stream_ && !next_batch.IsValid()) {
    wait_for_valid_next_ = true;
  } else {
    current_batch_ = next_batch;
  }

  // If infinite stream is set, we don't send Eow or Eos. Infinite streams therefore never cause
  // HasBatchesRemaining to be false. Instead the outer loop that calls GenerateNext() is
  // responsible for managing whether we continue the stream or end it.
  if (!current_batch_.IsValid() && !infinite_stream_) {
    row_batch->set_eow(true);
    row_batch->set_eos(true);
  }
  return row_batch;
}

Status MemorySourceNode::GenerateNextImpl(ExecState* exec_state) {
  PL_ASSIGN_OR_RETURN(auto row_batch, GetNextRowBatch(exec_state));
  PL_RETURN_IF_ERROR(SendRowBatchToChildren(exec_state, *row_batch));
  return Status::OK();
}

bool MemorySourceNode::InfiniteStreamNextBatchReady() {
  if (!wait_for_valid_next_) {
    return current_batch_.IsValid();
  }
  auto next_batch = table_->NextBatch(current_batch_);
  return next_batch.IsValid();
}

bool MemorySourceNode::NextBatchReady() {
  // Next batch is ready if we haven't seen an eow and if it's an infinite_stream that has batches
  // to push.
  return HasBatchesRemaining() && (!infinite_stream_ || InfiniteStreamNextBatchReady());
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
