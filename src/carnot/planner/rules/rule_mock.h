#pragma once
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/planner/rules/rules.h"

namespace pl {
namespace carnot {
namespace planner {
class MockRule : public Rule {
 public:
  explicit MockRule(CompilerState* compiler_state) : Rule(compiler_state) {}
  MockRule() : Rule(nullptr) {}

  MOCK_METHOD1(Execute, StatusOr<bool>(IR* ir_graph));

 protected:
  MOCK_METHOD1(Apply, StatusOr<bool>(IRNode* ir_node));
};
}  // namespace planner
}  // namespace carnot
}  // namespace pl
