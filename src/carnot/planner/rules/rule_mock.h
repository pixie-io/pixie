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
  explicit MockRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}
  MockRule() : Rule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

  MOCK_METHOD1(Execute, StatusOr<bool>(IR* ir_graph));

 protected:
  MOCK_METHOD1(Apply, StatusOr<bool>(IRNode* ir_node));
};
}  // namespace planner
}  // namespace carnot
}  // namespace pl
