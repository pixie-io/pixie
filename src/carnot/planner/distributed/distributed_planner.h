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
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/compiler_state/registry_info.h"
#include "src/carnot/planner/distributed/distributed_plan/distributed_plan.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/pattern_match.h"
#include "src/carnot/planner/rules/rule_executor.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

/**
 * Planner is the base interface for all types of query planners.
 */
class Planner {
 public:
  Planner() = default;
  virtual ~Planner() = default;
  virtual StatusOr<std::unique_ptr<DistributedPlan>> Plan(
      const distributedpb::DistributedState& distributed_state, CompilerState* compiler_state,
      const IR* logical_plan) = 0;
};

/**
 * @brief The planner takes in a logical plan and knowledge about the Machines available for
 * exeuction to create a plan that is close to what is actually executed on the nodes.
 *
 * The distributed plan maps identifiers of nodes to the Plan that corresponds to that node.
 *
 * Distributed planning occurs through the following steps:
 * 0. Planner initialized with the DistributedState
 * 1. Planner receives the logical plan.
 * 2. Split the logical plan into the Agent and Kelvin components.
 * 3. Layout the distributed plan (create the distributed plan dag).
 * 4. Prune extraneous edges.
 * 5. Return the mapping from distributed_node_id to the distributed plan for that node.
 *
 */
class DistributedPlanner : public NotCopyable, public Planner {
 public:
  /**
   * @brief The Creation function for the planner.
   *
   * @return StatusOr<std::unique_ptr<DistributedPlanner>>: the distributed planner object or an
   * error.
   */
  static StatusOr<std::unique_ptr<DistributedPlanner>> Create();

  /**
   * @brief Takes in a logical plan and outputs the distributed plan.
   *
   * @param distributed_state: the distributed layout of the vizier instance.
   * @param compiler_state: informastion passed to the compiler.
   * @param logical_plan
   * @return StatusOr<std::unique_ptr<DistributedPlan>>
   */
  StatusOr<std::unique_ptr<DistributedPlan>> Plan(
      const distributedpb::DistributedState& distributed_state, CompilerState* compiler_state,
      const IR* logical_plan) override;

 private:
  DistributedPlanner() {}

  Status Init();
};

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
