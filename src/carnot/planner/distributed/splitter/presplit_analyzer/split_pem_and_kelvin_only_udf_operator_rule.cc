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

#include <vector>

#include "src/carnot/planner/distributed/splitter/executor_utils.h"
#include "src/carnot/planner/distributed/splitter/presplit_analyzer/split_pem_and_kelvin_only_udf_operator_rule.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

std::string GetUniqueOutputName(FuncIR* input_expr,
                                const absl::flat_hash_set<std::string>& used_column_names) {
  std::string output_name;
  auto idx = 0;
  while (used_column_names.contains(
      output_name = absl::Substitute("$0_$1", input_expr->func_name(), idx++))) {
    // Keep incrementing idx until we get a unique name.
  }
  return output_name;
}

// If `expr` or one of its children is a PEM-only UDF, move it to be an output
// of the PEM-only map.
StatusOr<absl::flat_hash_set<std::string>>
SplitPEMAndKelvinOnlyUDFOperatorRule::OptionallyUpdateExpression(
    IRNode* expr_parent, ExpressionIR* expr, MapIR* pem_only_map,
    const absl::flat_hash_set<std::string>& used_column_names) {
  if (!Match(expr, Func())) {
    return absl::flat_hash_set<std::string>({});
  }

  absl::flat_hash_set<std::string> new_col_names;

  auto graph = expr->graph();
  auto func = static_cast<FuncIR*>(expr);

  // Check the root expression first.
  // In the event both this function and its child are both PEM-only UDFs, we should
  // move them both together, rather than moving the child then this parent after.
  PL_ASSIGN_OR_RETURN(
      auto is_scalar_func_executor,
      IsFuncWithExecutor(compiler_state_, expr, udfspb::UDFSourceExecutor::UDF_PEM));
  if (!is_scalar_func_executor) {
    // Even if this func itself isn't a PEM-only UDF, its children still might be.
    // Optionally update all of the child expressions of this expression.
    for (ExpressionIR* arg : func->args()) {
      PL_ASSIGN_OR_RETURN(auto arg_col_names,
                          OptionallyUpdateExpression(func, arg, pem_only_map, used_column_names));
      new_col_names.insert(arg_col_names.begin(), arg_col_names.end());
    }
    return new_col_names;
  }

  // Create the column that will replace the expression in the operator we are splitting.
  auto output_col_name = GetUniqueOutputName(func, used_column_names);
  new_col_names.insert(output_col_name);
  PL_ASSIGN_OR_RETURN(auto input_col, graph->CreateNode<ColumnIR>(expr->ast(), output_col_name,
                                                                  /*parent_op_idx*/ 0));
  // This column should have the same type as the expression, since it's just a projection.
  input_col->ResolveColumnType(expr->EvaluatedDataType());
  PL_RETURN_IF_ERROR(input_col->SetResolvedType(expr->resolved_type()));

  // Add the PEM-only expression to the PEM-only map.
  // It will get deleted from its original parent next.
  auto col_expr = ColumnExpression(output_col_name, expr);
  PL_RETURN_IF_ERROR(pem_only_map->AddColExpr(col_expr));

  // Update the original expression's parent to point to the new column in the PEM-only map.
  // We may want to refactor this logic into a utility for updating Operator expressions,
  // so that we don't have to match operator type in every place that we want to replace
  // expression with a new expression.
  if (Match(expr_parent, Filter())) {
    auto filter = static_cast<FilterIR*>(expr_parent);
    PL_RETURN_IF_ERROR(filter->SetFilterExpr(input_col));
  } else if (Match(expr_parent, Map())) {
    auto map = static_cast<MapIR*>(expr_parent);
    PL_RETURN_IF_ERROR(map->UpdateColExpr(expr, input_col));
  } else if (Match(expr_parent, Func())) {
    auto func = static_cast<FuncIR*>(expr_parent);
    PL_RETURN_IF_ERROR(func->UpdateArg(expr, input_col));
  } else {
    return error::Internal("Unexpected parent expression type: $0", expr_parent->type_string());
  }
  return new_col_names;
}

StatusOr<bool> SplitPEMAndKelvinOnlyUDFOperatorRule::Apply(IRNode* node) {
  if (!Match(node, Map()) && !Match(node, Filter())) {
    return false;
  }

  PL_ASSIGN_OR_RETURN(
      auto has_pem_only_udf,
      HasFuncWithExecutor(compiler_state_, node, udfspb::UDFSourceExecutor::UDF_PEM));
  PL_ASSIGN_OR_RETURN(
      auto has_kelvin_only_udf,
      HasFuncWithExecutor(compiler_state_, node, udfspb::UDFSourceExecutor::UDF_KELVIN));

  // Don't need to split this node unless a Kelvin-only UDF is scheduled on the
  // same operator as a PEM-only UDF.
  if (!has_pem_only_udf || !has_kelvin_only_udf) {
    return false;
  }

  auto graph = node->graph();
  auto op = static_cast<OperatorIR*>(node);
  if (op->parents().size() != 1) {
    return op->CreateIRNodeError("Operator unexpectedly has $0 parents, expected 1",
                                 op->parents().size());
  }
  auto parent = op->parents()[0];
  auto parent_relation = parent->relation();

  // Collect the expression(s) to optionally modify.
  std::vector<ExpressionIR*> operator_expressions;
  if (Match(op, Map())) {
    auto map = static_cast<MapIR*>(op);
    for (const auto& expr : map->col_exprs()) {
      operator_expressions.push_back(expr.node);
    }
  } else if (Match(op, Filter())) {
    auto filter = static_cast<FilterIR*>(op);
    operator_expressions.push_back(filter->filter_expr());
  } else {
    return op->CreateIRNodeError("Unexpected operator type, expected Map or Filter");
  }

  // Create a new Map node to handle all of the PEM-only expressions.
  // It will become the parent of the current operator.
  PL_ASSIGN_OR_RETURN(MapIR * pem_map,
                      graph->CreateNode<MapIR>(node->ast(), parent, ColExpressionVector({}),
                                               /* keep_input_columns */ false));
  // Optionally update each expression.
  // If it contains a PEM-only UDF, we will split it into the new map node we are creating.
  // If it doesn't contain a PEM-only UDF, we will not modify it.
  // Also, keep track of the column names the operators are using so we don't autogenerate
  // a new column with a name collision with an existing column.
  auto parent_col_names = parent_relation.col_names();
  absl::flat_hash_set<std::string> used_column_names(parent_col_names.begin(),
                                                     parent_col_names.end());
  for (ExpressionIR* operator_expression : operator_expressions) {
    PL_ASSIGN_OR_RETURN(auto new_col_names, OptionallyUpdateExpression(op, operator_expression,
                                                                       pem_map, used_column_names));
    used_column_names.insert(new_col_names.begin(), new_col_names.end());
  }

  // PEM-only map must contain all of the input columns to the operator `op`.
  // We do this last, because some of those required inputs may no longer be required.
  PL_ASSIGN_OR_RETURN(auto required_inputs_per_parent, op->RequiredInputColumns());
  if (required_inputs_per_parent.size() != 1) {
    return op->CreateIRNodeError("Operator unexpectedly has $0 parents, expected 1",
                                 op->parents().size());
  }
  for (const auto& required_input_col : required_inputs_per_parent[0]) {
    // If a required input column is one we just generated from a PEM-only function,
    // no need to add a column projection for it to the PEM map.
    if (!parent_relation.HasColumn(required_input_col)) {
      continue;
    }
    PL_ASSIGN_OR_RETURN(auto col_node, graph->CreateNode<ColumnIR>(op->ast(), required_input_col,
                                                                   /*parent_op_idx*/ 0));
    col_node->ResolveColumnType(parent_relation);
    PL_RETURN_IF_ERROR(pem_map->AddColExpr(ColumnExpression(required_input_col, col_node)));
  }

  // Update the relation of the PEM-only map.
  // The relation of the parent should be unchanged, since it is just a reassignment
  // of the same output value.
  PL_RETURN_IF_ERROR(pem_map->SetRelationFromExprs());
  PL_RETURN_IF_ERROR(ResolveOperatorType(pem_map, compiler_state_));
  PL_RETURN_IF_ERROR(op->ReplaceParent(parent, pem_map));
  return true;
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
