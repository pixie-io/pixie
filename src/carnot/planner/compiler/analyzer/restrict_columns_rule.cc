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
#include <vector>

#include "src/carnot/planner/compiler/analyzer/restrict_columns_rule.h"
#include "src/carnot/planner/ir/map_ir.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

bool IsRestrictedColumn(CompilerState* compiler_state, std::string_view table_name,
                        std::string_view input_col_name) {
  auto set = (*compiler_state->table_names_to_sensitive_columns())[table_name];
  return set.contains(input_col_name);
}

StatusOr<bool> RestrictColumnsRule::Apply(IRNode* ir_node) {
  if (redaction_impl_ == nullptr) {
    return false;
  }
  if (!Match(ir_node, MemorySource())) {
    return false;
  }
  auto memsrc = static_cast<MemorySourceIR*>(ir_node);
  if (!memsrc->is_type_resolved()) {
    return false;
  }
  // Early exit if the table is not in the sensitive tables list.
  // TODO(philkuz) optimize out maps that don't do anything.
  if (!compiler_state_->table_names_to_sensitive_columns()->contains(memsrc->table_name())) {
    return false;
  }

  PX_ASSIGN_OR_RETURN(auto new_op, redaction_impl_(memsrc));
  // Update all of memsrc's dependencies to point to the new node.
  for (const auto& dep : memsrc->Children()) {
    if (!dep->IsOperator()) {
      continue;
    }
    if (dep == new_op) {
      continue;
    }
    auto casted_node = static_cast<OperatorIR*>(dep);
    PX_RETURN_IF_ERROR(casted_node->ReplaceParent(memsrc, new_op));
  }

  PX_RETURN_IF_ERROR(PropagateTypeChangesFromNode(memsrc->graph(), new_op, compiler_state_));

  // Insert new node in between the memory source and the next operator.
  return true;
}

StatusOr<OperatorIR*> RestrictColumnsRule::RedactAllData(MemorySourceIR* memsrc) {
  ColExpressionVector col_exprs;
  for (const auto& input_col_name : memsrc->resolved_table_type()->ColumnNames()) {
    if (IsRestrictedColumn(compiler_state_, memsrc->table_name(), input_col_name)) {
      // Replace column with restricted value.
      PX_ASSIGN_OR_RETURN(StringIR * restricted_ir,
                          memsrc->graph()->CreateNode<StringIR>(memsrc->ast(), "REDACTED"));
      col_exprs.emplace_back(input_col_name, restricted_ir);
      continue;
    }

    // Copy the column otherwise.
    PX_ASSIGN_OR_RETURN(ColumnIR * column_ir,
                        memsrc->graph()->CreateNode<ColumnIR>(memsrc->ast(), input_col_name,
                                                              /*parent_op_idx*/ 0));
    col_exprs.emplace_back(input_col_name, column_ir);
  }
  // Create the new map.
  return memsrc->graph()->CreateNode<MapIR>(memsrc->ast(), memsrc, col_exprs,
                                            /* keep_input_columns */ false);
}

StatusOr<OperatorIR*> RestrictColumnsRule::RedactPIIData(MemorySourceIR* memsrc) {
  ColExpressionVector col_exprs;
  for (const auto& input_col_name : memsrc->resolved_table_type()->ColumnNames()) {
    PX_ASSIGN_OR_RETURN(ColumnIR * column_ir,
                        memsrc->graph()->CreateNode<ColumnIR>(memsrc->ast(), input_col_name,
                                                              /*parent_op_idx*/ 0));
    ExpressionIR* new_col_expr_ir = column_ir;
    if (IsRestrictedColumn(compiler_state_, memsrc->table_name(), input_col_name)) {
      // Replace column with call to px.redact_pii_best_effort.
      PX_ASSIGN_OR_RETURN(
          FuncIR * restricted_ir,
          memsrc->graph()->CreateNode<FuncIR>(
              memsrc->ast(), FuncIR::Op{FuncIR::Opcode::non_op, "", "redact_pii_best_effort"},
              std::vector<ExpressionIR*>{column_ir}));
      new_col_expr_ir = restricted_ir;
    }
    col_exprs.emplace_back(input_col_name, new_col_expr_ir);
  }
  // Create the new map.
  return memsrc->graph()->CreateNode<MapIR>(memsrc->ast(), memsrc, col_exprs,
                                            /* keep_input_columns */ false);
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
