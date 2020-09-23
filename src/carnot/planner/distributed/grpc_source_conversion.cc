#include <memory>
#include <string>
#include <vector>

#include "src/carnot/planner/distributed/grpc_source_conversion.h"
#include "src/carnot/planner/ir/ir_nodes.h"
#include "src/carnot/planner/rules/rules.h"

namespace pl {
namespace carnot {
namespace planner {
namespace distributed {
using table_store::schema::Relation;

StatusOr<bool> GRPCSourceGroupConversionRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, GRPCSourceGroup())) {
    return ExpandGRPCSourceGroup(static_cast<GRPCSourceGroupIR*>(ir_node));
  }
  return false;
}

StatusOr<bool> GRPCSourceGroupConversionRule::ExpandGRPCSourceGroup(GRPCSourceGroupIR* group_ir) {
  // Get the new parent.
  PL_ASSIGN_OR_RETURN(OperatorIR * new_parent, ConvertGRPCSourceGroup(group_ir));
  for (const auto child : group_ir->Children()) {
    // Replace the child node's parent with the new parent.
    PL_RETURN_IF_ERROR(child->ReplaceParent(group_ir, new_parent));
  }
  IR* graph = group_ir->graph();
  // Remove the old group_ir from the graph.
  PL_RETURN_IF_ERROR(graph->DeleteNode(group_ir->id()));
  return true;
}

StatusOr<GRPCSourceIR*> GRPCSourceGroupConversionRule::CreateGRPCSource(
    GRPCSourceGroupIR* group_ir) {
  DCHECK(group_ir->IsRelationInit());
  IR* graph = group_ir->graph();
  return graph->CreateNode<GRPCSourceIR>(group_ir->ast(), group_ir->relation());
}

// Have to get rid of this function. Instead, need to associate (agent_id, sink_id) ->
// source_id/destination_id.
Status UpdateSink(GRPCSourceIR* source, GRPCSinkIR* sink, int64_t agent_id) {
  sink->AddDestinationIDMap(source->id(), agent_id);
  return Status::OK();
}

StatusOr<OperatorIR*> GRPCSourceGroupConversionRule::ConvertGRPCSourceGroup(
    GRPCSourceGroupIR* group_ir) {
  auto ir_graph = group_ir->graph();
  auto sinks = group_ir->dependent_sinks();

  if (sinks.size() == 0) {
    PL_ASSIGN_OR_RETURN(EmptySourceIR * empty_source,
                        ir_graph->CreateNode<EmptySourceIR>(group_ir->ast(), group_ir->relation()));
    return empty_source;
  }

  // Don't add an unnecessary union node if there is only one sink.
  if (sinks.size() == 1 && sinks[0].second.size() == 1) {
    PL_ASSIGN_OR_RETURN(auto new_grpc_source, CreateGRPCSource(group_ir));
    PL_RETURN_IF_ERROR(UpdateSink(new_grpc_source, sinks[0].first, *(sinks[0].second.begin())));
    return new_grpc_source;
  }

  std::vector<OperatorIR*> grpc_sources;
  for (const auto& sink : sinks) {
    DCHECK_GE(sinks[0].second.size(), 1);
    for (int64_t agent_id : sink.second) {
      PL_ASSIGN_OR_RETURN(GRPCSourceIR * new_grpc_source, CreateGRPCSource(group_ir));
      PL_RETURN_IF_ERROR(UpdateSink(new_grpc_source, sink.first, agent_id));
      grpc_sources.push_back(new_grpc_source);
    }
  }

  PL_ASSIGN_OR_RETURN(UnionIR * union_op,
                      ir_graph->CreateNode<UnionIR>(group_ir->ast(), grpc_sources));
  PL_RETURN_IF_ERROR(union_op->SetRelationFromParents());
  DCHECK(union_op->HasColumnMappings());
  return union_op;
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
