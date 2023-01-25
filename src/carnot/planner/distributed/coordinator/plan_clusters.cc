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

#include <algorithm>
#include <memory>
#include <queue>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/carnot/planner/distributed/coordinator/plan_clusters.h"
#include "src/carnot/planner/rules/rules.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/uuid/uuid.h"
#include "src/shared/upid/upid.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

StatusOr<std::unique_ptr<IR>> PlanCluster::CreatePlan(const IR* base_query) const {
  // TODO(philkuz) invert this so we don't clone everything.
  PX_ASSIGN_OR_RETURN(std::unique_ptr<IR> new_ir, base_query->Clone());
  for (const auto& op : ops_to_remove) {
    // Some ops to remove are dependent upon each other, so they might be removed beforehand.
    if (!new_ir->HasNode(op->id())) {
      continue;
    }
    auto cur_op = new_ir->Get(op->id());
    CHECK(cur_op != nullptr);
    CHECK(Match(cur_op, Operator()));
    std::queue<OperatorIR*> ancestor_to_maybe_delete_q;
    for (const auto& p : static_cast<OperatorIR*>(new_ir->Get(op->id()))->parents()) {
      ancestor_to_maybe_delete_q.push(p);
    }

    PX_RETURN_IF_ERROR(new_ir->DeleteSubtree(op->id()));
    while (!ancestor_to_maybe_delete_q.empty()) {
      OperatorIR* ancestor = ancestor_to_maybe_delete_q.front();
      ancestor_to_maybe_delete_q.pop();
      // If all the children have been deleted, clean up the ancestor.
      if (ancestor->Children().size() != 0) {
        continue;
      }
      for (const auto& p : ancestor->parents()) {
        ancestor_to_maybe_delete_q.push(p);
      }
      PX_RETURN_IF_ERROR(new_ir->DeleteSubtree(ancestor->id()));
    }
  }
  return new_ir;
}

/**
 * A mapping of agent IDs to the corresponding plan.
 */
struct AgentToPlanMap {
  absl::flat_hash_map<int64_t, IR*> agent_to_plan_map;
  std::vector<std::unique_ptr<IR>> plan_pool;
  absl::flat_hash_map<IR*, absl::flat_hash_set<int64_t>> plan_to_agents;
};

/**
 * @brief Clusters Agents together based on similar sets of Operators to prune from the original
 * query plan.
 *
 * Finds the unique PEM plans based on the agents that remove the same set of Operators.
 *
 * @param set
 * @return std::vector<PlanCluster>
 */
std::vector<PlanCluster> ClusterOperators(const OperatorToAgentSet& set) {
  OperatorToAgentSet op_to_agents = set;
  std::vector<PlanCluster> plan_clusters;
  // While we still have agents that are in the ops_to_agents set.
  // Every loop iteration should finish with a new cluster.
  while (!op_to_agents.empty()) {
    absl::flat_hash_set<OperatorIR*> operators;
    absl::flat_hash_set<int64_t> current_set;
    for (const auto& [op, agent_set] : op_to_agents) {
      if (agent_set.empty()) {
        continue;
      }
      // If the current_set is empty, we need to start accumulating it and this operator will be
      // the first of the new cluster.
      if (current_set.empty()) {
        operators.insert(op);
        current_set = agent_set;
        continue;
      }
      absl::flat_hash_set<int64_t> intersection;
      for (const auto& c : current_set) {
        if (agent_set.contains(c)) {
          intersection.insert(c);
        }
      }
      // If the intersection is empty, we should just not include this op for now.
      if (intersection.empty()) {
        continue;
      }
      // If the intersection is non-empty that is our new set of agents for the cluster.
      current_set = std::move(intersection);
      operators.insert(op);
    }
    // Create the new cluster with the set of agents and the operators.
    plan_clusters.emplace_back(current_set, operators);
    // Remove the agents in the clusters from the OperatorToAgentSet mapping, as we know they
    // should not belong in other clusters.
    OperatorToAgentSet new_op_to_agents;
    for (const auto& [op, agents] : op_to_agents) {
      for (const auto& agent : agents) {
        if (current_set.contains(agent)) {
          continue;
        }
        new_op_to_agents[op].insert(agent);
      }
    }
    op_to_agents = std::move(new_op_to_agents);
  }
  return plan_clusters;
}

/**
 * @brief Returns the set of all_agents that don't appear in OperatorToAgentSet.
 *
 * @param op_to_agent_set The operators that can be removed on the specified agents.
 * @param all_agents Every agent that we want to do work.
 * @return absl::flat_hash_set<int64_t>
 */
absl::flat_hash_set<int64_t> RemainingAgents(const OperatorToAgentSet& op_to_agent_set,
                                             const absl::flat_hash_set<int64_t>& all_agents) {
  auto remaining_agents = all_agents;
  for (const auto& [op, agent_set] : op_to_agent_set) {
    for (const auto& agent : agent_set) {
      remaining_agents.erase(agent);
    }
  }
  return remaining_agents;
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
