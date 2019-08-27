#include <memory>
#include <string>
#include <vector>

#include "src/carnot/compiler/grpc_source_conversion.h"
#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/rules.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace physical {

StatusOr<bool> GRPCSourceGroupConversionRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, GRPCSourceGroup())) {
    return ExpandGRPCSourceGroup(static_cast<GRPCSourceGroupIR*>(ir_node));
  }
  return false;
}

StatusOr<GRPCSourceIR*> GRPCSourceGroupConversionRule::CreateGRPCSource(
    GRPCSourceGroupIR* group_ir, const std::string& remote_id) {
  DCHECK(group_ir->IsRelationInit());
  IR* graph = group_ir->graph_ptr();
  PL_ASSIGN_OR_RETURN(GRPCSourceIR * grpc_source, graph->MakeNode<GRPCSourceIR>());
  PL_RETURN_IF_ERROR(grpc_source->Init(remote_id, group_ir->relation(), group_ir->ast_node()));
  return grpc_source;
}

StatusOr<OperatorIR*> GRPCSourceGroupConversionRule::ConvertGRPCSourceGroup(
    GRPCSourceGroupIR* group_ir) {
  auto group_ids = group_ir->remote_string_ids();
  if (group_ids.size() == 0) {
    return group_ir->CreateIRNodeError("$0 must be affiliated with remote sinks.",
                                       group_ir->DebugString());
  }
  // IF only one GRPCSource, no need for a union.
  if (group_ids.size() == 1) {
    return CreateGRPCSource(group_ir, group_ids[0]);
  }
  // Create the GRPCSources, then a union.
  std::vector<OperatorIR*> grpc_sources;
  for (const auto& remote_id : group_ids) {
    PL_ASSIGN_OR_RETURN(GRPCSourceIR * new_grpc_source, CreateGRPCSource(group_ir, remote_id));
    grpc_sources.push_back(new_grpc_source);
  }
  IR* graph = group_ir->graph_ptr();
  PL_ASSIGN_OR_RETURN(UnionIR * union_op, graph->MakeNode<UnionIR>());
  PL_RETURN_IF_ERROR(union_op->Init(grpc_sources, {{}}, group_ir->ast_node()));
  PL_RETURN_IF_ERROR(union_op->SetRelationFromParents());
  return union_op;
}

StatusOr<bool> GRPCSourceGroupConversionRule::ExpandGRPCSourceGroup(GRPCSourceGroupIR* group_ir) {
  // Get the new parent.
  PL_ASSIGN_OR_RETURN(OperatorIR * new_parent, ConvertGRPCSourceGroup(group_ir));
  for (const auto child : group_ir->Children()) {
    // Replace the child node's parent with the new parent.
    PL_RETURN_IF_ERROR(child->ReplaceParent(group_ir, new_parent));
  }
  IR* graph = group_ir->graph_ptr();
  // Remove the old group_ir from the graph.
  PL_RETURN_IF_ERROR(graph->DeleteNode(group_ir->id()));
  return true;
}
Status GRPCSourceGroupConversionRule::RemoveGRPCSourceGroup(
    GRPCSourceGroupIR* grpc_source_group) const {
  PL_UNUSED(grpc_source_group);
  return Status::OK();
}

}  // namespace physical
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
