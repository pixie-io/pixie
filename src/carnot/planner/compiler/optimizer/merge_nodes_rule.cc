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

#include "src/carnot/planner/compiler/optimizer/merge_nodes_rule.h"
#include "src/carnot/planner/compiler/analyzer/resolve_types_rule.h"
#include "src/carnot/planner/ir/map_ir.h"
#include "src/carnot/planner/ir/operator_ir.h"

#include <algorithm>
#include <queue>

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

bool EqualStringVector(const std::vector<std::string>& a, const std::vector<std::string>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (int64_t i = 0; i < static_cast<int64_t>(a.size()); ++i) {
    if (a[i] != b[i]) {
      return false;
    }
  }
  return true;
}

/**
 * @brief CompareColumns makes sure the a and b vectors of columns have
 * contain the same elements, regardless of order.
 *
 * @param a
 * @param b
 * @return true
 * @return false
 */
bool CompareColumns(const std::vector<ColumnIR*>& a, const std::vector<ColumnIR*>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  absl::flat_hash_map<std::string, int64_t> a_col_names_to_count;
  absl::flat_hash_map<std::string, int64_t> b_col_names_to_count;
  for (auto col : a) {
    ++a_col_names_to_count[col->col_name()];
  }
  for (auto col : b) {
    ++b_col_names_to_count[col->col_name()];
  }

  // Make sure no names were missed.
  if (a_col_names_to_count.size() != b_col_names_to_count.size()) {
    return false;
  }
  for (const auto& [name, count] : a_col_names_to_count) {
    if (b_col_names_to_count[name] != count) {
      return false;
    }
  }

  return true;
}

/**
 * @brief Returns true if ColExpressionVectors are mergable.
 *
 * Two ColExpressionVectors are mergeable if for each ColExpressions with the same name have the
 * same expression. This also means if the size of the intesection of column names is 0, then Maps
 * can merge.
 *
 * @param exprs_a
 * @param exprs_b
 * @return true
 * @return false
 */
bool CompareExpressionLists(const ColExpressionVector& exprs_a,
                            const ColExpressionVector& exprs_b) {
  // TODO(philkuz) (PP-1812) Enable arbitrary expression list merging.
  if (exprs_a.size() != exprs_b.size()) {
    return false;
  }
  absl::flat_hash_map<std::string, ExpressionIR*> expr_map;
  for (const auto& e : exprs_a) {
    expr_map[e.name] = e.node;
  }
  DCHECK_EQ(expr_map.size(), exprs_a.size()) << "duplicate expr names passed in by exprs_a";

  absl::flat_hash_set<std::string> b_expr_names;
  for (const auto& e : exprs_b) {
    // Tracking expression names in b to make sure there are no duplicates.
    DCHECK(!b_expr_names.contains(e.name)) << "b_expr shouldn't contain duplicates " << e.name;
    b_expr_names.insert(e.name);
    // Each expression in a must also be in b.
    if (!expr_map.contains(e.name)) {
      return false;
    }
    if (!e.node->Equals(expr_map[e.name])) {
      return false;
    }
  }
  return true;
}

bool DoTimeIntervalsMerge(MemorySourceIR* src_a, MemorySourceIR* src_b) {
  if (src_a->IsTimeStartSet() != src_b->IsTimeStartSet()) {
    return false;
  }
  if (src_a->IsTimeStopSet() != src_b->IsTimeStopSet()) {
    return false;
  }
  bool can_merge = true;
  if (src_a->IsTimeStartSet()) {
    auto time_start_a = src_a->time_start_ns();
    auto time_start_b = src_b->time_start_ns();
    can_merge &= (time_start_a == time_start_b);
  }
  if (src_a->IsTimeStopSet()) {
    auto time_stop_a = src_a->time_stop_ns();
    auto time_stop_b = src_b->time_stop_ns();
    can_merge &= (time_stop_a == time_stop_b);
  }

  return can_merge;
}

bool MergeNodesRule::CanMerge(OperatorIR* a, OperatorIR* b) {
  if (a->type() != b->type()) {
    return false;
  }
  // Memory sinks should not match up until we change the API.
  // Do not remove this.
  if (Match(a, MemorySink())) {
    return false;
  } else if (Match(a, MemorySource())) {
    auto src_a = static_cast<MemorySourceIR*>(a);
    auto src_b = static_cast<MemorySourceIR*>(b);
    if (!DoTimeIntervalsMerge(src_a, src_b)) {
      return false;
    }

    return src_a->table_name() == src_b->table_name();
  } else if (Match(a, Map())) {
    auto map_a = static_cast<MapIR*>(a);
    auto map_b = static_cast<MapIR*>(b);
    return CompareExpressionLists(map_a->col_exprs(), map_b->col_exprs());
  } else if (Match(a, BlockingAgg())) {
    auto agg_a = static_cast<BlockingAggIR*>(a);
    auto agg_b = static_cast<BlockingAggIR*>(b);
    // Are the groups equal?
    if (!CompareColumns(agg_a->groups(), agg_b->groups())) {
      return false;
    }
    return CompareExpressionLists(agg_a->aggregate_expressions(), agg_b->aggregate_expressions());
  } else if (Match(a, Join())) {
    auto join_a = static_cast<JoinIR*>(a);
    auto join_b = static_cast<JoinIR*>(b);
    if (join_a->join_type() != join_b->join_type()) {
      return false;
    }
    if (!CompareColumns(join_a->output_columns(), join_b->output_columns())) {
      return false;
    }
    if (!CompareColumns(join_a->left_on_columns(), join_b->left_on_columns())) {
      return false;
    }
    if (!CompareColumns(join_a->right_on_columns(), join_b->right_on_columns())) {
      return false;
    }
    if (!EqualStringVector(join_a->suffix_strs(), join_b->suffix_strs())) {
      return false;
    }
    return true;
  } else if (Match(a, Filter())) {
    auto filter_a = static_cast<FilterIR*>(a);
    auto filter_b = static_cast<FilterIR*>(b);
    // Filter's output type mirrors its input parent type, so two filters can only be
    // merged if they share the same output type.
    DCHECK_EQ(1U, a->parents().size());
    DCHECK_EQ(1U, b->parents().size());
    return filter_a->filter_expr()->Equals(filter_b->filter_expr()) &&
           a->parents()[0]->resolved_table_type()->Equals(b->parents()[0]->resolved_table_type());
  } else if (Match(a, Limit())) {
    auto limit_a = static_cast<LimitIR*>(a);
    auto limit_b = static_cast<LimitIR*>(b);
    // Limit's output type mirrors its input parent type, so two limits can only be
    // merged if they share the same output type.
    DCHECK_EQ(1U, a->parents().size());
    DCHECK_EQ(1U, b->parents().size());
    return limit_a->limit_value() == limit_b->limit_value() &&
           a->parents()[0]->resolved_table_type()->Equals(b->parents()[0]->resolved_table_type());
  }
  VLOG(1) << "Can't match, so excluding from merge." << a->DebugString();
  return false;
}

std::vector<MatchingSet> MergeNodesRule::FindMatchingSets(
    const std::vector<OperatorIR*>& operators) {
  // The vector of sets that will eventually be output.
  std::vector<MatchingSet> matching_sets;
  // Data structure to track the operators that belong to matching sets.
  absl::flat_hash_set<OperatorIR*> has_matching_set;
  // For each operator, we compare it to all of the other operators.
  for (const auto& [i, base_op] : Enumerate(operators)) {
    // If the operator already has a matching set, that means all the operators that we would
    // compare to are either already in the same matching set or are not merge-able with this node,
    // given that merge-ability is transitive.
    if (has_matching_set.contains(base_op)) {
      continue;
    }
    has_matching_set.insert(base_op);

    // Create the new matching set.
    MatchingSet current_set{{base_op}};

    // Compare this operator to the remaining operators it has not been compared to yet.
    // For n ops, the idx 0 op compares to ops 1 -> n-1, the idx 1 op compares to 2 ->
    // n-1, etc
    for (auto it = operators.begin() + i + 1; it != operators.end(); ++it) {
      OperatorIR* comparison_op = *it;
      // Operators that belong to another set should not merge with base_op. If comparison_op was
      // merge-able base_op but belonged to another set, then base_op would already belong to that
      // other set as well and this inner loop would not happen.
      if (has_matching_set.contains(comparison_op)) {
        continue;
      }
      // Check whether the operators are merge-able. CanMerge must be transitive across all
      // operators.
      if (CanMerge(base_op, comparison_op)) {
        has_matching_set.insert(comparison_op);
        current_set.operators.push_back(comparison_op);
      }
    }
    matching_sets.push_back(current_set);
  }
  return matching_sets;
}

Status MergeExprs(ColExpressionVector* all_exprs,
                  absl::flat_hash_map<std::string, ExpressionIR*>* expr_map,
                  const ColExpressionVector& exprs_to_merge) {
  for (const auto& e : exprs_to_merge) {
    if (expr_map->contains(e.name)) {
      if (!e.node->Equals((*expr_map)[e.name])) {
        // TODO(philkuz) (PP-1812) figure out the merge strategy if the name matches but
        // expression is different.
        // TODO(philkuz) (PP-1812) figure out how to propagate the info about merges.
        return e.node->CreateIRNodeError("Can't support heterogenous expressions in maps.");
      }
    } else {
      (*expr_map)[e.name] = e.node;
      all_exprs->push_back(e);
    }
  }
  return Status::OK();
}

Status CheckParentsAreEqual(OperatorIR* base_op, OperatorIR* other_op) {
  if (other_op->parents()[0] != base_op->parents()[0]) {
    return other_op->CreateIRNodeError(
        "Expect op to have the same parent. $0 has $1 and $2 has $3", other_op->DebugString(),
        other_op->parents()[0]->DebugString(), base_op->DebugString(),
        base_op->parents()[0]->DebugString());
  }
  return Status::OK();
}

StatusOr<OperatorIR*> MergeNodesRule::MergeOps(IR* graph,
                                               const std::vector<OperatorIR*>& operators_to_merge) {
  // This function should receive >= 2 operators because groups of 1 should be
  // eliminated before hand.
  CHECK_GE(operators_to_merge.size(), 2UL);

  OperatorIR* base_op = operators_to_merge[0];
  OperatorIR* merged_op = nullptr;
  if (Match(base_op, MemorySource())) {
    auto base_src = static_cast<MemorySourceIR*>(base_op);
    PX_ASSIGN_OR_RETURN(auto new_src, graph->CopyNode(base_src));
    DCHECK(base_src->column_index_map_set());
    auto columns = new_src->column_names();
    auto column_idx_map = new_src->column_index_map();

    absl::flat_hash_set<std::string> column_names(columns.begin(), columns.end());
    for (OperatorIR* other_op : operators_to_merge) {
      MemorySourceIR* other_src = static_cast<MemorySourceIR*>(other_op);
      for (const auto& [idx, col] : Enumerate(other_src->column_names())) {
        if (column_names.contains(col)) {
          continue;
        }
        DCHECK(other_src->column_index_map_set());
        column_names.insert(col);
        columns.push_back(col);
        column_idx_map.push_back(other_src->column_index_map()[idx]);
      }
    }
    new_src->SetColumnNames(columns);
    new_src->SetColumnIndexMap(column_idx_map);
    // Memory sources are only merged if their start and end times are identical.
    // So the start and end times from the copied base src are already correct.
    merged_op = new_src;

  } else if (Match(base_op, Map())) {
    // TODO(philkuz) support Map operators where out_column_names conflict.
    auto base_map = static_cast<MapIR*>(base_op);
    DCHECK_EQ(base_map->parents().size(), 1UL) << "maps should only have one parent.";
    absl::flat_hash_map<std::string, ExpressionIR*> exprs;
    auto expr_list = base_map->col_exprs();
    for (const auto& e : base_map->col_exprs()) {
      exprs[e.name] = e.node;
    }
    for (OperatorIR* other_op : operators_to_merge) {
      DCHECK_EQ(other_op->parents().size(), 1UL);
      PX_RETURN_IF_ERROR(CheckParentsAreEqual(other_op, base_op));

      auto other_map = static_cast<MapIR*>(other_op);
      PX_RETURN_IF_ERROR(MergeExprs(&expr_list, &exprs, other_map->col_exprs()));
    }
    PX_ASSIGN_OR_RETURN(merged_op,
                        graph->CreateNode<MapIR>(base_map->ast(), base_map->parents()[0], expr_list,
                                                 /* keep_input_columns */ false));
  } else if (Match(base_op, BlockingAgg())) {
    // TODO(philkuz) support heterogenous Agg operators.
    auto base_agg = static_cast<BlockingAggIR*>(base_op);
    DCHECK_EQ(base_agg->parents().size(), 1Ul);
    absl::flat_hash_map<std::string, ExpressionIR*> exprs;
    auto expr_list = base_agg->aggregate_expressions();
    for (const auto& e : base_agg->aggregate_expressions()) {
      exprs[e.name] = e.node;
    }
    for (OperatorIR* other_op : operators_to_merge) {
      DCHECK_EQ(other_op->parents().size(), 1UL);
      PX_RETURN_IF_ERROR(CheckParentsAreEqual(other_op, base_op));
      auto other_agg = static_cast<BlockingAggIR*>(other_op);
      PX_RETURN_IF_ERROR(MergeExprs(&expr_list, &exprs, other_agg->aggregate_expressions()));
    }

    PX_ASSIGN_OR_RETURN(merged_op,
                        graph->CreateNode<BlockingAggIR>(base_agg->ast(), base_agg->parents()[0],
                                                         base_agg->groups(), expr_list));

  } else if (Match(base_op, Join())) {
    auto join = static_cast<JoinIR*>(base_op);
    PX_ASSIGN_OR_RETURN(JoinIR * new_join, graph->CopyNode(join));
    PX_RETURN_IF_ERROR(new_join->CopyParentsFrom(join));
    merged_op = new_join;
  } else if (Match(base_op, Filter())) {
    FilterIR* filter = static_cast<FilterIR*>(base_op);
    PX_ASSIGN_OR_RETURN(FilterIR * new_filter, graph->CopyNode(filter));
    PX_RETURN_IF_ERROR(new_filter->CopyParentsFrom(filter));
    merged_op = new_filter;
  } else if (Match(base_op, Limit())) {
    LimitIR* limit = static_cast<LimitIR*>(base_op);
    PX_ASSIGN_OR_RETURN(LimitIR * new_limit, graph->CopyNode(limit));
    PX_RETURN_IF_ERROR(new_limit->CopyParentsFrom(limit));
    merged_op = new_limit;
  } else {
    return base_op->CreateIRNodeError("Can't optimize $0", base_op->DebugString());
  }
  DCHECK_NE(merged_op, nullptr);
  merged_op->ClearResolvedType();
  ResolveTypesRule rule(compiler_state_);
  PX_ASSIGN_OR_RETURN(bool did_apply, rule.Apply(merged_op));
  DCHECK(did_apply) << merged_op->DebugString();
  return merged_op;
}

StatusOr<MapIR*> MakeNoOpMap(IR* graph, CompilerState* compiler_state, OperatorIR* op_to_copy) {
  PX_ASSIGN_OR_RETURN(MapIR * map,
                      graph->CreateNode<MapIR>(op_to_copy->ast(), op_to_copy, ColExpressionVector{},
                                               /* keep_input_columns */ true));

  ResolveTypesRule rule(compiler_state);
  PX_ASSIGN_OR_RETURN(bool did_change, rule.Apply(map));
  DCHECK(did_change);
  return map;
}

Status ReplaceOpsWithMerged(CompilerState* compiler_state, OperatorIR* merged,
                            const std::vector<OperatorIR*>& operators_to_merge) {
  absl::flat_hash_map<OperatorIR*, std::vector<OperatorIR*>> child_to_parent_map;
  for (OperatorIR* o : operators_to_merge) {
    for (OperatorIR* c : o->Children()) {
      child_to_parent_map[c].push_back(o);
    }
  }
  for (const auto& [child, parents] : child_to_parent_map) {
    DCHECK_GE(parents.size(), 1UL);
    // TODO(philkuz) (PP-1812) need to figure out how we would propagate information about
    // renamed output columns to the children. We'll need to add some data structure
    // that describes the transformation of o -> merged.
    PX_RETURN_IF_ERROR(child->ReplaceParent(parents[0], merged));
    // Insert a no-op map for each following parent that is repeated.
    for (int64_t i = 1; i < static_cast<int64_t>(parents.size()); ++i) {
      PX_ASSIGN_OR_RETURN(MapIR * map, MakeNoOpMap(parents[i]->graph(), compiler_state, merged));
      PX_RETURN_IF_ERROR(child->ReplaceParent(parents[i], map));
    }
  }
  return Status::OK();
}

Status RemoveMergedOps(IR* graph, const std::vector<OperatorIR*>& ops_to_merge) {
  for (auto op : ops_to_merge) {
    if (op->Children().size() != 0) {
      return op->CreateIRNodeError("'$0' Still has children.", op->DebugString());
    }
    PX_RETURN_IF_ERROR(graph->DeleteNode(op->id()));
  }
  return Status::OK();
}

/**
 * @brief Returns whether the operator's parents has been observed in the set.
 *
 *
 * @param observed_ops the set of operator ids that have been observed.
 * @param op the operator whose parents we want to check.
 * @return true if operator's parents are found in the set.
 * @return false otherwise.
 */
bool ParentsHaveBeenObserved(const absl::flat_hash_set<int64_t>& observed_ops, OperatorIR* op) {
  for (OperatorIR* p : op->parents()) {
    if (!observed_ops.contains(p->id())) {
      return false;
    }
  }
  return true;
}

StatusOr<bool> MergeNodesRule::Execute(IR* graph) {
  bool did_merge = false;
  // Start at the sources as all copies must start at the sources.
  std::vector<OperatorIR*> srcs;
  for (IRNode* src : graph->FindNodesThatMatch(MemorySource())) {
    srcs.push_back(static_cast<MemorySourceIR*>(src));
  }

  // matching_set_q is the queue of matching sets to merge together.
  std::queue<MatchingSet> matching_set_q;
  for (const auto& s : FindMatchingSets(srcs)) {
    matching_set_q.push(s);
  }

  // The set of operators that have been merged. Used to make sure we only look at multi-parent
  // operators once all of their parents have been observed.
  absl::flat_hash_set<int64_t> observed_ops;
  while (!matching_set_q.empty()) {
    auto ms = matching_set_q.front();
    matching_set_q.pop();

    // Create the merged operators.
    std::vector<OperatorIR*> all_children;
    // Need to insert into observed_ops in a separate loop first specifically
    // to handle children that have multiple parents that are all in this set of
    // operators. IE a self-join.
    for (OperatorIR* m : ms.operators) {
      observed_ops.insert(m->id());
    }

    // Go through the operators and add children that can be observed.
    for (OperatorIR* m : ms.operators) {
      for (OperatorIR* c : m->Children()) {
        // Children should only be added when all parents have been observed.
        if (ParentsHaveBeenObserved(observed_ops, c)) {
          all_children.push_back(c);
        }
      }
    }

    // Merge the operators together. If there's only one operator then we don't need to merge it.
    if (ms.operators.size() > 1) {
      did_merge = true;
      PX_ASSIGN_OR_RETURN(OperatorIR * merged_op, MergeOps(graph, ms.operators));
      PX_RETURN_IF_ERROR(ReplaceOpsWithMerged(compiler_state_, merged_op, ms.operators));
      PX_RETURN_IF_ERROR(RemoveMergedOps(graph, ms.operators));
      observed_ops.insert(merged_op->id());
    }

    // Take the children, find the matching sets and then insert them back into the queue.
    for (const auto& s : FindMatchingSets(all_children)) {
      matching_set_q.push(s);
    }
  }
  return did_merge;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
