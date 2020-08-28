#pragma once
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <sole.hpp>
#include "src/carnot/planner/distributed/distributed_plan.h"
#include "src/carnot/planner/rules/rule_executor.h"
#include "src/carnot/planner/rules/rules.h"

namespace pl {
namespace carnot {
namespace planner {
template <>
struct RuleTraits<distributed::DistributedPlan> {
  using node_type = distributed::CarnotInstance;
};

namespace distributed {
using DistributedRule = BaseRule<distributed::DistributedPlan>;
using DistributedRuleBatch = BaseRuleBatch<DistributedRule>;
using SchemaMap = absl::flat_hash_map<std::string, absl::flat_hash_set<sole::uuid>>;
/**
 * @brief This class supports running an IR graph rule (independently) over each IR graph of a
 * DistributedPlan. This is distinct from other DistributedRules, which may modify the
 * CarnotInstances and DistributedPlan dag.
 * Note that this rule shares the state of its inner rule across all Carnot instances.
 * TODO(nserrino): Add a version of this where there is a map from CarnotInstance to Rule,
 * so that non-state-sharing use cases are supported.
 *
 */
template <typename TRule>
class DistributedIRRule : public DistributedRule {
 public:
  DistributedIRRule()
      : DistributedRule(nullptr, /*use_topo*/ true, /*reverse_topological_execution*/ false) {
    subrule_ = std::make_unique<TRule>();
  }

  // Used for testing.
  TRule* subrule() { return subrule_.get(); }

 protected:
  StatusOr<bool> Apply(distributed::CarnotInstance* node) override {
    return subrule_->Execute(node->plan());
  }

  std::unique_ptr<TRule> subrule_;
};

class PruneUnavailableSourcesRule : public Rule {
 public:
  PruneUnavailableSourcesRule(distributedpb::CarnotInfo carnot_info, const SchemaMap& schema_map);
  StatusOr<bool> Apply(IRNode* node) override;

 private:
  StatusOr<bool> RemoveSourceIfNotNecessary(OperatorIR* node);
  StatusOr<bool> MaybePruneMemorySource(MemorySourceIR* mem_src);
  StatusOr<bool> MaybePruneUDTFSource(UDTFSourceIR* udtf_src);

  bool AgentExecutesUDTF(UDTFSourceIR* source, const distributedpb::CarnotInfo& carnot_info);
  bool UDTFMatchesFilters(UDTFSourceIR* source, const distributedpb::CarnotInfo& carnot_info);

  bool AgentSupportsMemorySources();
  bool AgentHasTable(std::string table_name);

  bool IsKelvin(const distributedpb::CarnotInfo& carnot_info);
  bool IsPEM(const distributedpb::CarnotInfo& carnot_info);

  distributedpb::CarnotInfo carnot_info_;

  SchemaMap schema_map_;
  sole::uuid agent_id_;
};

/**
 * @brief This rule removes sources from the plan that don't run on a particular Carnot instance.
 * For example, some UDTFSources should only run on Kelvins or run on only some PEMs.
 *
 */
class DistributedPruneUnavailableSourcesRule : public DistributedRule {
 public:
  explicit DistributedPruneUnavailableSourcesRule(const SchemaMap& schema_map)
      : DistributedRule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false),
        schema_map_(schema_map) {}

 protected:
  StatusOr<bool> Apply(distributed::CarnotInstance* node) override;

  SchemaMap schema_map_;
};

/**
 * @brief Prunes out plans that don't have nodes.
 *
 */
class PruneEmptyPlansRule : public DistributedRule {
 public:
  PruneEmptyPlansRule()
      : DistributedRule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(distributed::CarnotInstance* node) override;
};
/**
 * @brief LoadSchemaMap loads the schema map from a distributed state.
 *
 * @param distributed_state
 * @return StatusOr<SchemaMap>
 */
StatusOr<SchemaMap> LoadSchemaMap(const distributedpb::DistributedState& distributed_state);

/**
 * @brief
 */
class AnnotateAbortableSrcsForLimitsRule : public Rule {
 public:
  explicit AnnotateAbortableSrcsForLimitsRule(IR* graph)
      : Rule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false), graph_(graph) {}
  StatusOr<bool> Apply(IRNode* node) override;

 private:
  IR* graph_;
};

class DistributedAnnotateAbortableSrcsForLimitsRule : public DistributedRule {
 public:
  DistributedAnnotateAbortableSrcsForLimitsRule()
      : DistributedRule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(distributed::CarnotInstance* node) override;
};

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
