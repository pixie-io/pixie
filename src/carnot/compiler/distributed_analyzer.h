#pragma once
#include <memory>

#include "src/carnot/compiler/distributed_plan.h"
#include "src/carnot/compiler/rule_executor.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace distributed {

/**
 * @brief Executes a set of DistributedRules on an input DistributedPlan, matching the way
 * the Analyzer executes a set of Rules on an input IR.
 *
 */
class DistributedAnalyzer : public RuleExecutor<DistributedPlan> {
 public:
  static StatusOr<std::unique_ptr<DistributedAnalyzer>> Create() {
    std::unique_ptr<DistributedAnalyzer> analyzer(new DistributedAnalyzer());
    PL_RETURN_IF_ERROR(analyzer->Init());
    return analyzer;
  }

 private:
  Status Init() { return Status::OK(); }
};

}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
