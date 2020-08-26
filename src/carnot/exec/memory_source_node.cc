#include "src/carnot/exec/memory_source_node.h"

#include <limits>
#include <string>
#include <vector>

#include <absl/strings/substitute.h>

#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"

namespace pl {
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
  // Determine number of chunks at Open() time
  // because Stirling may be pushing to the table
  num_batches_ = table_->NumBatches();

  if (plan_node_->HasStartTime()) {
    start_batch_info_ = table_->FindBatchPositionGreaterThanOrEqual(plan_node_->start_time(),
                                                                    exec_state->exec_mem_pool());

    // TODO(philkuz) might have a race condition where the data hasn't loaded yet for the
    // start_time.

    // If start batch_idx == -1, no batches exist with a timestamp greater than or equal to the
    // given start time.
    current_batch_ = !start_batch_info_.FoundValidBatches() ? std::numeric_limits<int64_t>::max()
                                                            : start_batch_info_.batch_idx;
  }

  return Status::OK();
}

Status MemorySourceNode::CloseImpl(ExecState*) {
  stats()->AddExtraMetric("infinite", static_cast<double>(infinite_stream_));
  return Status::OK();
}

StatusOr<std::unique_ptr<RowBatch>> MemorySourceNode::GetNextRowBatch(ExecState* exec_state) {
  DCHECK(table_ != nullptr);

  if (current_batch_ >= table_->NumBatches()) {
    return RowBatch::WithZeroRows(*output_descriptor_, /* eow */ true, /* eos */ true);
  }

  auto offset = 0;
  auto end = -1;
  if (plan_node_->HasStartTime() && current_batch_ == start_batch_info_.batch_idx) {
    offset = start_batch_info_.row_idx;
  }

  // TODO(michelle): PL-388 Fix our table store to correctly support hot/cold data. For now, do not
  // support StopTime, since the current implementation is buggy.
  PL_ASSIGN_OR_RETURN(auto row_batch,
                      table_->GetRowBatchSlice(current_batch_, plan_node_->Columns(),
                                               exec_state->exec_mem_pool(), offset, end));

  rows_processed_ += row_batch->num_rows();
  bytes_processed_ += row_batch->NumBytes();
  current_batch_++;

  // If infinite stream is set, we don't send Eow or Eos. Infinite streams therefore never cause
  // HasBatchesRemaining to be false. Instead the outer loop that calls GenerateNext() is
  // responsible for managing whether we continue the stream or end it.
  if (current_batch_ >= table_->NumBatches() && !infinite_stream_) {
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

bool MemorySourceNode::NextBatchReady() {
  // Next batch is ready if we haven't seen an eow and if it's an infinite_stream that has batches
  // to push.
  return HasBatchesRemaining() && (!infinite_stream_ || (current_batch_ < table_->NumBatches()));
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
