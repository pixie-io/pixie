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
#include "src/carnot/planner/distributed/distributed_plan/distributed_plan.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/pattern_match.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

using distributedpb::CarnotInfo;
using SchemaToAgentsMap = absl::flat_hash_map<std::string, absl::flat_hash_set<int64_t>>;

struct CarnotGraph {
  plan::DAG dag;
  absl::flat_hash_map<int64_t, distributedpb::CarnotInfo> id_to_carnot_info;
};

/**
 * @brief The coordinator takes in a physical state and builds up the skeleton
 * of the physical plan graph based on the capabilities of the Carnot nodes passed in.
 */
class Coordinator : public NotCopyable {
 public:
  virtual ~Coordinator() = default;
  static StatusOr<std::unique_ptr<Coordinator>> Create(
      CompilerState* compiler_state, const distributedpb::DistributedState& distributed_state);

  /**
   * @brief Using the physical state and the current plan, assembles a proto Distributed Plan. This
   * plan is not ready to be sent out yet, but can be processed to work.
   * @param plan: the plan, pre-split along the expected lines.
   * @return StatusOr<std::unique_ptr<DistributedPlan>>
   */
  StatusOr<std::unique_ptr<DistributedPlan>> Coordinate(const IR* logical_plan);

  Status Init(CompilerState* compiler_state,
              const distributedpb::DistributedState& distributed_state);

 protected:
  Status ProcessConfig(const CarnotInfo& carnot_info);

  virtual Status InitImpl(CompilerState* compiler_state,
                          const distributedpb::DistributedState& distributed_state) = 0;

  /**
   * @brief Implementation of the Coordinate function. Using the phyiscal state and the plan, should
   * output a CarnotGraph that connects the different carnot instances
   *
   * @return StatusOr<CarnotGraph>
   */
  virtual StatusOr<std::unique_ptr<DistributedPlan>> CoordinateImpl(const IR* logical_plan) = 0;

  virtual Status ProcessConfigImpl(const CarnotInfo& carnot_info) = 0;
};

/**
 * @brief This coordinator creates a plan layout with 1 remote processor getting data
 * from N sources. If the passed in plan has special conditions, it will split differntly.
 *
 */
class CoordinatorImpl : public Coordinator {
 protected:
  StatusOr<std::unique_ptr<DistributedPlan>> CoordinateImpl(const IR* logical_plan) override;
  Status InitImpl(CompilerState* compiler_state,
                  const distributedpb::DistributedState& distributed_state) override;
  Status ProcessConfigImpl(const CarnotInfo& carnot_info) override;

 private:
  const distributedpb::CarnotInfo& GetRemoteProcessor() const;
  bool HasExecutableNodes(const IR* plan);

  // Nodes that have a source of data.
  std::vector<CarnotInfo> data_store_nodes_;
  // Nodes that remotely prcoess data.
  std::vector<CarnotInfo> remote_processor_nodes_;
  // The distributed state object.
  const distributedpb::DistributedState* distributed_state_ = nullptr;
  // The compiler state.
  CompilerState* compiler_state_ = nullptr;
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

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
