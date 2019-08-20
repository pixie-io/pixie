#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "src/carnot/compiler/physical_splitter.h"
namespace pl {
namespace carnot {
namespace compiler {
namespace physical {

StatusOr<bool> BlockingOperatorGRPCBridgeRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, MemorySource())) {
    return InsertGRPCBridgeForBlockingChildOperator(static_cast<OperatorIR*>(ir_node));
  }
  return false;
}

StatusOr<bool> BlockingOperatorGRPCBridgeRule::InsertGRPCBridgeForBlockingChildOperator(
    OperatorIR* op) {
  bool does_change = false;
  for (OperatorIR* child_op : op->Children()) {
    if (Match(child_op, BlockingOperator())) {
      PL_RETURN_IF_ERROR(AddNewGRPCNodes(op, child_op));
      does_change = true;
    } else {
      PL_ASSIGN_OR_RETURN(bool child_does_change,
                          InsertGRPCBridgeForBlockingChildOperator(child_op));
      does_change |= child_does_change;
    }
  }
  return does_change;
}

Status BlockingOperatorGRPCBridgeRule::AddNewGRPCNodes(OperatorIR* parent_op,
                                                       OperatorIR* child_op) {
  DCHECK(parent_op->IsRelationInit());
  IR* graph = parent_op->graph_ptr();

  PL_ASSIGN_OR_RETURN(GRPCSourceGroupIR * grpc_source_group, graph->MakeNode<GRPCSourceGroupIR>());
  PL_RETURN_IF_ERROR(
      grpc_source_group->Init(grpc_id_counter_, parent_op->relation(), parent_op->ast_node()));
  PL_ASSIGN_OR_RETURN(GRPCSinkIR * grpc_sink, graph->MakeNode<GRPCSinkIR>());
  PL_RETURN_IF_ERROR(grpc_sink->Init(parent_op, grpc_id_counter_, parent_op->ast_node()));

  PL_RETURN_IF_ERROR(child_op->ReplaceParent(parent_op, grpc_source_group));

  ++grpc_id_counter_;
  return Status::OK();
}
}  // namespace physical
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
