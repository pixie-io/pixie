#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"

namespace pl {
namespace carnot {
namespace exec {

class MemorySinkNode : public SinkNode {
 public:
  MemorySinkNode() : SinkNode() {}

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(const plan::Operator &plan_node, const RowDescriptor &output_descriptor,
                  const std::vector<RowDescriptor> &input_descriptors) override;
  Status PrepareImpl(ExecState *exec_state) override;
  Status OpenImpl(ExecState *exec_state) override;
  Status CloseImpl(ExecState *exec_state) override;
  Status ConsumeNextImpl(ExecState *exec_state, const RowBatch &rb) override;

 private:
  std::unique_ptr<plan::MemorySinkOperator> plan_node_;
  std::unique_ptr<RowDescriptor> output_descriptor_;
  std::shared_ptr<Table> table_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
