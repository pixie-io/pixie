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
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/planner/compiler_state/registry_info.h"
#include "src/carnot/planner/distributedpb/distributed_plan.pb.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/pattern_match.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/uuid/uuid.h"
#include "src/shared/metadata/metadata_filter.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

// Forward declare here so that CarnotInstance has access.
class DistributedPlan;

/**
 * @brief Object that represents a physical entity that uses the Carnot stream engine.
 * Contains the current plan on the node as well as physical information about the node.
 *
 */
class CarnotInstance {
 public:
  static StatusOr<std::unique_ptr<CarnotInstance>> Create(
      int64_t id, const distributedpb::CarnotInfo& carnot_info, DistributedPlan* parent_plan);

  const std::string& QueryBrokerAddress() const { return carnot_info_.query_broker_address(); }
  int64_t id() const { return id_; }

  void AddPlan(IR* plan) { plan_ = plan; }

  StatusOr<planpb::Plan> PlanProto() const { return plan_->ToProto(id_); }

  const distributedpb::CarnotInfo& carnot_info() const { return carnot_info_; }

  IR* plan() const { return plan_; }
  DistributedPlan* distributed_plan() const { return distributed_plan_; }

  std::string DebugString() const {
    return absl::Substitute("Carnot(id=$0, qb_address=$1)", id(), QueryBrokerAddress());
  }

  md::AgentMetadataFilter* metadata_filter() { return md_filter_.get(); }

 private:
  CarnotInstance(int64_t id, const distributedpb::CarnotInfo& carnot_info,
                 DistributedPlan* parent_plan, std::unique_ptr<md::AgentMetadataFilter> md_filter)
      : id_(id),
        carnot_info_(carnot_info),
        distributed_plan_(parent_plan),
        md_filter_(std::move(md_filter)) {}

  // The id used by the physical plan to define the DAG.
  int64_t id_;
  // The specification of this carnot instance.
  distributedpb::CarnotInfo carnot_info_;
  IR* plan_;
  // The distributed plan that this instance belongs to.
  DistributedPlan* distributed_plan_;
  // A filter containing the metadata entities stored on a particular Carnot.
  std::unique_ptr<md::AgentMetadataFilter> md_filter_ = nullptr;
};

// Note: this can be refactored to share a common base class with IR for shared
// operations like AddEdge, HasNode, etc.
class DistributedPlan {
 public:
  /**
   * @brief Adds a Carnot instance into the graph, and assigns a new id.
   *
   * @param carnot_instance the proto representation of the Carnot instance.
   * @return the id of the added carnot instance.
   */
  StatusOr<int64_t> AddCarnot(const distributedpb::CarnotInfo& carnot_instance);

  /**
   * @brief Gets the carnot instance at the index i.
   *
   * @param i: id to grab from the node map.
   * @return pointer to the Carnot instance at index i.
   */
  CarnotInstance* Get(int64_t i) const {
    auto id_node_iter = id_to_node_map_.find(i);
    CHECK(id_node_iter != id_to_node_map_.end()) << "Couldn't find index: " << i;
    return id_node_iter->second.get();
  }

  void AddEdge(CarnotInstance* from, CarnotInstance* to) { dag_.AddEdge(from->id(), to->id()); }
  void AddEdge(int64_t from, int64_t to) { dag_.AddEdge(from, to); }
  bool HasNode(int64_t node_id) const { return dag_.HasNode(node_id); }

  Status DeleteNode(int64_t node) {
    if (!HasNode(node)) {
      return error::InvalidArgument("No node $0 exists in graph.", node);
    }
    dag_.DeleteNode(node);
    return Status::OK();
  }

  StatusOr<distributedpb::DistributedPlan> ToProto() const;

  const plan::DAG& dag() const { return dag_; }

  void SetPlanOptions(planpb::PlanOptions plan_options) { plan_options_.CopyFrom(plan_options); }

  void AddPlan(std::unique_ptr<IR> plan) { plan_pool_.push_back(std::move(plan)); }
  const absl::flat_hash_map<sole::uuid, int64_t>& uuid_to_id_map() const { return uuid_to_id_map_; }

  std::vector<IR*> UniquePlans() {
    std::vector<IR*> plans;
    for (const auto& plan : plan_pool_) {
      plans.push_back(plan.get());
    }
    return plans;
  }

  void SetKelvin(CarnotInstance* kelvin) {
    DCHECK(id_to_node_map_.contains(kelvin->id()));
    CHECK(kelvin);
    kelvin_ = kelvin;
  }

  void SetExecutionCompleteAddress(const std::string& grpc_address,
                                   const std::string& ssl_targetname) {
    exec_complete_address_ = grpc_address;
    exec_complete_ssl_targetname_ = ssl_targetname;
  }

  void AddPlanToAgentMap(
      const absl::flat_hash_map<IR*, absl::flat_hash_set<int64_t>>& plan_to_agent_map) {
    plan_to_agent_map_ = std::move(plan_to_agent_map);
  }

  const absl::flat_hash_map<IR*, absl::flat_hash_set<int64_t>>& plan_to_agent_map() const {
    return plan_to_agent_map_;
  }

  CarnotInstance* kelvin() const { return kelvin_; }

 private:
  plan::DAG dag_;
  absl::flat_hash_map<int64_t, std::unique_ptr<CarnotInstance>> id_to_node_map_;
  absl::flat_hash_map<IR*, absl::flat_hash_set<int64_t>> plan_to_agent_map_;
  CarnotInstance* kelvin_ = nullptr;
  std::vector<std::unique_ptr<IR>> plan_pool_;
  absl::flat_hash_map<int64_t, IR*> agent_to_plan_map_;
  absl::flat_hash_map<sole::uuid, int64_t> uuid_to_id_map_;
  int64_t id_counter_ = 0;
  planpb::PlanOptions plan_options_;
  std::string exec_complete_address_;
  std::string exec_complete_ssl_targetname_;
};

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
