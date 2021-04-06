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
 * PreSplitAnalyzer performs a PEM and Kelvin-aware transformations on the IR before it is
 * split by the distributed splitter. This phase is intended for transformations that need to be
 * aware of distributed aspects of the plan, without needing to perform a per-Carnot instance
 * level of optimization that would need to execute on every replicated PEM plan. For optimizations,
 * rather than correctness-oriented transformations, see the PreSplitOptimizer.
 */
class PreSplitAnalyzer : public RuleExecutor<IR> {
 public:
  static StatusOr<std::unique_ptr<PreSplitAnalyzer>> Create(CompilerState* compiler_state) {
    std::unique_ptr<PreSplitAnalyzer> optimizer(new PreSplitAnalyzer(compiler_state));
    PL_RETURN_IF_ERROR(optimizer->Init());
    return optimizer;
  }

 private:
  explicit PreSplitAnalyzer(CompilerState* compiler_state) : compiler_state_(compiler_state) {}

  Status Init() {
    PL_UNUSED(compiler_state_);
    return Status::OK();
  }

  CompilerState* compiler_state_;
};
}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
