#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
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
  bool HasBatchesRemaining() override;
  bool NextBatchReady() override;
  virtual Status EnqueueRowBatch(std::unique_ptr<carnotpb::RowBatchRequest> row_batch);

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(
      const plan::Operator& plan_node, const table_store::schema::RowDescriptor& output_descriptor,
      const std::vector<table_store::schema::RowDescriptor>& input_descriptors) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status GenerateNextImpl(ExecState* exec_state) override;

 private:
  Status OptionallyPopRowBatch();

  static constexpr std::chrono::microseconds grpc_timeout_us_{150 * 1000};
  bool sent_eos_ = false;
  std::unique_ptr<table_store::schema::RowBatch> next_up_;
  moodycamel::BlockingConcurrentQueue<std::unique_ptr<carnotpb::RowBatchRequest>> row_batch_queue_;

  std::unique_ptr<plan::GRPCSourceOperator> plan_node_;
  std::unique_ptr<table_store::schema::RowDescriptor> output_descriptor_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
