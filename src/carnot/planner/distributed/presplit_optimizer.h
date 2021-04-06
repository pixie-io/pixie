#pragma once
#include <memory>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/distributed/distributed_rules.h"
#include "src/carnot/planner/rules/rule_executor.h"

namespace pl {
namespace carnot {
namespace planner {
namespace distributed {

/**
 * PreSplitOptimizer performs a PEM and Kelvin-aware optimization on the IR before it is
 * split by the distributed splitter. This phase is intended for optimizations that need to be
 * aware of distributed aspects of the plan, without needing to perform a per-Carnot instance
 * level of optimization that would need to execute on every replicated PEM plan.
 */
class PreSplitOptimizer : public RuleExecutor<IR> {
 public:
  static StatusOr<std::unique_ptr<PreSplitOptimizer>> Create(CompilerState* compiler_state) {
    std::unique_ptr<PreSplitOptimizer> optimizer(new PreSplitOptimizer(compiler_state));
    PL_RETURN_IF_ERROR(optimizer->Init());
    return optimizer;
  }

 private:
  explicit PreSplitOptimizer(CompilerState* compiler_state) : compiler_state_(compiler_state) {}

  void CreateLimitPushdownBatch() {
    RuleBatch* limit_pushdown = CreateRuleBatch<FailOnMax>("LimitPushdown", 2);
    limit_pushdown->AddRule<LimitPushdownRule>(compiler_state_);
  }

  Status Init() {
    CreateLimitPushdownBatch();
    return Status::OK();
  }
  CompilerState* compiler_state_;
};
}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
