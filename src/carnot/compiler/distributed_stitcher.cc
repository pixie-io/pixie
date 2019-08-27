#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "src/carnot/compiler/distributed_stitcher.h"
#include "src/carnot/compiler/grpc_source_conversion.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace distributed {

StatusOr<std::unique_ptr<Stitcher>> Stitcher::Create(CompilerState* compiler_state) {
  return std::unique_ptr<Stitcher>(new Stitcher(compiler_state));
}

Status Stitcher::Stitch(DistributedPlan* plan) {
  PL_RETURN_IF_ERROR(PrepareDistributedPlan(plan));
  PL_RETURN_IF_ERROR(AssociateEdges(plan));
  return FinalizePlan(plan);
}

Status Stitcher::PrepareDistributedPlan(DistributedPlan* plan) {
  for (int64_t node : plan->dag().TopologicalSort()) {
    auto carnot = plan->Get(node);
    PL_RETURN_IF_ERROR(SetSourceGroupGRPCAddress(carnot));
  }
  return Status::OK();
}

Status Stitcher::AssociateEdges(DistributedPlan* plan) {
  for (int64_t from : plan->dag().TopologicalSort()) {
    for (int64_t to : plan->dag().DependenciesOf(from)) {
      auto from_carnot = plan->Get(from);
      auto to_carnot = plan->Get(to);
      PL_RETURN_IF_ERROR(ConnectGraphs(from_carnot->plan(), to_carnot->plan()));
    }
  }
  return Status::OK();
}

Status Stitcher::FinalizePlan(DistributedPlan* plan) {
  for (int64_t node : plan->dag().TopologicalSort()) {
    auto carnot = plan->Get(node);
    PL_RETURN_IF_ERROR(FinalizeGraph(carnot->plan()));
  }
  return Status::OK();
}

Status Stitcher::SetSourceGroupGRPCAddress(CarnotInstance* carnot_instance) {
  SetSourceGroupGRPCAddressRule rule(carnot_instance->carnot_info().grpc_address(),
                                     carnot_instance->carnot_info().query_broker_address());
  PL_ASSIGN_OR_RETURN(auto updated, rule.Execute(carnot_instance->plan()));
  // TODO(philkuz) determine whether we should check if something is updated or not.
  PL_UNUSED(updated);
  return Status::OK();
}

Status Stitcher::ConnectGraphs(IR* from_graph, IR* to_graph) {
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

  for (const auto& [bridge_id, sink] : bridge_id_to_sink_map) {
    auto source_map_iter = bridge_id_to_source_map.find(bridge_id);
    DCHECK(source_map_iter != bridge_id_to_source_map.end());
    GRPCSourceGroupIR* source_group = source_map_iter->second;
    PL_RETURN_IF_ERROR(source_group->AddGRPCSink(sink));
  }

  // TODO(philkuz) handle issues where source_map has keys that aren't called.
  // Really just take the time to figure it all out.
  return Status::OK();
}

Status Stitcher::FinalizeGraph(IR* graph) {
  // TODO(philkuz) compiler_state will be used for pruning the graph later on.
  PL_UNUSED(compiler_state_);
  // TODO(philkuz) make a rule executor for finalizing the physical plan.
  GRPCSourceGroupConversionRule rule;
  PL_ASSIGN_OR_RETURN(bool result, rule.Execute(graph));
  PL_UNUSED(result);
  // TODO(philkuz) do something with the result here.
  // TODO(philkuz) do something to check for non-physical nodes.
  return Status::OK();
}

StatusOr<bool> SetSourceGroupGRPCAddressRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, GRPCSourceGroup())) {
    static_cast<GRPCSourceGroupIR*>(ir_node)->SetGRPCAddress(grpc_address_);
    return true;
  } else if (Match(ir_node, GRPCSink())) {
    static_cast<GRPCSinkIR*>(ir_node)->SetDistributedID(query_broker_address_);
  }

  return false;
}
}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
