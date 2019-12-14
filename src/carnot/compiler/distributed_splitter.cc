#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/compiler/distributed_splitter.h"
namespace pl {
namespace carnot {
namespace compiler {
namespace distributed {

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
  DCHECK(parent_op->IsRelationInit()) << parent_op->DebugString();
  IR* graph = parent_op->graph_ptr();

  PL_ASSIGN_OR_RETURN(GRPCSourceGroupIR * grpc_source_group,
                      graph->CreateNode<GRPCSourceGroupIR>(parent_op->ast_node(), grpc_id_counter_,
                                                           parent_op->relation()));
  PL_ASSIGN_OR_RETURN(
      GRPCSinkIR * grpc_sink,
      graph->CreateNode<GRPCSinkIR>(parent_op->ast_node(), parent_op, grpc_id_counter_));
  PL_RETURN_IF_ERROR(grpc_sink->SetRelation(parent_op->relation()));

  PL_RETURN_IF_ERROR(child_op->ReplaceParent(parent_op, grpc_source_group));

  ++grpc_id_counter_;
  return Status::OK();
}

StatusOr<std::unique_ptr<IR>> DistributedSplitter::ApplyGRPCBridgeRule(const IR* logical_plan) {
  BlockingOperatorGRPCBridgeRule grpc_bridge_rule;
  PL_ASSIGN_OR_RETURN(auto grpc_bridge_plan, logical_plan->Clone());
  PL_ASSIGN_OR_RETURN(bool execute_result, grpc_bridge_rule.Execute(grpc_bridge_plan.get()));
  if (!execute_result) {
    return error::InvalidArgument("Could not find a blocking node in the graph.");
  }
  return grpc_bridge_plan;
}

BlockingSplitNodeIDGroups DistributedSplitter::GetBlockingSplitGroupsFromIR(const IR* graph) {
  BlockingSplitNodeIDGroups node_groups;
  auto independent_node_ids = graph->dag().IndependentGraphs();
  for (auto& node_set : independent_node_ids) {
    bool is_after_blocking = false;
    for (auto node : node_set) {
      IRNode* ir_node = graph->Get(node);
      if (Match(ir_node, BlockingOperator()) && !Match(ir_node, GRPCSink())) {
        is_after_blocking = true;
        break;
      }
    }
    if (is_after_blocking) {
      node_groups.after_blocking_nodes.merge(node_set);
    } else {
      node_groups.before_blocking_nodes.merge(node_set);
    }
  }
  DCHECK_EQ(node_groups.before_blocking_nodes.size() + node_groups.after_blocking_nodes.size(),
            graph->dag().nodes().size());
  return node_groups;
}

StatusOr<std::unique_ptr<BlockingSplitPlan>> DistributedSplitter::SplitAtBlockingNode(
    const IR* logical_plan) {
  PL_ASSIGN_OR_RETURN(std::unique_ptr<IR> grpc_bridge_plan, ApplyGRPCBridgeRule(logical_plan));

  BlockingSplitNodeIDGroups nodes = GetBlockingSplitGroupsFromIR(grpc_bridge_plan.get());

  PL_ASSIGN_OR_RETURN(std::unique_ptr<IR> before_blocking_plan, grpc_bridge_plan->Clone());
  auto after_blocking_plan = std::move(grpc_bridge_plan);

  // Before blocking plan should remove the after blocking set and vice versa.
  PL_RETURN_IF_ERROR(before_blocking_plan->Prune(nodes.after_blocking_nodes));
  PL_RETURN_IF_ERROR(after_blocking_plan->Prune(nodes.before_blocking_nodes));

  auto split_plan = std::make_unique<BlockingSplitPlan>();
  split_plan->after_blocking = std::move(after_blocking_plan);
  split_plan->before_blocking = std::move(before_blocking_plan);
  return split_plan;
}
}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
