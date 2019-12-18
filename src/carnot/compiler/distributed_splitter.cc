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

BlockingSplitNodeIDGroups DistributedSplitter::GetSplitGroups(
    const IR* graph, const absl::flat_hash_map<int64_t, bool>& on_kelvin) {
  BlockingSplitNodeIDGroups node_groups;
  auto independent_node_ids = graph->dag().IndependentGraphs();
  for (auto& node_set : independent_node_ids) {
    bool is_after_blocking = false;
    for (auto node : node_set) {
      IRNode* ir_node = graph->Get(node);
      if (ir_node->IsOperator()) {
        auto on_kelvin_iter = on_kelvin.find(ir_node->id());
        if (on_kelvin_iter != on_kelvin.end() && on_kelvin_iter->second) {
          is_after_blocking = true;
          break;
        }
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

bool DistributedSplitter::ExecutesOnDataStores(const udfspb::UDTFSourceExecutor& executor) {
  return executor == udfspb::UDTF_ALL_AGENTS || executor == udfspb::UDTF_ALL_PEM ||
         executor == udfspb::UDTF_SUBSET_PEM;
}

bool DistributedSplitter::ExecutesOnRemoteProcessors(const udfspb::UDTFSourceExecutor& executor) {
  return executor == udfspb::UDTF_ALL_AGENTS || executor == udfspb::UDTF_ALL_KELVIN ||
         executor == udfspb::UDTF_ONE_KELVIN || executor == udfspb::UDTF_SUBSET_KELVIN;
}

bool DistributedSplitter::RunsOnDataStores(const std::vector<OperatorIR*> sources) {
  for (auto* s : sources) {
    if (Match(s, UDTFSource())) {
      auto udtf_op = static_cast<UDTFSourceIR*>(s);

      if (ExecutesOnDataStores(udtf_op->udtf_spec().executor())) {
        return true;
      }
    }
    if (Match(s, MemorySource())) {
      return true;
    }
  }
  return false;
}

bool DistributedSplitter::RunsOnRemoteProcessors(const std::vector<OperatorIR*> sources) {
  for (auto* s : sources) {
    if (Match(s, UDTFSource())) {
      auto udtf_op = static_cast<UDTFSourceIR*>(s);

      if (ExecutesOnRemoteProcessors(udtf_op->udtf_spec().executor())) {
        return true;
      }
    }
    if (Match(s, MemorySource())) {
      return true;
    }
  }
  return false;
}

bool DistributedSplitter::IsSourceOnKelvin(OperatorIR* source_op) {
  DCHECK(source_op->is_source());
  if (!Match(source_op, UDTFSource())) {
    return false;
  }
  UDTFSourceIR* udtf = static_cast<UDTFSourceIR*>(source_op);
  const auto& spec = udtf->udtf_spec();
  // Fail in Debug w/ ALL_KELVIN, SUBSET_KELVIN, and ALL_AGENTS
  DCHECK(spec.executor() != udfspb::UDTF_ALL_KELVIN) << "All kelvin execution not supported";
  DCHECK(spec.executor() != udfspb::UDTF_ALL_AGENTS) << "All agents execution not supported";
  DCHECK(spec.executor() != udfspb::UDTF_SUBSET_KELVIN) << "Subset kelvin execution not supported";
  return ExecutesOnRemoteProcessors(spec.executor());
}

bool DistributedSplitter::IsChildOpOnKelvin(bool is_parent_on_kelvin, OperatorIR* child_op) {
  DCHECK(!child_op->is_source());
  // In the future we will add more complex logic here to determine if
  // we can actually run this on the Kelvin or not.
  return is_parent_on_kelvin || child_op->IsBlocking();
}

absl::flat_hash_map<int64_t, bool> DistributedSplitter::GetKelvinNodes(
    const std::vector<OperatorIR*>& sources) {
  absl::flat_hash_map<int64_t, bool> on_kelvin;
  // Loop through all of the sources and see if they must be located on Kelvin.
  std::queue<OperatorIR*> child_q;
  for (auto src_op : sources) {
    DCHECK(Match(src_op, SourceOperator()));
    on_kelvin[src_op->id()] = IsSourceOnKelvin(src_op);
    child_q.push(src_op);
  }

  while (!child_q.empty()) {
    // Look at the children of this op and updates them to be on Kelvin.
    OperatorIR* parent_op = child_q.front();
    child_q.pop();
    DCHECK(on_kelvin.contains(parent_op->id()));

    // Note we look at the child relative to _this_ parent.
    // Eventually the queue-loop will reach other parents.
    bool is_parent_on_kelvin = on_kelvin[parent_op->id()];

    for (OperatorIR* child_op : parent_op->Children()) {
      // If we've seen an operator before and it's already on kelvin, then all
      // of its children will already be on Kelvin and we can't change that.
      if (on_kelvin.contains(child_op->id()) && on_kelvin[child_op->id()]) {
        continue;
      }
      // A child is on kelvin if this parent is on Kelvin or if it requires multi-nodes of data.
      on_kelvin[child_op->id()] = IsChildOpOnKelvin(is_parent_on_kelvin, child_op);
      child_q.push(child_op);
    }
  }
  return on_kelvin;
}

StatusOr<std::unique_ptr<BlockingSplitPlan>> DistributedSplitter::SplitKelvinAndAgents(
    const IR* logical_plan) {
  // TODO(philkuz) redo the GetKelvinNodes and everything else to copy the plan before the
  // CreateGRPCBridge call.
  std::vector<OperatorIR*> sources = logical_plan->GetSources();
  absl::flat_hash_map<int64_t, bool> on_kelvin = GetKelvinNodes(sources);
  // Source_ids are necessary because we will make a clone of the plan at which point we will no
  // longer be able to use an IRNode vector. Addressing the above mentioned issue should make this
  // less confusing.
  std::vector<int64_t> source_ids;
  for (OperatorIR* src : sources) {
    source_ids.push_back(src->id());
  }
  PL_ASSIGN_OR_RETURN(std::unique_ptr<IR> grpc_bridge_plan,
                      CreateGRPCBridge(logical_plan, on_kelvin, source_ids));

  PL_ASSIGN_OR_RETURN(std::unique_ptr<IR> pem_plan, grpc_bridge_plan->Clone());
  auto kelvin_plan = std::move(grpc_bridge_plan);

  BlockingSplitNodeIDGroups nodes = GetSplitGroups(kelvin_plan.get(), on_kelvin);
  // Removes all of the nodes that are not on PEM.
  PL_RETURN_IF_ERROR(pem_plan->Prune(nodes.after_blocking_nodes));
  // Removes all of the nodes that are not on Kelvin.
  PL_RETURN_IF_ERROR(kelvin_plan->Prune(nodes.before_blocking_nodes));

  auto split_plan = std::make_unique<BlockingSplitPlan>();
  split_plan->after_blocking = std::move(kelvin_plan);
  split_plan->before_blocking = std::move(pem_plan);
  return split_plan;
}

absl::flat_hash_map<OperatorIR*, std::vector<OperatorIR*>> DistributedSplitter::GetEdgesToBreak(
    const IR* logical_plan, const absl::flat_hash_map<int64_t, bool>& on_kelvin,
    const std::vector<int64_t>& source_ids) {
  absl::flat_hash_map<OperatorIR*, std::vector<OperatorIR*>> edges_to_break;
  std::queue<OperatorIR*> child_q;
  for (auto src_id : source_ids) {
    DCHECK(Match(logical_plan->Get(src_id), SourceOperator()));
    OperatorIR* src = static_cast<OperatorIR*>(logical_plan->Get(src_id));
    child_q.push(src);
  }

  while (!child_q.empty()) {
    // Look at the children of this op and updates them to be on Kelvin.
    OperatorIR* parent_op = child_q.front();
    child_q.pop();

    // Note we look at the child relative to _this_ parent. Eventually the queue-loop will reach the
    // other parent.
    auto is_parent_on_kelvin_iter = on_kelvin.find(parent_op->id());
    DCHECK(is_parent_on_kelvin_iter != on_kelvin.end());
    bool is_parent_on_kelvin = is_parent_on_kelvin_iter->second;
    if (is_parent_on_kelvin) {
      continue;
    }

    for (OperatorIR* child_op : parent_op->Children()) {
      // Iterates through the children of this op and checks whether the edge can be broken and if
      // the parent already has an edge with a child that will be broken.
      auto is_child_on_kelvin_iter = on_kelvin.find(child_op->id());
      DCHECK(is_child_on_kelvin_iter != on_kelvin.end());
      bool is_child_on_kelvin = is_child_on_kelvin_iter->second;
      if (is_parent_on_kelvin != is_child_on_kelvin) {
        if (!edges_to_break.contains(parent_op)) {
          edges_to_break[parent_op] = {};
        }
        edges_to_break[parent_op].push_back(child_op);
      }
      // A child is on kelvin if this parent is on Kelvin or if it requires multi-nodes of data.
      child_q.push(child_op);
    }
  }
  return edges_to_break;
}

StatusOr<std::unique_ptr<IR>> DistributedSplitter::CreateGRPCBridge(
    const IR* logical_plan, const absl::flat_hash_map<int64_t, bool>& on_kelvin,
    const std::vector<int64_t>& sources) {
  PL_ASSIGN_OR_RETURN(std::unique_ptr<IR> grpc_bridge_plan, logical_plan->Clone());
  absl::flat_hash_map<OperatorIR*, std::vector<OperatorIR*>> edges_to_break =
      GetEdgesToBreak(grpc_bridge_plan.get(), on_kelvin, sources);
  int64_t grpc_id_counter = 0;
  // TODO(philkuz) enable for (PL-846)
  // for (const auto& [parent, children] : edges_to_break) {
  //   PL_ASSIGN_OR_RETURN(GRPCSinkIR * grpc_sink, CreateGRPCSink(parent, grpc_id_counter));
  //   PL_ASSIGN_OR_RETURN(GRPCSourceGroupIR * grpc_source_group,
  //                       CreateGRPCSourceGroup(parent, grpc_id_counter));
  //   // Go through the children and replace the parent with the new child.
  //   for (auto child : children) {
  //     PL_RETURN_IF_ERROR(child->ReplaceParent(parent, grpc_source_group));
  //   }

  //   ++grpc_id_counter;
  // }

  // TODO(philkuz) replace with the above when ready (846)
  for (const auto& [parent, children] : edges_to_break) {
    // Go through the children and replace the parent with the new child.
    for (auto child : children) {
      PL_ASSIGN_OR_RETURN(GRPCSinkIR * grpc_sink, CreateGRPCSink(parent, grpc_id_counter));
      PL_ASSIGN_OR_RETURN(GRPCSourceGroupIR * grpc_source_group,
                          CreateGRPCSourceGroup(parent, grpc_id_counter));
      PL_RETURN_IF_ERROR(child->ReplaceParent(parent, grpc_source_group));
      DCHECK_EQ(grpc_sink->destination_id(), grpc_source_group->source_id());
      ++grpc_id_counter;
    }
  }
  return grpc_bridge_plan;
}

StatusOr<GRPCSinkIR*> DistributedSplitter::CreateGRPCSink(OperatorIR* parent_op, int64_t grpc_id) {
  DCHECK(parent_op->IsRelationInit()) << parent_op->DebugString();
  IR* graph = parent_op->graph_ptr();

  PL_ASSIGN_OR_RETURN(GRPCSinkIR * grpc_sink,
                      graph->CreateNode<GRPCSinkIR>(parent_op->ast_node(), parent_op, grpc_id));
  PL_RETURN_IF_ERROR(grpc_sink->SetRelation(parent_op->relation()));
  return grpc_sink;
}

StatusOr<GRPCSourceGroupIR*> DistributedSplitter::CreateGRPCSourceGroup(OperatorIR* parent_op,
                                                                        int64_t grpc_id) {
  DCHECK(parent_op->IsRelationInit()) << parent_op->DebugString();
  IR* graph = parent_op->graph_ptr();

  PL_ASSIGN_OR_RETURN(
      GRPCSourceGroupIR * grpc_source_group,
      graph->CreateNode<GRPCSourceGroupIR>(parent_op->ast_node(), grpc_id, parent_op->relation()));
  return grpc_source_group;
}
}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
