#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "src/carnot/compiler/distributed_stitcher.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace distributed {

StatusOr<bool> SetSourceGroupGRPCAddressRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, GRPCSourceGroup())) {
    static_cast<GRPCSourceGroupIR*>(ir_node)->SetGRPCAddress(grpc_address_);
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
    PL_ASSIGN_OR_RETURN(bool did_connect_this_graph,
                        ConnectGraphs(from_carnot_instance->plan(), to_carnot_instance->plan()));
    did_connect_any_graph |= did_connect_this_graph;
  }
  return did_connect_any_graph;
}

StatusOr<bool> AssociateDistributedPlanEdgesRule::ConnectGraphs(IR* from_graph, IR* to_graph) {
  // Make a map of from's destination_ids to : GRPC sink ids
  absl::flat_hash_map<int64_t, GRPCSinkIR*> bridge_id_to_sink_map;
  for (int64_t i : from_graph->dag().TopologicalSort()) {
    IRNode* ir_node = from_graph->Get(i);
    if (Match(ir_node, GRPCSink())) {
      auto sink = static_cast<GRPCSinkIR*>(ir_node);
      bridge_id_to_sink_map[sink->destination_id()] = sink;
    }
  }

  // Make a map of from's source_ids to : GRPC source group ids
  absl::flat_hash_map<int64_t, GRPCSourceGroupIR*> bridge_id_to_source_map;
  for (int64_t i : to_graph->dag().TopologicalSort()) {
    IRNode* ir_node = to_graph->Get(i);
    if (Match(ir_node, GRPCSourceGroup())) {
      auto source = static_cast<GRPCSourceGroupIR*>(ir_node);
      bridge_id_to_source_map[source->source_id()] = source;
    }
  }
  bool did_connect_graph = false;
  for (const auto& [bridge_id, sink] : bridge_id_to_sink_map) {
    auto source_map_iter = bridge_id_to_source_map.find(bridge_id);
    DCHECK(source_map_iter != bridge_id_to_source_map.end());
    GRPCSourceGroupIR* source_group = source_map_iter->second;
    PL_RETURN_IF_ERROR(source_group->AddGRPCSink(sink));
    did_connect_graph = true;
  }

  // TODO(philkuz) handle issues where source_map has keys that aren't called.
  // Really just take the time to figure it all out.
  return did_connect_graph;
}
}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
