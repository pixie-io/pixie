#pragma once

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/plan/operators.h"
#include "src/common/base/base.h"
#include "src/common/base/status.h"
#include "src/table_store/schema/row_batch.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;

class MemorySourceNode : public SourceNode {
 public:
  MemorySourceNode() = default;
  bool NextBatchReady() override;

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(const plan::Operator& plan_node) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status GenerateNextImpl(ExecState* exec_state) override;

 private:
  StatusOr<std::unique_ptr<RowBatch>> GetNextRowBatch(ExecState* exec_state);

  int64_t num_batches_;
  int64_t current_batch_ = 0;
  // Whether this memory source will stream infinitely. Can be stopped by the
  // exec_state_->keep_running() call in exec_graph.
  bool infinite_stream_ = false;
  table_store::BatchPosition start_batch_info_;

  std::unique_ptr<plan::MemorySourceOperator> plan_node_;
  table_store::Table* table_ = nullptr;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
