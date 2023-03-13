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

#include "src/carnot/planner/distributed/distributed_rules.h"
#include "src/carnot/planner/distributed/splitter/presplit_analyzer/presplit_analyzer.h"
#include "src/carnot/planner/distributed/splitter/presplit_optimizer/presplit_optimizer.h"
#include "src/carnot/planner/distributed/splitter/scalar_udfs_run_on_executor_rule.h"
#include "src/carnot/planner/distributed/splitter/splitter.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

StatusOr<bool> OperatorMustRunOnKelvin(CompilerState* compiler_state, OperatorIR* op) {
  // If the operator can't run on a PEM, or is a blocking operator, we should
  // schedule this node to run on a Kelvin.
  PX_ASSIGN_OR_RETURN(bool runs_on_pem,
                      ScalarUDFsRunOnPEMRule::OperatorUDFsRunOnPEM(compiler_state, op));
  return !runs_on_pem || op->IsBlocking();
}

StatusOr<bool> OperatorCanRunOnPEM(CompilerState* compiler_state, OperatorIR* op) {
  // If the operator can't run on a Kelvin, and is not a blocking operator, we can
  // schedule this node to run on a PEM.
  PX_ASSIGN_OR_RETURN(bool runs_on_pem,
                      ScalarUDFsRunOnPEMRule::OperatorUDFsRunOnPEM(compiler_state, op));
  return runs_on_pem && !op->IsBlocking();
}

BlockingSplitNodeIDGroups Splitter::GetSplitGroups(
    const IR* graph, const absl::flat_hash_map<int64_t, bool>& on_kelvin) {
  BlockingSplitNodeIDGroups node_groups;
  auto independent_node_ids = graph->IndependentGraphs();
  // Here we see which node_sets belong to which side. Each node set will reside on the same
  // side of the execution.
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
  return node_groups;
}

StatusOr<absl::flat_hash_map<int64_t, bool>> Splitter::GetKelvinNodes(
    const IR* logical_plan, const std::vector<int64_t>& source_ids) {
  // TODO(philkuz)  update on_kelvin to actually be on_data_processor and flip bools around.
  absl::flat_hash_map<int64_t, bool> on_kelvin;
  // Loop through all of the sources and see if they must be located on Kelvin.
  std::queue<OperatorIR*> child_q;
  // Sources only run on data sources.
  for (auto src_id : source_ids) {
    DCHECK(Match(logical_plan->Get(src_id), SourceOperator()));
    // TODO(philkuz) Why don't we filter out UDTFs here that are Kelvin only?
    on_kelvin[src_id] = false;
    child_q.push(static_cast<OperatorIR*>(logical_plan->Get(src_id)));
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
      // A child is on kelvin if this parent is on Kelvin or if it is a node that must run on
      // Kelvin.
      PX_ASSIGN_OR_RETURN(bool node_on_kelvin, OperatorMustRunOnKelvin(compiler_state_, child_op));
      on_kelvin[child_op->id()] = is_parent_on_kelvin || node_on_kelvin;
      child_q.push(child_op);
    }
  }
  return on_kelvin;
}

StatusOr<std::unique_ptr<BlockingSplitPlan>> Splitter::SplitKelvinAndAgents(const IR* input_plan) {
  PX_ASSIGN_OR_RETURN(auto logical_plan, input_plan->Clone());

  // Run the pre-split analysis step.
  PX_ASSIGN_OR_RETURN(std::unique_ptr<PreSplitAnalyzer> analyzer,
                      PreSplitAnalyzer::Create(compiler_state_));
  PX_RETURN_IF_ERROR(analyzer->Execute(logical_plan.get()));
  // Run the pre-split optimization step.
  PX_ASSIGN_OR_RETURN(std::unique_ptr<PreSplitOptimizer> optimizer,
                      PreSplitOptimizer::Create(compiler_state_));
  PX_RETURN_IF_ERROR(optimizer->Execute(logical_plan.get()));

  // Source_ids are necessary because we will make a clone of the plan at which point we will no
  // longer be able to use IRNode pointers and only IDs will be valid.
  std::vector<int64_t> source_ids;
  for (OperatorIR* src : logical_plan->GetSources()) {
    source_ids.push_back(src->id());
  }

  PX_ASSIGN_OR_RETURN(auto on_kelvin, GetKelvinNodes(logical_plan.get(), source_ids));
  PX_ASSIGN_OR_RETURN(std::unique_ptr<IR> grpc_bridge_plan,
                      CreateGRPCBridgePlan(logical_plan.get(), on_kelvin, source_ids));

  PX_ASSIGN_OR_RETURN(std::unique_ptr<IR> pem_plan, grpc_bridge_plan->Clone());
  PX_ASSIGN_OR_RETURN(std::unique_ptr<IR> kelvin_plan, grpc_bridge_plan->Clone());

  BlockingSplitNodeIDGroups nodes = GetSplitGroups(kelvin_plan.get(), on_kelvin);
  // Removes all of the nodes that are not on PEM.
  PX_RETURN_IF_ERROR(pem_plan->Prune(nodes.after_blocking_nodes));
  // Removes all of the nodes that are not on Kelvin.
  PX_RETURN_IF_ERROR(kelvin_plan->Prune(nodes.before_blocking_nodes));

  // Will error out if a Kelvin-only UDF has been scheduled on the PEM portion of the plan.
  ScalarUDFsRunOnPEMRule pem_rule(compiler_state_);
  PX_RETURN_IF_ERROR(pem_rule.Execute(pem_plan.get()));

  // Will error out if a PEM-only UDF has been scheduled on the Kelvin portion of the plan.
  ScalarUDFsRunOnKelvinRule kelvin_rule(compiler_state_);
  PX_RETURN_IF_ERROR(kelvin_rule.Execute(kelvin_plan.get()));

  auto split_plan = std::make_unique<BlockingSplitPlan>();
  split_plan->after_blocking = std::move(kelvin_plan);
  split_plan->before_blocking = std::move(pem_plan);
  split_plan->original_plan = std::move(grpc_bridge_plan);
  return split_plan;
}

absl::flat_hash_set<OperatorIR*> GetSourcesOfOp(const IR* logical_plan, OperatorIR* op) {
  if (Match(op, SourceOperator())) {
    return {op};
  }
  absl::flat_hash_set<OperatorIR*> sources;
  for (const auto& p : op->parents()) {
    auto parent_srcs = GetSourcesOfOp(logical_plan, p);
    sources.insert(parent_srcs.begin(), parent_srcs.end());
  }
  return sources;
}

bool MatchSparseFilterExpr(ExpressionIR* expr) {
  auto logical_and = Match(expr, LogicalAnd(Value(), Value()));
  auto logical_or = Match(expr, LogicalOr(Value(), Value()));
  if (logical_and || logical_or) {
    auto func = static_cast<FuncIR*>(expr);
    auto lhs = MatchSparseFilterExpr(func->args()[0]);
    auto rhs = MatchSparseFilterExpr(func->args()[1]);
    return logical_and ? lhs && rhs : lhs || rhs;
  }

  return Match(expr, Equals(MetadataExpression(), String()));
}

bool SparseFilter(OperatorIR* op) {
  if (!Match(op, Filter())) {
    return false;
  }
  return MatchSparseFilterExpr(static_cast<FilterIR*>(op)->filter_expr());
}

bool MatchMetadataOrSubExpression(ExpressionIR* expr) {
  if (Match(expr, MetadataExpression())) {
    return true;
  } else if (Match(expr, Func())) {
    auto func = static_cast<FuncIR*>(expr);
    for (const auto& arg : func->args()) {
      if (MatchMetadataOrSubExpression(arg)) {
        return true;
      }
    }
  }
  return false;
}

bool MustBeOnPemMap(OperatorIR* op) {
  if (!Match(op, Map())) {
    return false;
  }
  MapIR* map = static_cast<MapIR*>(op);
  for (const auto& expr : map->col_exprs()) {
    if (MatchMetadataOrSubExpression(expr.node)) {
      return true;
    }
  }
  return false;
}

bool MustBeOnPemFilter(OperatorIR* op) {
  if (!Match(op, Filter())) {
    return false;
  }
  FilterIR* filter = static_cast<FilterIR*>(op);
  return MatchMetadataOrSubExpression(filter->filter_expr());
}

bool MustBeOnPem(OperatorIR* op) { return MustBeOnPemMap(op) || MustBeOnPemFilter(op); }

bool Splitter::CanBeGRPCBridgeTree(OperatorIR* op) {
  // We can generalize this beyond sparse filters in the future as needed.
  return SparseFilter(op);
}

void Splitter::ConstructGRPCBridgeTree(
    OperatorIR* op, GRPCBridgeTree* node,
    const absl::flat_hash_map<OperatorIR*, std::vector<OperatorIR*>>& grpc_bridges) {
  absl::flat_hash_set<OperatorIR*> ignore_children;
  if (grpc_bridges.contains(op)) {
    node->bridges.push_back(op);
    auto grpc_bridge_ends = grpc_bridges.find(op)->second;
    bool all_have_partial_mgr = AllHavePartialMgr(grpc_bridge_ends);
    node->all_bridges_partial_mgr &= all_have_partial_mgr;

    // Ignore the children of the GRPCBridge under the assumption that no such bridges
    // should exist. That will cause a bug otherwise and should be avoided in the creator
    // of the bridges.
    ignore_children.insert(grpc_bridge_ends.begin(), grpc_bridge_ends.end());
  }
  for (OperatorIR* child : op->Children()) {
    if (ignore_children.contains(child)) {
      if (MustBeOnPem(child)) {
        LOG(ERROR) << "must be on PEM, but not found: " << child->DebugString();
      }
      continue;
    }
    // Certain operators must be on PEM, ie because of metadatas locality. If an operator
    // is on a PEM then we must make sure we place it there, regardless of GRPCBridges.
    if (MustBeOnPem(child)) {
      GRPCBridgeTree child_bridge_node;
      child_bridge_node.must_be_on_pem = true;
      child_bridge_node.starting_op = child;
      ConstructGRPCBridgeTree(child, &child_bridge_node, grpc_bridges);
      node->AddChild(child_bridge_node);
      continue;
    }
    // Start a new tree if this child can optionally exist as its own separate GRPCBridge.
    // This branch should not be reach with the current setup because every thing that
    // returns true here MustBeOnPem.
    if (CanBeGRPCBridgeTree(child)) {
      DCHECK(false) << "StartNewGRPCBridgeTree should not be reached for now.";
      GRPCBridgeTree child_bridge_node;
      child_bridge_node.starting_op = child;
      ConstructGRPCBridgeTree(child, &child_bridge_node, grpc_bridges);
      node->AddChild(child_bridge_node);
      continue;
    }

    ConstructGRPCBridgeTree(child, node, grpc_bridges);
  }
}

Status Splitter::ExtractBridgesFromGRPCBridgeTree(
    const GRPCBridgeTree& bridge_node,
    absl::flat_hash_map<OperatorIR*, std::vector<OperatorIR*>>* new_grpc_bridges,
    const absl::flat_hash_map<OperatorIR*, std::vector<OperatorIR*>>& old_grpc_bridges) {
  // If any GRPC Bridge doesn't have a partial manager and none of the children GRPCBridgeTrees must
  // be on pem, we consolidate the bridges into a single one that's a mutual parent to all of the
  // GRPC Bridges as well as any children BridgeNodes.
  if (!bridge_node.all_bridges_partial_mgr && !bridge_node.HasChildrenThatMustBeOnPem()) {
    // We insert the bridge before the first blocking node or branching of the source
    // subgraph.
    auto op = bridge_node.starting_op;

    while (op->Children().size() == 1) {
      PX_ASSIGN_OR_RETURN(bool on_pem, OperatorCanRunOnPEM(compiler_state_, op->Children()[0]));
      if (!on_pem) {
        break;
      }
      op = op->Children()[0];
    }

    (*new_grpc_bridges)[op] = op->Children();
    return Status::OK();
  }

  // Otherwise, all of the original bridges assigned to this bridge_node are preserved.
  for (OperatorIR* start_op : bridge_node.bridges) {
    (*new_grpc_bridges)[start_op] = old_grpc_bridges.find(start_op)->second;
  }
  // And then run through the child bridge nodes, which will either have it's own partial operator
  // or will just create a grpc bridge around the blob.
  for (const auto& child_bridge_node : bridge_node.children) {
    PX_RETURN_IF_ERROR(
        ExtractBridgesFromGRPCBridgeTree(child_bridge_node, new_grpc_bridges, old_grpc_bridges));
  }
  return Status::OK();
}

StatusOr<absl::flat_hash_map<OperatorIR*, std::vector<OperatorIR*>>> Splitter::ConsolidateEdges(
    const IR* logical_plan,
    const absl::flat_hash_map<OperatorIR*, std::vector<OperatorIR*>>& grpc_bridges) {
  absl::flat_hash_map<OperatorIR*, std::vector<OperatorIR*>> new_grpc_bridges;
  for (const auto& src_raw : logical_plan->FindNodesThatMatch(SourceOperator())) {
    // Create a GRPCBridgeTree per src.
    OperatorIR* src = static_cast<OperatorIR*>(src_raw);
    GRPCBridgeTree bridge_tree;
    bridge_tree.starting_op = src;
    ConstructGRPCBridgeTree(src, &bridge_tree, grpc_bridges);
    // Consolidate GRPCBridges using the tree structure.
    PX_RETURN_IF_ERROR(
        ExtractBridgesFromGRPCBridgeTree(bridge_tree, &new_grpc_bridges, grpc_bridges));
  }

  return new_grpc_bridges;
}

absl::flat_hash_map<OperatorIR*, std::vector<OperatorIR*>> Splitter::GetEdgesToBreak(
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

    // If parent_op is on Kelvin, all of it's children should be as well, in which case
    // we can stop looking through the children.
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
        edges_to_break[parent_op].push_back(child_op);
      }
      // A child is on kelvin if this parent is on Kelvin or if it requires multi-nodes of data.
      child_q.push(child_op);
    }
  }
  return edges_to_break;
}

PartialOperatorMgr* Splitter::GetPartialOperatorMgr(OperatorIR* op) const {
  for (const auto& mgr : partial_operator_mgrs_) {
    if (mgr->Matches(op)) {
      return mgr.get();
    }
  }
  return nullptr;
}

bool Splitter::AllHavePartialMgr(std::vector<OperatorIR*> children) const {
  for (OperatorIR* c : children) {
    PartialOperatorMgr* mgr = GetPartialOperatorMgr(c);
    // mgr is nullptr if no mgr matches the Operator.
    if (!mgr) {
      return false;
    }
  }
  return true;
}

Status Splitter::InsertGRPCBridge(IR* plan, OperatorIR* parent,
                                  const std::vector<OperatorIR*>& blocking_children) {
  // Possible results of this execution:
  // 1. Some of the blocking_children do not have a partial implementation and we output 1 bridge.
  // 2. All of the blocking_children have partial implementations and we output one bridge per.
  // 3. All of the blocking nodes have partial implementations but the sum cost of GRPC Bridge per
  // operator is greater than outputting a single Bridge for the parent as in case #1 (NOTE: not
  // implemented yet).

  // Handle case where some of the blocking_children dont have partial implementations. This
  // assumes that network costs are greater than processing costs, so we just minimize network
  // costs here.
  if (!AllHavePartialMgr(blocking_children)) {
    PX_ASSIGN_OR_RETURN(GRPCSinkIR * grpc_sink, CreateGRPCSink(parent, grpc_id_counter_));
    PX_ASSIGN_OR_RETURN(GRPCSourceGroupIR * grpc_source_group,
                        CreateGRPCSourceGroup(parent, grpc_id_counter_));
    DCHECK_EQ(grpc_sink->destination_id(), grpc_source_group->source_id());
    // Go through the blocking_children and replace the parent with the new child.
    for (auto child : blocking_children) {
      PX_RETURN_IF_ERROR(child->ReplaceParent(parent, grpc_source_group));
    }
    ++grpc_id_counter_;
    return Status::OK();
  }

  // Handle case where each child has a partial implementation.
  for (OperatorIR* c : blocking_children) {
    // Create the prepare version of the operator.
    PartialOperatorMgr* mgr = GetPartialOperatorMgr(c);
    DCHECK(mgr) << "mgr not found for " << c->DebugString();
    PX_ASSIGN_OR_RETURN(OperatorIR * prepare_op, mgr->CreatePrepareOperator(plan, c));
    DCHECK(prepare_op->IsChildOf(parent)) << absl::Substitute(
        "'$0' is not a child of '$1'", prepare_op->DebugString(), parent->DebugString());

    // Create the GRPC Bridge from the prepare_op.
    PX_ASSIGN_OR_RETURN(GRPCSinkIR * grpc_sink, CreateGRPCSink(prepare_op, grpc_id_counter_));
    PX_ASSIGN_OR_RETURN(GRPCSourceGroupIR * grpc_source_group,
                        CreateGRPCSourceGroup(prepare_op, grpc_id_counter_));
    DCHECK_EQ(grpc_sink->destination_id(), grpc_source_group->source_id());

    // Create the finalize version of the operator, getting input from the grpc_source_group.
    PX_ASSIGN_OR_RETURN(OperatorIR * finalize_op,
                        mgr->CreateMergeOperator(plan, grpc_source_group, c));
    // Replace this child's children with the new parent.
    for (auto grandchild : c->Children()) {
      PX_RETURN_IF_ERROR(grandchild->ReplaceParent(c, finalize_op));
    }
    PX_RETURN_IF_ERROR(plan->DeleteNode(c->id()));
    ++grpc_id_counter_;
  }
  return Status::OK();
}

StatusOr<std::unique_ptr<IR>> Splitter::CreateGRPCBridgePlan(
    const IR* logical_plan, const absl::flat_hash_map<int64_t, bool>& on_kelvin,
    const std::vector<int64_t>& sources) {
  PX_ASSIGN_OR_RETURN(std::unique_ptr<IR> grpc_bridge_plan, logical_plan->Clone());
  absl::flat_hash_map<OperatorIR*, std::vector<OperatorIR*>> edges_to_break =
      GetEdgesToBreak(grpc_bridge_plan.get(), on_kelvin, sources);

  PX_ASSIGN_OR_RETURN(edges_to_break, ConsolidateEdges(grpc_bridge_plan.get(), edges_to_break));
  for (const auto& [parent, children] : edges_to_break) {
    PX_RETURN_IF_ERROR(InsertGRPCBridge(grpc_bridge_plan.get(), parent, children));
  }
  return grpc_bridge_plan;
}

StatusOr<GRPCSinkIR*> Splitter::CreateGRPCSink(OperatorIR* parent_op, int64_t grpc_id) {
  DCHECK(parent_op->is_type_resolved()) << parent_op->DebugString();
  IR* graph = parent_op->graph();

  PX_ASSIGN_OR_RETURN(GRPCSinkIR * grpc_sink,
                      graph->CreateNode<GRPCSinkIR>(parent_op->ast(), parent_op, grpc_id));
  PX_RETURN_IF_ERROR(grpc_sink->SetResolvedType(parent_op->resolved_type()));
  return grpc_sink;
}

StatusOr<GRPCSourceGroupIR*> Splitter::CreateGRPCSourceGroup(OperatorIR* parent_op,
                                                             int64_t grpc_id) {
  DCHECK(parent_op->is_type_resolved()) << parent_op->DebugString();
  IR* graph = parent_op->graph();

  PX_ASSIGN_OR_RETURN(
      GRPCSourceGroupIR * grpc_source_group,
      graph->CreateNode<GRPCSourceGroupIR>(parent_op->ast(), grpc_id, parent_op->resolved_type()));
  return grpc_source_group;
}
}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
