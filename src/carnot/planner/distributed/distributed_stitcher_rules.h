/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include "src/carnot/planner/distributed/coordinator/coordinator.h"
#include "src/carnot/planner/distributed/distributed_plan/distributed_plan.h"
#include "src/carnot/planner/distributed/distributed_rules.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/pattern_match.h"

namespace px {
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
}  // namespace px
