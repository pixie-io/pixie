#pragma once
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include "src/carnot/planner/distributed/distributed_coordinator.h"
#include "src/carnot/planner/distributed/distributed_plan.h"
#include "src/carnot/planner/distributed/distributed_rules.h"
#include "src/carnot/planner/ir/ir_nodes.h"
#include "src/carnot/planner/ir/pattern_match.h"

namespace pl {
namespace carnot {
namespace planner {
namespace distributed {

using distributedpb::CarnotInfo;

/**
 * @brief Sets the GRPC addresses and query broker addresses in GRPCSourceGroups.
 */
class SetSourceGroupGRPCAddressRule : public Rule {
 public:
  SetSourceGroupGRPCAddressRule(const std::string& grpc_address, const std::string& ssl_targetname)
      : Rule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false),
        grpc_address_(grpc_address),
        ssl_targetname_(ssl_targetname) {}

 private:
  StatusOr<bool> Apply(IRNode* node) override;
  std::string grpc_address_;
  std::string ssl_targetname_;
};

/**
 * @brief Distributed wrapper of SetSourceGroupGRPCAddressRule to apply the rule using the info of
 * each carnot instance.
 */
class DistributedSetSourceGroupGRPCAddressRule : public DistributedRule {
 public:
  DistributedSetSourceGroupGRPCAddressRule()
      : DistributedRule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

  StatusOr<bool> Apply(CarnotInstance* carnot_instance) override {
    SetSourceGroupGRPCAddressRule rule(carnot_instance->carnot_info().grpc_address(),
                                       carnot_instance->carnot_info().ssl_targetname());
    return rule.Execute(carnot_instance->plan());
  }
};

/**
 * @brief Connects the GRPCSinks to GRPCSourceGroups.
 *
 */
class AssociateDistributedPlanEdgesRule : public DistributedRule {
 public:
  AssociateDistributedPlanEdgesRule()
      : DistributedRule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

  static StatusOr<bool> ConnectGraphs(IR* from_graph,
                                      const absl::flat_hash_set<int64_t>& from_agents,
                                      IR* to_graph);

 protected:
  StatusOr<bool> Apply(CarnotInstance* from_carnot_instance) override;
};

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
