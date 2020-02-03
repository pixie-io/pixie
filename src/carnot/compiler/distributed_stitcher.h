#pragma once
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include "src/carnot/compiler/distributed_coordinator.h"
#include "src/carnot/compiler/distributed_plan.h"
#include "src/carnot/compiler/distributed_rules.h"
#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/ir/pattern_match.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace distributed {

using distributedpb::CarnotInfo;

/**
 * @brief Sets the GRPC addresses and query broker addresses in GRPCSourceGroups.
 */
class SetSourceGroupGRPCAddressRule : public Rule {
 public:
  explicit SetSourceGroupGRPCAddressRule(const std::string& grpc_address,
                                         const std::string& query_broker_address)
      : Rule(nullptr), grpc_address_(grpc_address), query_broker_address_(query_broker_address) {}

 private:
  StatusOr<bool> Apply(IRNode* node) override;
  std::string grpc_address_;
  std::string query_broker_address_;
};

/**
 * @brief Distributed wrapper of SetSourceGroupGRPCAddressRule to apply the rule using the info of
 * each carnot instance.
 */
class DistributedSetSourceGroupGRPCAddressRule : public DistributedRule {
 public:
  DistributedSetSourceGroupGRPCAddressRule() : DistributedRule(nullptr) {}

 protected:
  StatusOr<bool> Apply(CarnotInstance* carnot_instance) override {
    SetSourceGroupGRPCAddressRule rule(carnot_instance->carnot_info().grpc_address(),
                                       carnot_instance->carnot_info().query_broker_address());
    return rule.Execute(carnot_instance->plan());
  }
};

/**
 * @brief Connects the GRPCSinks to GRPCSourceGroups.
 *
 */
class AssociateDistributedPlanEdgesRule : public DistributedRule {
 public:
  AssociateDistributedPlanEdgesRule() : DistributedRule(nullptr) {}

 protected:
  StatusOr<bool> Apply(CarnotInstance* from_carnot_instance) override;
  StatusOr<bool> ConnectGraphs(IR* from_graph, IR* to_graph);
};

}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
