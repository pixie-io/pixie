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
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/planner/distributed/distributed_stitcher_rules.h"
#include "src/carnot/planner/ir/grpc_sink_ir.h"
#include "src/carnot/planner/ir/grpc_source_group_ir.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

StatusOr<bool> SetSourceGroupGRPCAddressRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, GRPCSourceGroup())) {
    static_cast<GRPCSourceGroupIR*>(ir_node)->SetGRPCAddress(grpc_address_);
    static_cast<GRPCSourceGroupIR*>(ir_node)->SetSSLTargetName(ssl_targetname_);
    return true;
  }
  return false;
}

StatusOr<bool> AssociateDistributedPlanEdgesRule::Apply(CarnotInstance* from_carnot_instance) {
  bool did_connect_any_graph = false;
  for (int64_t to_carnot_instance_id :
       from_carnot_instance->distributed_plan()->dag().DependenciesOf(from_carnot_instance->id())) {
    CarnotInstance* to_carnot_instance =
        from_carnot_instance->distributed_plan()->Get(to_carnot_instance_id);
    PX_ASSIGN_OR_RETURN(bool did_connect_this_graph,
                        ConnectGraphs(from_carnot_instance->plan(), {from_carnot_instance->id()},
                                      to_carnot_instance->plan()));
    did_connect_any_graph |= did_connect_this_graph;
  }
  // Make sure we can connect to self.
  PX_ASSIGN_OR_RETURN(bool did_connect_graph_to_self,
                      ConnectGraphs(from_carnot_instance->plan(), {from_carnot_instance->id()},
                                    from_carnot_instance->plan()));
  did_connect_any_graph |= did_connect_graph_to_self;
  return did_connect_any_graph;
}

StatusOr<bool> AssociateDistributedPlanEdgesRule::ConnectGraphs(
    IR* from_graph, const absl::flat_hash_set<int64_t>& from_agents, IR* to_graph) {
  // In this, we find the bridge ids that overlap between the from_graph and to_graph.
  // 1. We get a mapping of bridge id to grpc sink in the from_graph
  // 2. We iterate through the GRPCSourceGroups, if any are connected to existing sinks, we add them
  // to the bridge list with the sink.
  // 3. We connect the bridges we find between the two of them.
  absl::flat_hash_map<int64_t, GRPCSinkIR*> bridge_id_to_grpc_sink;
  std::vector<std::pair<GRPCSourceGroupIR*, GRPCSinkIR*>> grpc_bridges;
  for (IRNode* ir_node : from_graph->FindNodesOfType(IRNodeType::kGRPCSink)) {
    DCHECK(Match(ir_node, GRPCSink()));
    auto sink = static_cast<GRPCSinkIR*>(ir_node);
    bridge_id_to_grpc_sink[sink->destination_id()] = sink;
  }

  // Make a map of from's source_ids to : GRPC source group ids.
  for (IRNode* ir_node : to_graph->FindNodesOfType(IRNodeType::kGRPCSourceGroup)) {
    DCHECK(Match(ir_node, GRPCSourceGroup()));
    auto source = static_cast<GRPCSourceGroupIR*>(ir_node);
    // Only add sources that have a matching grpc sink, otherwise the bridge is for another plan.
    if (!bridge_id_to_grpc_sink.contains(source->source_id())) {
      continue;
    }
    grpc_bridges.push_back({source, bridge_id_to_grpc_sink[source->source_id()]});
  }

  bool did_connect_graph = false;
  for (const auto& [source_group, sink] : grpc_bridges) {
    PX_RETURN_IF_ERROR(source_group->AddGRPCSink(sink, from_agents));
    did_connect_graph = true;
  }

  return did_connect_graph;
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
