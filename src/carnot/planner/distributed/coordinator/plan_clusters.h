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

#include <algorithm>
#include <memory>
#include <queue>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/carnot/planner/rules/rules.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/uuid/uuid.h"
#include "src/shared/upid/upid.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

using OperatorToAgentSet = absl::flat_hash_map<OperatorIR*, absl::flat_hash_set<int64_t>>;

/**
 * @brief Data structure that tracks a set of agents that can remove an Operator and simplifies
 * the set logic to combine two such data structures when recursively evaluating subexpressions that
 * produce different sets of agents and need to be combined.
 *
 * Comes with Methods that simplify an otherwise complex set logic for combining two agent sets
 * using boolean logic. The complexity comes the fact this data structure nature: these are agents
 * to _remove_ rather than keep, so the set operations are flipped from typical And/Or set logic.
 *
 * Simply:
 * And := Union
 * Or := Intersection
 *
 */
struct AgentSet {
  AgentSet Union(const AgentSet& other) {
    // If either struct is empty(), we only trim the other side of the expression.
    if (other.agents.empty()) {
      return *this;
    }
    // If this has no_agents, we return whatever is in other.
    if (agents.empty()) {
      return other;
    }
    AgentSet unioned;
    for (const auto& agent : agents) {
      unioned.agents.insert(agent);
    }
    for (const auto& agent : other.agents) {
      unioned.agents.insert(agent);
    }
    return unioned;
  }

  AgentSet Intersection(const AgentSet& other) {
    // If either struct is empty(), that means we trim no agents so we return the empty struct.
    if (other.agents.empty() || agents.empty()) {
      return AgentSet();
    }
    AgentSet intersection;
    const absl::flat_hash_set<int64_t>* smaller = &(other.agents);
    const absl::flat_hash_set<int64_t>* bigger = &agents;
    if (smaller->size() > bigger->size()) {
      bigger = &(other.agents);
      smaller = &agents;
    }
    for (const auto& agent : *smaller) {
      if (bigger->contains(agent)) {
        intersection.agents.insert(agent);
      }
    }
    return intersection;
  }

  absl::flat_hash_set<int64_t> agents;
};

/**
 * @brief PlanCluster is the data structure that represents each unique plan in the distributed
 * plan. Each cluster represents a plan derivable from the original base plan and the agents that
 * should receive this plan.
 *
 * To create the derivable plan, call CreatePlan().
 *
 */
struct PlanCluster {
  PlanCluster(absl::flat_hash_set<int64_t> agents, absl::flat_hash_set<OperatorIR*> ops)
      : agent_set(std::move(agents)), ops_to_remove(std::move(ops)) {}

  StatusOr<std::unique_ptr<IR>> CreatePlan(const IR* base_query) const;

  // The agents that correspond to this plan.
  absl::flat_hash_set<int64_t> agent_set;
  absl::flat_hash_set<OperatorIR*> ops_to_remove;
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
std::vector<PlanCluster> ClusterOperators(const OperatorToAgentSet& set);

/**
 * @brief Returns the set of all_agents that don't appear in OperatorToAgentSet.
 *
 * @param op_to_agent_set The operators that can be removed on the specified agents.
 * @param all_agents Every agent that we want to do work.
 * @return absl::flat_hash_set<int64_t>
 */
absl::flat_hash_set<int64_t> RemainingAgents(const OperatorToAgentSet& op_to_agent_set,
                                             const absl::flat_hash_set<int64_t>& all_agents);

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
