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
#include "src/table_store/table/table.h"

#include <limits>
#include <string>
#include <vector>

#include <absl/strings/substitute.h>

#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace exec {

using StartSpec = Table::Cursor::StartSpec;
using StopSpec = Table::Cursor::StopSpec;

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

  streaming_ = plan_node_->streaming();

  if (table_ == nullptr) {
    return error::NotFound("Table '$0' not found", plan_node_->TableName());
  }

  StartSpec start_spec;
  if (plan_node_->HasStartTime()) {
    start_spec.type = StartSpec::StartType::StartAtTime;
    start_spec.start_time = plan_node_->start_time();
  } else {
    start_spec.type = StartSpec::StartType::CurrentStartOfTable;
  }

  StopSpec stop_spec;
  if (streaming_) {
    if (plan_node_->HasStopTime()) {
      stop_spec.type = StopSpec::StopType::StopAtTime;
      stop_spec.stop_time = plan_node_->stop_time();
    } else {
      stop_spec.type = StopSpec::StopType::Infinite;
    }
  } else {
    if (plan_node_->HasStopTime()) {
      stop_spec.type = StopSpec::StopType::StopAtTimeOrEndOfTable;
      stop_spec.stop_time = plan_node_->stop_time();
    } else {
      stop_spec.type = StopSpec::StopType::CurrentEndOfTable;
    }
  }
  cursor_ = std::make_unique<Table::Cursor>(table_, start_spec, stop_spec);

  return Status::OK();
}

Status MemorySourceNode::CloseImpl(ExecState*) {
  stats()->AddExtraInfo("streaming", streaming_ ? "true" : "false");
  return Status::OK();
}

StatusOr<std::unique_ptr<RowBatch>> MemorySourceNode::GetNextRowBatch(ExecState*) {
  DCHECK(table_ != nullptr);

  if (!cursor_->NextBatchReady()) {
    // If the NextBatch is not ready, but the cursor is not yet exhausted, then we need to output
    // 0-row row batches, while we wait for more data to be added. This currently only occurs in the
    // case of an infinite stream. In the future, it should also occur when a stop time is set in
    // the future, but this is not yet supported by Table.
    // If the cursor is exhausted, then we return a 0-row row batch with eow=eos=true.
    return RowBatch::WithZeroRows(*output_descriptor_, /* eow */ cursor_->Done(),
                                  /* eos */ cursor_->Done());
  }

  PX_ASSIGN_OR_RETURN(auto row_batch, cursor_->GetNextRowBatch(plan_node_->Columns()));

  rows_processed_ += row_batch->num_rows();
  bytes_processed_ += row_batch->NumBytes();

  // If infinite stream is set, we don't send Eow or Eos. Infinite streams therefore never cause
  // HasBatchesRemaining to be false. Instead the outer loop that calls GenerateNext() is
  // responsible for managing whether we continue the stream or end it.
  if (cursor_->Done()) {
    row_batch->set_eow(true);
    row_batch->set_eos(true);
  }
  return row_batch;
}

Status MemorySourceNode::GenerateNextImpl(ExecState* exec_state) {
  PX_ASSIGN_OR_RETURN(auto row_batch, GetNextRowBatch(exec_state));
  PX_RETURN_IF_ERROR(SendRowBatchToChildren(exec_state, *row_batch));
  return Status::OK();
}

bool MemorySourceNode::InfiniteStreamNextBatchReady() { return cursor_->NextBatchReady(); }

bool MemorySourceNode::NextBatchReady() {
  // Next batch is ready if we haven't seen an eow and if it's an infinite_stream that has batches
  // to push.
  return HasBatchesRemaining() && (!streaming_ || InfiniteStreamNextBatchReady());
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
