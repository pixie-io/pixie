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

#include "src/carnot/planner/compiler/analyzer/combine_consecutive_maps_rule.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

bool ContainsChildColumn(const ExpressionIR& expr,
                         const absl::flat_hash_set<std::string>& colnames) {
  if (Match(&expr, ColumnNode())) {
    return colnames.contains(static_cast<const ColumnIR*>(&expr)->col_name());
  }
  if (Match(&expr, Func())) {
    auto func = static_cast<const FuncIR*>(&expr);
    for (const auto& arg : func->all_args()) {
      if (ContainsChildColumn(*arg, colnames)) {
        return true;
      }
    }
    return false;
  }
  return false;
}

bool CombineConsecutiveMapsRule::ShouldCombineMaps(
    MapIR* parent, MapIR* child, const absl::flat_hash_set<std::string>& parent_col_names) {
  // This rule is targeted at combining maps of the following style:
  // df.foo = 1
  // df.bar = df.abc * 2
  // The logic for combining maps where the child keeps input columns is different
  // from the logic where the child does not keep input columns, so for simplicity we only
  // solve the former case right now.
  if (!child->keep_input_columns()) {
    return false;
  }
  // If the parent has more than one child (one besides the child map), then we don't want
  // to combine them together because it could affect the output of the other child.
  if (parent->Children().size() > 1) {
    return false;
  }
  for (const auto& child_expr : child->col_exprs()) {
    if (ContainsChildColumn(*(child_expr.node), parent_col_names)) {
      return false;
    }
  }
  return true;
}

Status CombineConsecutiveMapsRule::CombineMaps(
    MapIR* parent, MapIR* child, const absl::flat_hash_set<std::string>& parent_col_names) {
  // If the column name is simply being overwritten, that's ok.
  for (const auto& child_col_expr : child->col_exprs()) {
    ExpressionIR* child_expr = child_col_expr.node;

    if (parent_col_names.contains(child_col_expr.name)) {
      // Overwrite it in the parent with the child if it's a name reassignment.
      for (const ColumnExpression& parent_col_expr : parent->col_exprs()) {
        if (parent_col_expr.name == child_col_expr.name) {
          PX_RETURN_IF_ERROR(parent->UpdateColExpr(parent_col_expr.name, child_expr));
        }
      }
    } else {
      // Otherwise just append it to the list.
      PX_RETURN_IF_ERROR(parent->AddColExpr(child_col_expr));
    }
    PX_RETURN_IF_ERROR(child_expr->graph()->DeleteEdge(child, child_expr));
  }

  PX_RETURN_IF_ERROR(child->RemoveParent(parent));
  for (auto grandchild : child->Children()) {
    PX_RETURN_IF_ERROR(grandchild->ReplaceParent(child, parent));
  }
  PX_RETURN_IF_ERROR(child->graph()->DeleteNode(child->id()));
  return Status::OK();
}

StatusOr<bool> CombineConsecutiveMapsRule::Apply(IRNode* ir_node) {
  // Roll the child into the parent so we only have to iterate over the graph once.
  if (!Match(ir_node, Map())) {
    return false;
  }
  auto child = static_cast<MapIR*>(ir_node);
  CHECK_EQ(child->parents().size(), 1UL);
  auto parent_op = child->parents()[0];

  if (!Match(parent_op, Map())) {
    return false;
  }
  auto parent = static_cast<MapIR*>(parent_op);

  absl::flat_hash_set<std::string> parent_cols;
  for (const auto& parent_expr : parent->col_exprs()) {
    parent_cols.insert(parent_expr.name);
  }

  if (!ShouldCombineMaps(parent, child, parent_cols)) {
    return false;
  }
  PX_RETURN_IF_ERROR(CombineMaps(parent, child, parent_cols));
  return true;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
