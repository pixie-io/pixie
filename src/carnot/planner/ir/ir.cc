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

#include <farmhash.h>
#include <queue>

#include "src/carnot/planner/ir/all_ir_nodes.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/pattern_match.h"
#include "src/shared/upid/upid.h"

namespace px {
namespace carnot {
namespace planner {

Status IR::AddEdge(int64_t from_node, int64_t to_node) {
  dag_.AddEdge(from_node, to_node);
  return Status::OK();
}

Status IR::AddEdge(const IRNode* from_node, const IRNode* to_node) {
  return AddEdge(from_node->id(), to_node->id());
}

bool IR::HasEdge(const IRNode* from_node, const IRNode* to_node) const {
  return HasEdge(from_node->id(), to_node->id());
}

bool IR::HasEdge(int64_t from_node, int64_t to_node) const {
  return dag_.HasEdge(from_node, to_node);
}

Status IR::DeleteEdge(int64_t from_node, int64_t to_node) {
  DCHECK(dag_.HasEdge(from_node, to_node))
      << absl::Substitute("No edge ($0, $1) exists. (from:'$2', to:'$3')", from_node, to_node,
                          HasNode(from_node) ? Get(from_node)->DebugString() : "DoesNotExist",
                          HasNode(to_node) ? Get(to_node)->DebugString() : "DoesNotExist");
  if (!dag_.HasEdge(from_node, to_node)) {
    return error::InvalidArgument("No edge ($0, $1) exists.", from_node, to_node);
  }
  dag_.DeleteEdge(from_node, to_node);
  return Status::OK();
}

Status IR::DeleteEdge(IRNode* from_node, IRNode* to_node) {
  return DeleteEdge(from_node->id(), to_node->id());
}

Status IR::DeleteOrphansInSubtree(int64_t id) {
  auto children = dag_.DependenciesOf(id);
  // If the node has valid parents, then don't delete it.
  auto parents = dag_.ParentsOf(id);
  if (parents.size()) {
    return Status::OK();
  }
  PX_RETURN_IF_ERROR(DeleteNode(id));
  for (auto child_id : children) {
    PX_RETURN_IF_ERROR(DeleteOrphansInSubtree(child_id));
  }
  return Status::OK();
}

Status IR::DeleteSubtree(int64_t id) {
  for (const auto& p : dag_.ParentsOf(id)) {
    dag_.DeleteEdge(p, id);
  }
  return DeleteOrphansInSubtree(id);
}

Status IR::DeleteNode(int64_t node) {
  if (!dag_.HasNode(node)) {
    return error::InvalidArgument("No node $0 exists in graph.", node);
  }
  dag_.DeleteNode(node);
  id_node_map_.erase(node);
  return Status::OK();
}

StatusOr<IRNode*> IR::MakeNodeWithType(IRNodeType node_type, int64_t new_node_id) {
  switch (node_type) {
#undef PX_CARNOT_IR_NODE
#define PX_CARNOT_IR_NODE(NAME) \
  case IRNodeType::k##NAME:     \
    return MakeNode<NAME##IR>(new_node_id);
#include "src/carnot/planner/ir/ir_nodes.inl"
#undef PX_CARNOT_IR_NODE

    case IRNodeType::kAny:
    case IRNodeType::number_of_types:
      break;
  }
  return error::Internal("Received unknown IRNode type");
}

std::string IR::DebugString() const {
  std::string debug_string = dag().DebugString() + "\n";
  for (auto const& a : id_node_map_) {
    debug_string += a.second->DebugString() + "\n";
  }
  return debug_string;
}

std::string IR::OperatorsDebugString() {
  std::string debug_string;
  for (int64_t i : dag().TopologicalSort()) {
    if (Match(Get(i), Operator())) {
      auto op = static_cast<OperatorIR*>(Get(i));
      std::string parents = absl::StrJoin(op->parents(), ",", [](std::string* out, OperatorIR* in) {
        absl::StrAppend(out, in->id());
      });

      debug_string += absl::Substitute("[$0] $1 \n", parents, op->DebugString());
    }
  }
  return debug_string;
}

std::vector<OperatorIR*> IR::GetSources() const {
  std::vector<OperatorIR*> operators;
  for (auto* node : FindNodesThatMatch(SourceOperator())) {
    operators.push_back(static_cast<OperatorIR*>(node));
  }
  return operators;
}

std::vector<absl::flat_hash_set<int64_t>> IR::IndependentGraphs() const {
  auto sources = GetSources();
  CHECK(!sources.empty()) << "no source operators found";

  std::vector<absl::flat_hash_set<int64_t>> subgraphs;
  absl::flat_hash_set<int64_t> visited;
  // We treat the graph as undirected (both parent -> child and child -> parent count as edges), and
  // do BFS from each source (skipping sources we have already visited). Each BFS yields one
  // connected component of the undirected graph, which is also a "weakly" connected
  // component in the directed graph. This is linear in the number of nodes since each node is only
  // visited once.
  for (auto s : sources) {
    if (visited.contains(s->id())) {
      continue;
    }
    absl::flat_hash_set<int64_t> current_subgraph;
    std::queue<OperatorIR*> q;
    q.push(s);
    while (!q.empty()) {
      OperatorIR* curr_node = q.front();
      q.pop();
      visited.insert(curr_node->id());
      current_subgraph.insert(curr_node->id());
      for (auto child : curr_node->Children()) {
        if (!visited.contains(child->id())) {
          q.push(child);
        }
      }
      for (auto parent : curr_node->parents()) {
        if (!visited.contains(parent->id())) {
          q.push(parent);
        }
      }
    }
    subgraphs.push_back(current_subgraph);
  }
  return subgraphs;
}

StatusOr<std::unique_ptr<IR>> IR::Clone() const {
  auto new_ir = std::make_unique<IR>();
  absl::flat_hash_set<int64_t> nodes{dag().nodes().begin(), dag().nodes().end()};
  PX_RETURN_IF_ERROR(new_ir->CopySelectedNodesAndDeps(this, nodes));
  // TODO(philkuz) check to make sure these are the same.
  new_ir->dag_ = dag_;
  return new_ir;
}

Status IR::CopySelectedNodesAndDeps(const IR* src,
                                    const absl::flat_hash_set<int64_t>& selected_nodes) {
  absl::flat_hash_map<const IRNode*, IRNode*> copied_nodes_map;
  // Need to perform the copies in topological sort order to ensure the edges can be successfully
  // added.
  for (int64_t i : src->dag().TopologicalSort()) {
    if (!selected_nodes.contains(i)) {
      continue;
    }
    IRNode* node = src->Get(i);
    if (HasNode(i) && Get(i)->type() == node->type()) {
      continue;
    }
    PX_ASSIGN_OR_RETURN(IRNode * new_node, CopyNode(node, &copied_nodes_map));
    if (new_node->IsOperator()) {
      PX_RETURN_IF_ERROR(static_cast<OperatorIR*>(new_node)->CopyParentsFrom(
          static_cast<const OperatorIR*>(node)));
    }
  }
  return Status::OK();
}

Status IR::CopyOperatorSubgraph(const IR* src,
                                const absl::flat_hash_set<OperatorIR*>& selected_ops) {
  absl::flat_hash_set<int64_t> op_ids;
  for (auto op : selected_ops) {
    op_ids.insert(op->id());
  }
  return CopySelectedNodesAndDeps(src, op_ids);
}

StatusOr<planpb::Plan> IR::ToProto() const { return ToProto(0); }

IRNode* IR::Get(int64_t id) const {
  auto iterator = id_node_map_.find(id);
  if (iterator == id_node_map_.end()) {
    return nullptr;
  }
  return iterator->second.get();
}

StatusOr<planpb::Plan> IR::ToProto(int64_t agent_id) const {
  auto plan = planpb::Plan();

  auto plan_dag = plan.mutable_dag();
  auto plan_dag_node = plan_dag->add_nodes();
  plan_dag_node->set_id(1);

  auto plan_fragment = plan.add_nodes();
  plan_fragment->set_id(1);

  absl::flat_hash_set<int64_t> non_op_nodes;
  auto operators = dag().TopologicalSort();
  for (const auto& node_id : operators) {
    auto node = Get(node_id);
    if (node->IsOperator()) {
      PX_RETURN_IF_ERROR(OutputProto(plan_fragment, static_cast<OperatorIR*>(node), agent_id));
    } else {
      non_op_nodes.emplace(node_id);
    }
  }
  dag_.ToProto(plan_fragment->mutable_dag(), non_op_nodes);
  return plan;
}

Status IR::OutputProto(planpb::PlanFragment* pf, const OperatorIR* op_node,
                       int64_t agent_id) const {
  // Check to make sure that the type is resolved for this op_node, otherwise it's not connected to
  // a Sink.
  CHECK(op_node->is_type_resolved())
      << absl::Substitute("$0 doesn't have a resolved_type.", op_node->DebugString());

  // Add PlanNode.
  auto plan_node = pf->add_nodes();
  plan_node->set_id(op_node->id());
  auto op_pb = plan_node->mutable_op();
  // Special case for GRPCSinks.
  if (Match(op_node, GRPCSink())) {
    const GRPCSinkIR* grpc_sink = static_cast<const GRPCSinkIR*>(op_node);
    if (grpc_sink->has_output_table()) {
      return grpc_sink->ToProto(op_pb);
    }
    return grpc_sink->ToProto(op_pb, agent_id);
  }

  return op_node->ToProto(op_pb);
}

Status IR::Prune(const absl::flat_hash_set<int64_t>& ids_to_prune) {
  for (auto node : ids_to_prune) {
    for (auto child : dag_.DependenciesOf(node)) {
      PX_RETURN_IF_ERROR(DeleteEdge(node, child));
    }
    for (auto parent : dag_.ParentsOf(node)) {
      PX_RETURN_IF_ERROR(DeleteEdge(parent, node));
    }
    PX_RETURN_IF_ERROR(DeleteNode(node));
  }
  return Status::OK();
}

Status IR::Keep(const absl::flat_hash_set<int64_t>& ids_to_keep) {
  absl::flat_hash_set<int64_t> ids_to_prune;
  for (int64_t n : dag().nodes()) {
    if (ids_to_keep.contains(n)) {
      continue;
    }
    ids_to_prune.insert(n);
  }
  return Prune(ids_to_prune);
}

std::vector<IRNode*> IR::FindNodesOfType(IRNodeType type) const {
  std::vector<IRNode*> nodes;
  for (int64_t i : dag().nodes()) {
    IRNode* node = Get(i);
    if (node->type() == type) {
      nodes.push_back(node);
    }
  }
  return nodes;
}

Status ResolveOperatorType(OperatorIR* op, CompilerState* compiler_state) {
  if (op->is_type_resolved()) {
    return Status::OK();
  }
  op->PullParentTypes();
  PX_RETURN_IF_ERROR(op->UpdateOpAfterParentTypesResolved());
  switch (op->type()) {
#undef PX_CARNOT_IR_NODE
#define PX_CARNOT_IR_NODE(NAME) \
  case IRNodeType::k##NAME:     \
    return OperatorTraits<NAME##IR>::ResolveType(static_cast<NAME##IR*>(op), compiler_state);
#include "src/carnot/planner/ir/operators.inl"
#undef PX_CARNOT_IR_NODE
    default:
      return error::Internal(
          absl::Substitute("cannot resolve operator type for non-operator: $0", op->type_string()));
  }
}

Status CheckTypeCast(ExpressionIR* expr, std::shared_ptr<ValueType> from_type,
                     std::shared_ptr<ValueType> to_type) {
  if (from_type->data_type() != to_type->data_type()) {
    return expr->CreateIRNodeError(
        "Cannot cast from '$0' to '$1'. Only semantic type casts are allowed.",
        types::ToString(from_type->data_type()), types::ToString(to_type->data_type()));
  }
  return Status::OK();
}

Status ResolveExpressionType(ExpressionIR* expr, CompilerState* compiler_state,
                             const std::vector<TypePtr>& parent_types) {
  if (expr->is_type_resolved()) {
    return Status::OK();
  }
  Status status;
  switch (expr->type()) {
#undef PX_CARNOT_IR_NODE
#define PX_CARNOT_IR_NODE(NAME)                                                                    \
  case IRNodeType::k##NAME:                                                                        \
    status = ExpressionTraits<NAME##IR>::ResolveType(static_cast<NAME##IR*>(expr), compiler_state, \
                                                     parent_types);                                \
    break;
#include "src/carnot/planner/ir/expressions.inl"
#undef PX_CARNOT_IR_NODE
    default:
      return error::Internal(absl::Substitute(
          "cannot resolve expression type for non-expression: $0", expr->type_string()));
  }
  if (!status.ok() || !expr->HasTypeCast()) {
    return status;
  }
  PX_RETURN_IF_ERROR(CheckTypeCast(expr, std::static_pointer_cast<ValueType>(expr->resolved_type()),
                                   expr->type_cast()));
  return expr->SetResolvedType(expr->type_cast());
}

Status PropagateTypeChangesFromNode(IR* graph, IRNode* node, CompilerState* compiler_state) {
  // First, if node is not an Operator, we gather all its parent Operators (including Operators that
  // are parents of parent expressions) into `clear_all_deps_nodes`. All of the expressions along
  // the way also have their types cleared, so that they can be re-resolved. For example, if `node`
  // is a function nested inside another function nested inside a Map, then the Map operator will be
  // added to `clear_all_deps_nodes` and both of the functions will have their types cleared so that
  // they can be re-resolved.
  std::queue<IRNode*> clear_all_deps_nodes;
  std::queue<IRNode*> op_search_nodes;
  op_search_nodes.push(node);
  while (!op_search_nodes.empty()) {
    auto curr_node = op_search_nodes.front();
    op_search_nodes.pop();
    if (curr_node->IsExpression()) {
      curr_node->ClearResolvedType();
      for (auto parent_node : graph->dag().ParentsOf(curr_node->id())) {
        op_search_nodes.push(graph->Get(parent_node));
      }
    } else {
      clear_all_deps_nodes.push(curr_node);
    }
  }

  // All dependencies (ops and exprs) of the nodes in `clear_all_deps_nodes` have their types
  // cleared, and all of the depenedent Operator's are added to `ops_to_resolve`.
  absl::flat_hash_set<int64_t> ops_to_resolve;
  while (!clear_all_deps_nodes.empty()) {
    auto curr_node = clear_all_deps_nodes.front();
    clear_all_deps_nodes.pop();
    curr_node->ClearResolvedType();
    if (curr_node->IsOperator()) {
      ops_to_resolve.insert(curr_node->id());
    }
    for (auto child_id : graph->dag().DependenciesOf(curr_node->id())) {
      clear_all_deps_nodes.push(graph->Get(child_id));
    }
  }

  // The ops in `ops_to_resolve` need to be ordered topologically, because type resolution expects
  // parent nodes to be resolved before children.
  for (auto op_id : graph->dag().TopologicalSort()) {
    if (!ops_to_resolve.contains(op_id)) {
      continue;
    }
    auto op = static_cast<OperatorIR*>(graph->Get(op_id));
    PX_RETURN_IF_ERROR(ResolveOperatorType(op, compiler_state));
  }
  return Status::OK();
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
