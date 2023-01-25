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

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "src/carnot/planner/distributed/annotate_abortable_sources_for_limits_rule.h"
#include "src/carnot/planner/distributed/coordinator/coordinator.h"
#include "src/carnot/planner/distributed/distributed_planner.h"
#include "src/carnot/planner/distributed/distributed_rules.h"
#include "src/carnot/planner/distributed/distributed_stitcher_rules.h"
#include "src/carnot/planner/distributed/grpc_source_conversion.h"
#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

StatusOr<std::unique_ptr<DistributedPlanner>> DistributedPlanner::Create() {
  std::unique_ptr<DistributedPlanner> planner(new DistributedPlanner());
  PX_RETURN_IF_ERROR(planner->Init());
  return planner;
}

Status DistributedPlanner::Init() { return Status::OK(); }
Status StitchPlan(DistributedPlan* distributed_plan) {
  DCHECK(distributed_plan);
  auto remote_carnot = distributed_plan->kelvin();
  DCHECK(remote_carnot);
  auto remote_node_id = remote_carnot->id();
  IR* remote_plan = remote_carnot->plan();
  DCHECK(remote_plan);

  DistributedSetSourceGroupGRPCAddressRule set_grpc_address_rule;
  PX_RETURN_IF_ERROR(set_grpc_address_rule.Apply(remote_carnot));

  // Connect the plans.
  for (const auto& [plan, agents] : distributed_plan->plan_to_agent_map()) {
    PX_ASSIGN_OR_RETURN(auto did_connect_plan, AssociateDistributedPlanEdgesRule::ConnectGraphs(
                                                   plan, agents, remote_plan));
    DCHECK(did_connect_plan);
  }

  // TODO(philkuz) make this connect to self without a grpc bridge.
  PX_RETURN_IF_ERROR(
      AssociateDistributedPlanEdgesRule::ConnectGraphs(remote_plan, {remote_node_id}, remote_plan));

  // Expand GRPCSourceGroups in the remote_plan.
  GRPCSourceGroupConversionRule conversion_rule;
  PX_RETURN_IF_ERROR(conversion_rule.Execute(remote_plan));
  return MergeSameNodeGRPCBridgeRule(remote_node_id).Execute(remote_plan).status();
}

StatusOr<std::unique_ptr<DistributedPlan>> DistributedPlanner::Plan(
    const distributedpb::DistributedState& distributed_state, CompilerState* compiler_state,
    const IR* logical_plan) {
  PX_ASSIGN_OR_RETURN(std::unique_ptr<Coordinator> coordinator,
                      Coordinator::Create(compiler_state, distributed_state));

  PX_ASSIGN_OR_RETURN(std::unique_ptr<DistributedPlan> distributed_plan,
                      coordinator->Coordinate(logical_plan));

  PX_RETURN_IF_ERROR(StitchPlan(distributed_plan.get()));

  AnnotateAbortableSourcesForLimitsRule rule;
  for (IR* agent_plan : distributed_plan->UniquePlans()) {
    rule.Execute(agent_plan);
  }

  return distributed_plan;
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
