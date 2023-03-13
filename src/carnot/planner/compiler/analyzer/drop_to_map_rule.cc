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

#include "src/carnot/planner/compiler/analyzer/drop_to_map_rule.h"
#include "src/carnot/planner/ir/map_ir.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using table_store::schema::Relation;

StatusOr<bool> DropToMapOperatorRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, Drop())) {
    return DropToMap(static_cast<DropIR*>(ir_node));
  }
  return false;
}

StatusOr<bool> DropToMapOperatorRule::DropToMap(DropIR* drop_ir) {
  IR* ir_graph = drop_ir->graph();
  DCHECK_EQ(drop_ir->parents().size(), 1UL);
  OperatorIR* parent_op = drop_ir->parents()[0];

  DCHECK(parent_op->is_type_resolved());
  auto parent_table_type = parent_op->resolved_table_type();

  absl::flat_hash_set<std::string> dropped_columns;
  for (const auto& name : drop_ir->col_names()) {
    if (!parent_table_type->HasColumn(name)) {
      return drop_ir->CreateIRNodeError("Column '$0' not found in parent dataframe", name);
    }
    dropped_columns.insert(name);
  }

  ColExpressionVector col_exprs;
  for (const auto& input_col_name : parent_table_type->ColumnNames()) {
    if (dropped_columns.contains(input_col_name)) {
      continue;
    }
    PX_ASSIGN_OR_RETURN(ColumnIR * column_ir,
                        ir_graph->CreateNode<ColumnIR>(drop_ir->ast(), input_col_name,
                                                       /*parent_op_idx*/ 0));
    PX_RETURN_IF_ERROR(ResolveExpressionType(column_ir, compiler_state_, {parent_table_type}));
    col_exprs.emplace_back(input_col_name, column_ir);
  }

  // Init the map from the drop.
  PX_ASSIGN_OR_RETURN(MapIR * map_ir,
                      ir_graph->CreateNode<MapIR>(drop_ir->ast(), parent_op, col_exprs,
                                                  /* keep_input_columns */ false));
  PX_RETURN_IF_ERROR(ResolveOperatorType(map_ir, compiler_state_));

  // Update all of drop's dependencies to point to src.
  for (const auto& dep : drop_ir->Children()) {
    if (!dep->IsOperator()) {
      return drop_ir->CreateIRNodeError(
          "Received unexpected non-operator dependency on Drop node.");
    }
    auto casted_node = static_cast<OperatorIR*>(dep);
    PX_RETURN_IF_ERROR(casted_node->ReplaceParent(drop_ir, map_ir));
  }
  PX_RETURN_IF_ERROR(drop_ir->RemoveParent(parent_op));
  PX_RETURN_IF_ERROR(ir_graph->DeleteNode(drop_ir->id()));
  return true;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
