#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace exec {

class MemorySourceNode : public SourceNode {
 public:
  MemorySourceNode() = default;
  bool HasBatchesRemaining() override;
  bool NextBatchReady() override;

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
  int64_t num_batches_;
  int64_t current_batch_ = 0;
  table_store::BatchPosition start_batch_info_;

  std::unique_ptr<plan::MemorySourceOperator> plan_node_;
  std::unique_ptr<table_store::schema::RowDescriptor> output_descriptor_;
  table_store::Table* table_ = nullptr;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
