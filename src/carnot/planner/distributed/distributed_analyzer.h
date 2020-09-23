#pragma once
#include <memory>

#include "src/carnot/planner/distributed/distributed_plan.h"
#include "src/carnot/planner/distributed/distributed_rules.h"
#include "src/carnot/planner/distributed/distributed_stitcher_rules.h"
#include "src/carnot/planner/distributed/grpc_source_conversion.h"
#include "src/carnot/planner/distributed/tablet_rules.h"
#include "src/carnot/planner/rules/rule_executor.h"

namespace pl {
namespace carnot {
namespace planner {
namespace distributed {

/**
 * @brief Executes a set of DistributedRules on an input DistributedPlan, matching the way
 * the Analyzer executes a set of Rules on an input IR.
 */
class DistributedAnalyzer : public RuleExecutor<DistributedPlan> {
 public:
  static StatusOr<std::unique_ptr<DistributedAnalyzer>> Create(
      const distributedpb::DistributedState& distributed_state) {
    std::unique_ptr<DistributedAnalyzer> analyzer(new DistributedAnalyzer());
    PL_RETURN_IF_ERROR(analyzer->Init(distributed_state));
    return analyzer;
  }

 private:
  void CreateTabletizerBatch() {
    // TryUntilMax used because this should only be run once.
    DistributedRuleBatch* tabletizer_batch = CreateRuleBatch<TryUntilMax>("TabletizePlans", 1);
    tabletizer_batch->AddRule<DistributedTabletizerRule>();
  }

  // void CreateSourcePruneBatch() {
  //   DistributedRuleBatch* source_prune_batch = CreateRuleBatch<TryUntilMax>("SourcePruneBatch",
  //   2); source_prune_batch->AddRule<DistributedPruneUnavailableSourcesRule>(agent_schema_map_);
  // }

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

  void CreateAnnotateLimitsBatch() {
    DistributedRuleBatch* limit_batch =
        CreateRuleBatch<TryUntilMax>("AnnotateLimitsWithAbortableSrcs", 1);
    limit_batch->AddRule<DistributedAnnotateAbortableSrcsForLimitsRule>();
  }

  Status Init(const distributedpb::DistributedState&) {
    // PL_ASSIGN_OR_RETURN(agent_schema_map_, LoadSchemaMap(distributed_state));
    // TODO(philkuz) Enable tabletizer batch when we need it.
    // CreateTabletizerBatch();
    // CreateSourcePruneBatch();
    // CreatePlanPruneBatch();
    // CreateStitcherBatch();
    CreateAnnotateLimitsBatch();
    return Status::OK();
  }

  // SchemaMap agent_schema_map_;
};

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
