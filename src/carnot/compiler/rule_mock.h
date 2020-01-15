#pragma once
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/compiler/rules.h"

namespace pl {
namespace carnot {
namespace compiler {
class MockRule : public Rule {
 public:
  explicit MockRule(CompilerState* compiler_state) : Rule(compiler_state) {}
  MockRule() : Rule(nullptr) {}

  MOCK_METHOD1(Execute, StatusOr<bool>(IR* ir_graph));

 protected:
  MOCK_METHOD1(Apply, StatusOr<bool>(IRNode* ir_node));
};
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
