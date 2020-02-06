#pragma once
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include "src/carnot/compiler/rules.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace distributed {

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
