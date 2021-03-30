#pragma once

#include "src/carnot/planner/ir/ir_nodes.h"
#include "src/carnot/planner/rules/rules.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * @brief This rule pushes limits as early in the IR as possible. It should be at most once, because
 * in the case of a limit preceded by a union, the rule will clone the limit to exist before and
 * after each instance of the union. The rule does not contain the logic to check if a past
 * invocation of the rule has already pushed a given limit up before a union.
 * TODO(nserrino): Add support to allow this rule to run multiple times in a row.
 *
 */
class LimitPushdownRule : public Rule {
 public:
  LimitPushdownRule() : Rule(nullptr, /*use_topo*/ true, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode*) override;

 private:
  absl::flat_hash_set<OperatorIR*> NewLimitParents(OperatorIR* current_node);
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
