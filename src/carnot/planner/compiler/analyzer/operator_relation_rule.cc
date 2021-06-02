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

#include <string>

#include "src/carnot/planner/compiler/analyzer/data_type_rule.h"
#include "src/carnot/planner/compiler/analyzer/operator_relation_rule.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<bool> OperatorRelationRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, UnresolvedReadyOp(BlockingAgg()))) {
    return SetBlockingAgg(static_cast<BlockingAggIR*>(ir_node));
  }
  if (Match(ir_node, UnresolvedReadyOp(Map()))) {
    return SetMap(static_cast<MapIR*>(ir_node));
  }
  if (Match(ir_node, UnresolvedReadyOp(Union()))) {
    return SetUnion(static_cast<UnionIR*>(ir_node));
  }
  if (Match(ir_node, UnresolvedReadyOp(Join()))) {
    JoinIR* join_node = static_cast<JoinIR*>(ir_node);
    if (Match(ir_node, UnsetOutputColumnsJoin())) {
      PL_RETURN_IF_ERROR(SetJoinOutputColumns(join_node));
    }
    return SetOldJoin(join_node);
  }
  if (Match(ir_node, UnresolvedReadyOp(Drop()))) {
    // Another rule handles this.
    // TODO(philkuz) unify this rule with the drop to map rule.
    return false;
  }
  if (Match(ir_node, UnresolvedReadyOp(MemorySink()))) {
    return SetMemorySink(static_cast<MemorySinkIR*>(ir_node));
  }
  if (Match(ir_node, UnresolvedReadyOp(ExternalGRPCSink()))) {
    return SetGRPCSink(static_cast<GRPCSinkIR*>(ir_node));
  }
  if (Match(ir_node, UnresolvedReadyOp(Limit())) || Match(ir_node, UnresolvedReadyOp(Filter())) ||
      Match(ir_node, UnresolvedReadyOp(GroupBy())) ||
      Match(ir_node, UnresolvedReadyOp(Rolling()))) {
    // Explicitly match because the general matcher keeps causing problems.
    return SetOther(static_cast<OperatorIR*>(ir_node));
  }
  if (Match(ir_node, UnresolvedReadyOp())) {
    // Fails in this path because future writers should specify the op.
    DCHECK(false) << ir_node->DebugString();
    return SetOther(static_cast<OperatorIR*>(ir_node));
  }
  return false;
}

StatusOr<bool> OperatorRelationRule::SetOldJoin(JoinIR* join_node) const {
  DCHECK_EQ(join_node->parents().size(), 2UL);
  OperatorIR* left = join_node->parents()[0];
  OperatorIR* right = join_node->parents()[1];

  Relation left_relation = left->relation();
  Relation right_relation = right->relation();

  Relation out_relation;

  for (const auto& [col_idx, col] : Enumerate(join_node->output_columns())) {
    if (!col->IsDataTypeEvaluated()) {
      return false;
    }
    const std::string& new_col_name = join_node->column_names()[col_idx];
    Relation* col_relation;
    if (col->container_op_parent_idx() == 0) {
      col_relation = &left_relation;
    } else {
      col_relation = &right_relation;
    }
    out_relation.AddColumn(col_relation->GetColumnType(col->col_name()), new_col_name);
  }

  PL_RETURN_IF_ERROR(join_node->SetRelation(out_relation));
  return true;
}

bool UpdateColumn(ColumnIR* col_expr, Relation* relation_ptr) {
  if (!col_expr->IsDataTypeEvaluated()) {
    return false;
  }
  relation_ptr->AddColumn(col_expr->EvaluatedDataType(), col_expr->col_name());
  return true;
}

StatusOr<ColumnIR*> OperatorRelationRule::CreateOutputColumn(JoinIR* join_node,
                                                             const std::string& col_name,
                                                             int64_t parent_idx,
                                                             const Relation& relation) const {
  PL_ASSIGN_OR_RETURN(ColumnIR * col, join_node->graph()->CreateNode<ColumnIR>(
                                          join_node->ast(), col_name, parent_idx));
  PL_RETURN_IF_ERROR(DataTypeRule::EvaluateColumnFromRelation(col, relation));
  return col;
}

StatusOr<std::vector<ColumnIR*>> OperatorRelationRule::CreateOutputColumnIRNodes(
    JoinIR* join_node, const Relation& left_relation, const Relation& right_relation) const {
  int64_t left_idx = 0;
  int64_t right_idx = 1;
  if (join_node->specified_as_right()) {
    left_idx = 1;
    right_idx = 0;
  }
  std::vector<ColumnIR*> output_columns;
  output_columns.reserve(left_relation.NumColumns() + right_relation.NumColumns());
  for (const auto& col_name : left_relation.col_names()) {
    PL_ASSIGN_OR_RETURN(
        ColumnIR * col,
        CreateOutputColumn(join_node, col_name, /* parent_idx */ left_idx, left_relation));
    output_columns.push_back(col);
  }
  for (const auto& right_df_name : right_relation.col_names()) {
    PL_ASSIGN_OR_RETURN(ColumnIR * col,
                        CreateOutputColumn(join_node, right_df_name,
                                           /* parent_idx */ right_idx, right_relation));
    output_columns.push_back(col);
  }
  return output_columns;
}

Status OperatorRelationRule::SetJoinOutputColumns(JoinIR* join_node) const {
  DCHECK_EQ(join_node->parents().size(), 2UL);
  int64_t left_idx = 0;
  int64_t right_idx = 1;

  if (join_node->specified_as_right()) {
    left_idx = 1;
    right_idx = 0;
  }
  const Relation& left_relation = join_node->parents()[left_idx]->relation();
  const Relation& right_relation = join_node->parents()[right_idx]->relation();
  const std::string& left_suffix = join_node->suffix_strs()[left_idx];
  const std::string& right_suffix = join_node->suffix_strs()[right_idx];

  absl::flat_hash_set<std::string> columns_set(left_relation.col_names().begin(),
                                               left_relation.col_names().end());
  columns_set.reserve(left_relation.NumColumns() + right_relation.NumColumns());
  // The left relation should only have unique names.
  DCHECK_EQ(columns_set.size(), left_relation.NumColumns())
      << "Left relation has duplicate columns, should have caught this earlier.";

  std::vector<std::string> output_column_names(left_relation.col_names().begin(),
                                               left_relation.col_names().end());
  output_column_names.reserve(left_relation.NumColumns() + right_relation.NumColumns());

  absl::flat_hash_set<std::string> duplicate_column_names;
  for (const auto& right_df_name : right_relation.col_names()) {
    // Output columns are added to regardless, we replace both duplicated columns in the following
    // loop.
    output_column_names.push_back(right_df_name);

    if (columns_set.contains(right_df_name)) {
      duplicate_column_names.insert(right_df_name);
      continue;
    }
    columns_set.insert(right_df_name);
  }

  // Resolve any of the duplicates, check to see if there are duplicates afterwards
  for (const auto& dup_name : duplicate_column_names) {
    columns_set.erase(dup_name);
    std::string left_column = absl::Substitute("$0$1", dup_name, left_suffix);
    std::string right_column = absl::Substitute("$0$1", dup_name, right_suffix);

    std::string err_string = absl::Substitute(
        "duplicate column '$0' after merge. Change the specified suffixes ('$1','$2') to fix "
        "this",
        "$0", left_suffix, right_suffix);
    // Make sure that the new left_column doesn't already exist in the
    if (columns_set.contains(left_column)) {
      return join_node->CreateIRNodeError(err_string, left_column);
    }
    // Insert before checking right column to make sure left_column != right_column. Saves a check.
    columns_set.insert(left_column);
    if (columns_set.contains(right_column)) {
      return join_node->CreateIRNodeError(err_string, right_column);
    }
    columns_set.insert(right_column);
    ReplaceDuplicateNames(&output_column_names, dup_name, left_column, right_column);
  }

  // Assertion that columns are the same size as the sum of the columns.
  DCHECK_EQ(columns_set.size(), left_relation.NumColumns() + right_relation.NumColumns());
  PL_ASSIGN_OR_RETURN(auto output_columns,
                      CreateOutputColumnIRNodes(join_node, left_relation, right_relation));

  return join_node->SetOutputColumns(output_column_names, output_columns);
}

void OperatorRelationRule::ReplaceDuplicateNames(std::vector<std::string>* column_names,
                                                 const std::string& dup_name,
                                                 const std::string& left_column,
                                                 const std::string& right_column) const {
  bool left_found = false;
  bool right_found = false;
  // Iterate through the output column names and replace the two duplicate columns.
  for (const auto& [idx, str] : Enumerate(*column_names)) {
    if (str != dup_name) {
      continue;
    }
    // Left column should be found first.
    if (left_found) {
      (*column_names)[idx] = right_column;
      // When right column is found, then we exit the loop.
      right_found = true;
      break;
    }
    (*column_names)[idx] = left_column;
    left_found = true;
  }
  DCHECK(right_found);
}

StatusOr<bool> OperatorRelationRule::SetBlockingAgg(BlockingAggIR* agg_ir) const {
  Relation agg_rel;
  for (ColumnIR* group : agg_ir->groups()) {
    if (!UpdateColumn(group, &agg_rel)) {
      return false;
    }
  }
  ColExpressionVector col_exprs = agg_ir->aggregate_expressions();
  for (auto& entry : col_exprs) {
    std::string col_name = entry.name;
    if (!entry.node->IsDataTypeEvaluated()) {
      return false;
    }
    agg_rel.AddColumn(entry.node->EvaluatedDataType(), col_name);
  }

  PL_RETURN_IF_ERROR(agg_ir->SetRelation(agg_rel));
  return true;
}

StatusOr<bool> OperatorRelationRule::SetMap(MapIR* map_ir) const {
  DCHECK_EQ(map_ir->parents().size(), 1UL) << "There should be exactly one parent.";
  auto parent_relation = map_ir->parents()[0]->relation();
  const ColExpressionVector& expressions = map_ir->col_exprs();

  for (auto& entry : expressions) {
    if (!entry.node->IsDataTypeEvaluated()) {
      return false;
    }
  }

  if (map_ir->keep_input_columns()) {
    ColExpressionVector output_expressions;

    absl::flat_hash_set<std::string> new_columns;
    for (ColumnExpression expr : expressions) {
      new_columns.insert(expr.name);
    }

    for (const auto& input_col_name : parent_relation.col_names()) {
      // If this column is being overwritten with a new expression, skip it here.
      if (new_columns.contains(input_col_name)) {
        continue;
      }
      // Otherwise, bring over the column from the previous relation.
      PL_ASSIGN_OR_RETURN(ColumnIR * col_ir,
                          map_ir->graph()->CreateNode<ColumnIR>(map_ir->ast(), input_col_name,
                                                                0 /*parent_op_idx*/));
      col_ir->ResolveColumnType(parent_relation);
      output_expressions.push_back(ColumnExpression(input_col_name, col_ir));
    }

    for (const ColumnExpression& expr : expressions) {
      output_expressions.push_back(expr);
    }

    map_ir->set_keep_input_columns(false);
    PL_RETURN_IF_ERROR(map_ir->SetColExprs(output_expressions));
  }

  PL_RETURN_IF_ERROR(map_ir->SetRelationFromExprs());
  return true;
}

StatusOr<bool> OperatorRelationRule::SetUnion(UnionIR* union_ir) const {
  PL_RETURN_IF_ERROR(union_ir->SetRelationFromParents());
  return true;
}

StatusOr<bool> OperatorRelationRule::SetMemorySink(MemorySinkIR* sink_ir) const {
  if (!sink_ir->out_columns().size()) {
    return SetOther(sink_ir);
  }
  auto input_relation = sink_ir->parents()[0]->relation();
  Relation output_relation;
  for (const auto& col_name : sink_ir->out_columns()) {
    output_relation.AddColumn(input_relation.GetColumnType(col_name), col_name,
                              input_relation.GetColumnSemanticType(col_name),
                              input_relation.GetColumnDesc(col_name));
  }
  PL_RETURN_IF_ERROR(sink_ir->SetRelation(output_relation));
  return true;
}

StatusOr<bool> OperatorRelationRule::SetGRPCSink(GRPCSinkIR* sink_ir) const {
  DCHECK(sink_ir->has_output_table());
  if (!sink_ir->out_columns().size()) {
    return SetOther(sink_ir);
  }
  auto input_relation = sink_ir->parents()[0]->relation();
  Relation output_relation;
  for (const auto& col_name : sink_ir->out_columns()) {
    output_relation.AddColumn(input_relation.GetColumnType(col_name), col_name,
                              input_relation.GetColumnSemanticType(col_name),
                              input_relation.GetColumnDesc(col_name));
  }
  PL_RETURN_IF_ERROR(sink_ir->SetRelation(output_relation));
  return true;
}

StatusOr<bool> OperatorRelationRule::SetOther(OperatorIR* operator_ir) const {
  CHECK_EQ(operator_ir->parents().size(), 1UL);
  PL_RETURN_IF_ERROR(operator_ir->SetRelation(operator_ir->parents()[0]->relation()));
  return true;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
