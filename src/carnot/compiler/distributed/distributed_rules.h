#pragma once
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include "src/carnot/compiler/distributed/distributed_plan.h"
#include "src/carnot/compiler/rules/rule_executor.h"
#include "src/carnot/compiler/rules/rules.h"

namespace pl {
namespace carnot {
namespace compiler {
template <>
struct RuleTraits<distributed::DistributedPlan> {
  using node_type = distributed::CarnotInstance;
};

namespace distributed {
using DistributedRule = BaseRule<distributed::DistributedPlan>;
using DistributedRuleBatch = BaseRuleBatch<DistributedRule>;
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
  DistributedIRRule() : DistributedRule(nullptr) { subrule_ = std::make_unique<TRule>(); }

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
  explicit PruneUnavailableSourcesRule(distributedpb::CarnotInfo carnot_info)
      : Rule(nullptr), carnot_info_(carnot_info) {}
  StatusOr<bool> Apply(IRNode* node) override;

 private:
  StatusOr<bool> RemoveSourceIfNotNecessary(OperatorIR* node);
  StatusOr<bool> MaybePruneMemorySource(MemorySourceIR* mem_src);
  StatusOr<bool> MaybePruneUDTFSource(UDTFSourceIR* udtf_src);

  bool AgentExecutesUDTF(UDTFSourceIR* source, const distributedpb::CarnotInfo& carnot_info);
  bool UDTFMatchesFilters(UDTFSourceIR* source, const distributedpb::CarnotInfo& carnot_info);

  bool AgentSupportsMemorySources();

  bool IsKelvin(const distributedpb::CarnotInfo& carnot_info);
  bool IsPEM(const distributedpb::CarnotInfo& carnot_info);

  distributedpb::CarnotInfo carnot_info_;
};

/**
 * @brief This rule removes sources from the plan that don't run on a particular Carnot instance.
 * For example, some UDTFSources should only run on Kelvins or run on only some PEMs.
 *
 */
class DistributedPruneUnavailableSourcesRule : public DistributedRule {
 public:
  DistributedPruneUnavailableSourcesRule() : DistributedRule(nullptr) {}

 protected:
  StatusOr<bool> Apply(distributed::CarnotInstance* node) override;
};

/**
 * @brief Prunes out plans that don't have nodes.
 *
 */
class PruneEmptyPlansRule : public DistributedRule {
 public:
  PruneEmptyPlansRule() : DistributedRule(nullptr) {}

 protected:
  StatusOr<bool> Apply(distributed::CarnotInstance* node) override;
};

}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
