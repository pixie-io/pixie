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
using SchemaToAgentsMap = absl::flat_hash_map<std::string, absl::flat_hash_set<int64_t>>;
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
  PruneUnavailableSourcesRule(int64_t agent_id, const distributedpb::CarnotInfo& carnot_info,
                              const SchemaToAgentsMap& schema_map);
  StatusOr<bool> Apply(IRNode* node) override;

  static bool UDTFMatchesFilters(UDTFSourceIR* source,
                                 const distributedpb::CarnotInfo& carnot_info);

 private:
  StatusOr<bool> RemoveSourceIfNotNecessary(OperatorIR* node);
  StatusOr<bool> MaybePruneMemorySource(MemorySourceIR* mem_src);
  StatusOr<bool> MaybePruneUDTFSource(UDTFSourceIR* udtf_src);

  bool AgentExecutesUDTF(UDTFSourceIR* source, const distributedpb::CarnotInfo& carnot_info);

  bool AgentSupportsMemorySources();
  bool AgentHasTable(std::string table_name);

  bool IsKelvin(const distributedpb::CarnotInfo& carnot_info);
  bool IsPEM(const distributedpb::CarnotInfo& carnot_info);

  int64_t agent_id_;
  const distributedpb::CarnotInfo& carnot_info_;
  const SchemaToAgentsMap& schema_map_;
};

/**
 * @brief This rule removes sources from the plan that don't run on a particular Carnot instance.
 * For example, some UDTFSources should only run on Kelvins or run on only some PEMs.
 *
 */
class DistributedPruneUnavailableSourcesRule : public DistributedRule {
 public:
  explicit DistributedPruneUnavailableSourcesRule(const SchemaToAgentsMap& schema_map)
      : DistributedRule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false),
        schema_map_(schema_map) {}

  StatusOr<bool> Apply(distributed::CarnotInstance* node) override;

 protected:
  const SchemaToAgentsMap& schema_map_;
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
 * @param uuid_to_id_map
 * @return StatusOr<SchemaToAgentsMap>
 */
StatusOr<SchemaToAgentsMap> LoadSchemaMap(
    const distributedpb::DistributedState& distributed_state,
    const absl::flat_hash_map<sole::uuid, int64_t>& uuid_to_id_map);

/**
 * @brief
 */
class AnnotateAbortableSrcsForLimitsRule : public Rule {
 public:
  AnnotateAbortableSrcsForLimitsRule()
      : Rule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* node) override;
};

class DistributedAnnotateAbortableSrcsForLimitsRule : public DistributedRule {
 public:
  DistributedAnnotateAbortableSrcsForLimitsRule()
      : DistributedRule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(distributed::CarnotInstance* node) override;
};

/**
 * @brief Ensures that all Scalar UDFs in this plan can run on a PEM.
 */
class ScalarUDFsRunOnPEMRule : public Rule {
 public:
  explicit ScalarUDFsRunOnPEMRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

  // Returns true if a given operator's UDFs can successfully run on a PEM.
  static StatusOr<bool> OperatorUDFsRunOnPEM(CompilerState* compiler_state, OperatorIR* op);

 protected:
  StatusOr<bool> Apply(IRNode* node) override;
};

/**
 * @brief Ensures that all Scalar UDFs in this plan can run on a Kelvin.
 */
class ScalarUDFsRunOnKelvinRule : public Rule {
 public:
  explicit ScalarUDFsRunOnKelvinRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

  // Returns true if a given operator's UDFs can successfully run on a Kelvin.
  static StatusOr<bool> OperatorUDFsRunOnKelvin(CompilerState* compiler_state, OperatorIR* op);

 protected:
  StatusOr<bool> Apply(IRNode* node) override;
};

/**
 * @brief This rule pushes limits as early in the IR as possible, without pushing them
 * past PEM-only operators.
 */
class LimitPushdownRule : public Rule {
 public:
  explicit LimitPushdownRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ true, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode*) override;

 private:
  StatusOr<absl::flat_hash_set<OperatorIR*>> NewLimitParents(OperatorIR* current_node);
};

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
