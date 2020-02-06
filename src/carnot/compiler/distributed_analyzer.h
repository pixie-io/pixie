#pragma once
#include <memory>

#include "src/carnot/compiler/distributed_plan.h"
#include "src/carnot/compiler/distributed_rules.h"
#include "src/carnot/compiler/distributed_stitcher_rules.h"
#include "src/carnot/compiler/grpc_source_conversion.h"
#include "src/carnot/compiler/rule_executor.h"
#include "src/carnot/compiler/tablet_rules.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace distributed {

/**
 * @brief Executes a set of DistributedRules on an input DistributedPlan, matching the way
 * the Analyzer executes a set of Rules on an input IR.
 */
class DistributedAnalyzer : public RuleExecutor<DistributedPlan> {
 public:
  static StatusOr<std::unique_ptr<DistributedAnalyzer>> Create() {
    std::unique_ptr<DistributedAnalyzer> analyzer(new DistributedAnalyzer());
    PL_RETURN_IF_ERROR(analyzer->Init());
    return analyzer;
  }

 private:
  void CreateTabletizerBatch() {
    // TryUntilMax used because this should only be run once.
    DistributedRuleBatch* tabletizer_batch = CreateRuleBatch<TryUntilMax>("TabletizePlans", 1);
    tabletizer_batch->AddRule<DistributedTabletizerRule>();
  }

  void CreateSourcePruneBatch() {
    DistributedRuleBatch* source_prune_batch = CreateRuleBatch<TryUntilMax>("SourcePruneBatch", 2);
    source_prune_batch->AddRule<DistributedPruneUnavailableSourcesRule>();
  }

  void CreatePlanPruneBatch() {
    DistributedRuleBatch* plan_prune_batch = CreateRuleBatch<TryUntilMax>("PlanPruneBatch", 2);
    plan_prune_batch->AddRule<PruneEmptyPlansRule>();
  }

  void CreateStitcherBatch() {
    DistributedRuleBatch* stitcher_batch =
        CreateRuleBatch<TryUntilMax>("StitchGRPCBridgesBetweenPlans", 1);
    stitcher_batch->AddRule<DistributedSetSourceGroupGRPCAddressRule>();
    stitcher_batch->AddRule<AssociateDistributedPlanEdgesRule>();
    stitcher_batch->AddRule<DistributedIRRule<GRPCSourceGroupConversionRule>>();
  }

  Status Init() {
    CreateTabletizerBatch();
    CreateSourcePruneBatch();
    CreatePlanPruneBatch();
    CreateStitcherBatch();
    return Status::OK();
  }
};

}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
