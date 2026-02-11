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

#include "src/carnot/planner/distributed/distributed_plan/distributed_plan.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

StatusOr<distributedpb::DistributedPlan> DistributedPlan::ToProto() const {
  distributedpb::DistributedPlan physical_plan_pb;
  auto physical_plan_dag = physical_plan_pb.mutable_dag();
  auto qb_address_to_plan_pb = physical_plan_pb.mutable_qb_address_to_plan();
  auto qb_address_to_dag_id_pb = physical_plan_pb.mutable_qb_address_to_dag_id();

  for (int64_t i : dag_.TopologicalSort()) {
    CarnotInstance* carnot = Get(i);
    CHECK_EQ(carnot->id(), i) << absl::Substitute("Index in node ($1) and DAG ($0) don't agree.", i,
                                                  carnot->id());
    DCHECK(carnot->plan()) << absl::Substitute("$0 doesn't have a plan set.",
                                               carnot->DebugString());
    PX_ASSIGN_OR_RETURN(auto plan_proto, carnot->PlanProto());
    for (int64_t parent_i : dag_.ParentsOf(i)) {
      *(plan_proto.add_incoming_agent_ids()) = Get(parent_i)->carnot_info().agent_id();
    }
    for (int64_t child_i : dag_.DependenciesOf(i)) {
      auto dest = plan_proto.add_execution_status_destinations();
      const auto& carnot_info = Get(child_i)->carnot_info();
      dest->set_grpc_address(carnot_info.grpc_address());
      dest->set_ssl_targetname(carnot_info.ssl_targetname());
    }
    if (!plan_proto.execution_status_destinations_size()) {
      auto dest = plan_proto.add_execution_status_destinations();
      dest->set_grpc_address(exec_complete_address_);
      dest->set_ssl_targetname(exec_complete_ssl_targetname_);
    }
    (*qb_address_to_plan_pb)[carnot->QueryBrokerAddress()] = plan_proto;
    (*qb_address_to_dag_id_pb)[carnot->QueryBrokerAddress()] = i;

    auto plan_opts = (*qb_address_to_plan_pb)[carnot->QueryBrokerAddress()].mutable_plan_options();
    plan_opts->CopyFrom(plan_options_);
  }
  dag_.ToProto(physical_plan_dag);
  return physical_plan_pb;
}

StatusOr<int64_t> DistributedPlan::AddCarnot(const distributedpb::CarnotInfo& carnot_info) {
  int64_t carnot_id = id_counter_;
  ++id_counter_;
  PX_ASSIGN_OR_RETURN(auto instance, CarnotInstance::Create(carnot_id, carnot_info, this));
  id_to_node_map_.emplace(carnot_id, std::move(instance));
  PX_ASSIGN_OR_RETURN(sole::uuid uuid, ParseUUID(carnot_info.agent_id()));
  uuid_to_id_map_[uuid] = carnot_id;
  dag_.AddNode(carnot_id);
  return carnot_id;
}

StatusOr<std::unique_ptr<CarnotInstance>> CarnotInstance::Create(
    int64_t id, const distributedpb::CarnotInfo& carnot_info, DistributedPlan* parent_plan) {
  if (carnot_info.has_metadata_info()) {
    PX_ASSIGN_OR_RETURN(auto bf, md::AgentMetadataFilter::FromProto(carnot_info.metadata_info()));
    return std::unique_ptr<CarnotInstance>(
        new CarnotInstance(id, carnot_info, parent_plan, std::move(bf)));
  }
  return std::unique_ptr<CarnotInstance>(new CarnotInstance(id, carnot_info, parent_plan, nullptr));
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
