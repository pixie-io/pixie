#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/plan/operators.h"
#include "src/carnotpb/carnot.pb.h"
#include "src/common/base/base.h"
#include "src/table_store/table_store.h"

PL_SUPPRESS_WARNINGS_START()
// TODO(nserrino/michelle): Fix this lint issue so that we can remove the warning.
// NOLINTNEXTLINE(build/include_subdir)
#include "blockingconcurrentqueue.h"
PL_SUPPRESS_WARNINGS_END()

namespace pl {
namespace carnot {
namespace exec {

class GRPCSourceNode : public SourceNode {
 public:
  GRPCSourceNode() = default;
  bool NextBatchReady() override;
  virtual Status EnqueueRowBatch(std::unique_ptr<carnotpb::TransferResultChunkRequest> row_batch);

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(const plan::Operator& plan_node) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status GenerateNextImpl(ExecState* exec_state) override;

 private:
  Status PopRowBatch();

  std::unique_ptr<table_store::schema::RowBatch> rb_;
  moodycamel::BlockingConcurrentQueue<std::unique_ptr<carnotpb::TransferResultChunkRequest>>
      row_batch_queue_;

  std::unique_ptr<plan::GRPCSourceOperator> plan_node_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
