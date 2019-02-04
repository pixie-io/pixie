#pragma once

#include <gmock/gmock.h>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_node.h"

namespace pl {
namespace carnot {
namespace exec {

class MockExecNode : public ExecNode {
 public:
  MockExecNode() : ExecNode(ExecNodeType::kProcessingNode) {}
  explicit MockExecNode(const ExecNodeType& exec_node_type) : ExecNode(exec_node_type) {}

  MOCK_METHOD0(DebugStringImpl, std::string());
  MOCK_METHOD3(InitImpl,
               Status(const plan::Operator& plan_node, const RowDescriptor& output_descriptor,
                      const std::vector<RowDescriptor>& input_descriptors));
  MOCK_METHOD1(PrepareImpl, Status(ExecState* exec_state));
  MOCK_METHOD1(OpenImpl, Status(ExecState* exec_state));
  MOCK_METHOD1(CloseImpl, Status(ExecState* exec_state));
  MOCK_METHOD1(GenerateNextImpl, Status(ExecState*));
  MOCK_METHOD2(ConsumeNextImpl, Status(ExecState*, const RowBatch&));
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
