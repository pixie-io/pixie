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

#include "src/carnot/planner/ir/map_ir.h"
#include "src/carnot/planner/ir/column_ir.h"
#include "src/carnot/planner/ir/ir.h"

namespace px {
namespace carnot {
namespace planner {

Status MapIR::SetColExprs(const ColExpressionVector& exprs) {
  auto old_exprs = col_exprs_;
  col_exprs_.clear();
  for (const ColumnExpression& expr : old_exprs) {
    PX_RETURN_IF_ERROR(graph()->DeleteEdge(this, expr.node));
  }
  for (const auto& expr : exprs) {
    PX_RETURN_IF_ERROR(AddColExpr(expr));
  }
  for (const ColumnExpression& expr : old_exprs) {
    PX_RETURN_IF_ERROR(graph()->DeleteOrphansInSubtree(expr.node->id()));
  }
  return Status::OK();
}

Status MapIR::AddColExpr(const ColumnExpression& expr) {
  PX_ASSIGN_OR_RETURN(auto expr_node, graph()->OptionallyCloneWithEdge(this, expr.node));
  col_exprs_.emplace_back(expr.name, expr_node);
  return Status::OK();
}

Status MapIR::UpdateColExpr(std::string_view name, ExpressionIR* expr) {
  PX_ASSIGN_OR_RETURN(auto expr_node, graph()->OptionallyCloneWithEdge(this, expr));
  for (size_t i = 0; i < col_exprs_.size(); ++i) {
    if (col_exprs_[i].name == name) {
      auto old_expr = col_exprs_[i].node;
      col_exprs_[i].node = expr_node;
      PX_RETURN_IF_ERROR(graph()->DeleteEdge(this, old_expr));
      return graph()->DeleteOrphansInSubtree(old_expr->id());
    }
  }
  return error::Internal("Column $0 does not exist in Map", name);
}

Status MapIR::UpdateColExpr(ExpressionIR* old_expr, ExpressionIR* new_expr) {
  PX_ASSIGN_OR_RETURN(auto expr_node, graph()->OptionallyCloneWithEdge(this, new_expr));
  for (size_t i = 0; i < col_exprs_.size(); ++i) {
    if (col_exprs_[i].node == old_expr) {
      col_exprs_[i].node = expr_node;
      PX_RETURN_IF_ERROR(graph()->DeleteEdge(this, old_expr));
      return graph()->DeleteOrphansInSubtree(old_expr->id());
    }
  }
  return error::Internal("Expression $0 does not exist in Map", old_expr->DebugString());
}

Status MapIR::Init(OperatorIR* parent, const ColExpressionVector& col_exprs,
                   bool keep_input_columns) {
  PX_RETURN_IF_ERROR(AddParent(parent));
  PX_RETURN_IF_ERROR(SetColExprs(col_exprs));
  keep_input_columns_ = keep_input_columns;
  return Status::OK();
}

StatusOr<std::vector<absl::flat_hash_set<std::string>>> MapIR::RequiredInputColumns() const {
  absl::flat_hash_set<std::string> required;
  for (const auto& expr : col_exprs_) {
    PX_ASSIGN_OR_RETURN(auto inputs, expr.node->InputColumnNames());
    required.insert(inputs.begin(), inputs.end());
  }
  return std::vector<absl::flat_hash_set<std::string>>{required};
}

StatusOr<absl::flat_hash_set<std::string>> MapIR::PruneOutputColumnsToImpl(
    const absl::flat_hash_set<std::string>& output_colnames) {
  ColExpressionVector new_exprs;
  for (const auto& expr : col_exprs_) {
    if (output_colnames.contains(expr.name)) {
      new_exprs.push_back(expr);
    }
  }
  PX_RETURN_IF_ERROR(SetColExprs(new_exprs));
  return output_colnames;
}

Status MapIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_map_op();
  op->set_op_type(planpb::MAP_OPERATOR);

  for (const auto& col_expr : col_exprs_) {
    auto expr = pb->add_expressions();
    PX_RETURN_IF_ERROR(col_expr.node->ToProto(expr));
    pb->add_column_names(col_expr.name);
  }

  return Status::OK();
}

Status MapIR::CopyFromNodeImpl(const IRNode* node,
                               absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const MapIR* map_ir = static_cast<const MapIR*>(node);
  ColExpressionVector new_col_exprs;
  for (const ColumnExpression& col_expr : map_ir->col_exprs_) {
    PX_ASSIGN_OR_RETURN(ExpressionIR * new_node,
                        graph()->CopyNode(col_expr.node, copied_nodes_map));
    new_col_exprs.push_back({col_expr.name, new_node});
  }
  keep_input_columns_ = map_ir->keep_input_columns_;
  return SetColExprs(new_col_exprs);
}

Status MapIR::UpdateOpAfterParentTypesResolvedImpl() {
  if (!keep_input_columns_) {
    return Status::OK();
  }

  ColExpressionVector new_col_exprs;

  absl::flat_hash_set<std::string> new_column_names;
  for (auto expr : col_exprs_) {
    new_column_names.insert(expr.name);
  }

  DCHECK_EQ(1UL, parents().size());
  auto parent_table_type = parents()[0]->resolved_table_type();
  for (auto col_name : parent_table_type->ColumnNames()) {
    // If this column is being overwritten with a new expression, skip it here.
    if (new_column_names.contains(col_name)) {
      continue;
    }
    // Otherwise, bring over the column from the previous operator.
    PX_ASSIGN_OR_RETURN(ColumnIR * col_ir,
                        graph()->CreateNode<ColumnIR>(ast(), col_name, 0 /*parent_op_idx*/));
    new_col_exprs.push_back(ColumnExpression(col_name, col_ir));
  }
  for (auto expr : col_exprs_) {
    new_col_exprs.push_back(expr);
  }

  keep_input_columns_ = false;
  PX_RETURN_IF_ERROR(SetColExprs(new_col_exprs));
  return Status::OK();
}

Status MapIR::ResolveType(CompilerState* compiler_state) {
  DCHECK_EQ(1U, parent_types().size());
  // keep_input_columns_ = true case should be handled by UpdateOpAfterParentTypesResolvedImpl.
  DCHECK(!keep_input_columns_);
  auto table_type = TableType::Create();
  for (const auto& col_expr : col_exprs_) {
    PX_RETURN_IF_ERROR(ResolveExpressionType(col_expr.node, compiler_state, parent_types()));
    table_type->AddColumn(col_expr.name, col_expr.node->resolved_type());
  }
  return SetResolvedType(table_type);
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
