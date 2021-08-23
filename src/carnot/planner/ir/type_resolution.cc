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

#include "src/carnot/planner/ir/ir_nodes.h"

namespace px {
namespace carnot {
namespace planner {

Status MapIR::ResolveType(CompilerState* compiler_state) {
  DCHECK_EQ(1, parent_types().size());
  auto table = TableType::Create();
  for (const auto& col_expr : col_exprs_) {
    PL_RETURN_IF_ERROR(ResolveExpressionType(col_expr.node, compiler_state, parent_types()));
    table->AddColumn(col_expr.name, col_expr.node->resolved_type());
  }
  return SetResolvedType(table);
}

Status MemorySinkIR::ResolveType(CompilerState* /* compiler_state */) {
  DCHECK_EQ(1, parent_types().size());
  // When out_columns_ is empty, the MemorySink just copies the parent type.
  if (out_columns_.size() == 0) {
    PL_ASSIGN_OR_RETURN(auto type_ptr, OperatorIR::DefaultResolveType(parent_types()));
    return SetResolvedType(type_ptr);
  }
  auto parent_table_type = std::static_pointer_cast<TableType>(parent_types()[0]);
  auto table = TableType::Create();
  for (const auto& col_name : out_columns_) {
    PL_ASSIGN_OR_RETURN(auto col_type, parent_table_type->GetColumnType(col_name));
    table->AddColumn(col_name, col_type);
  }
  return SetResolvedType(table);
}

Status FilterIR::ResolveType(CompilerState* compiler_state) {
  PL_RETURN_IF_ERROR(ResolveExpressionType(filter_expr_, compiler_state, parent_types()));
  PL_ASSIGN_OR_RETURN(auto type_ptr, OperatorIR::DefaultResolveType(parent_types()));
  return SetResolvedType(type_ptr);
}

Status GRPCSinkIR::ResolveType(CompilerState* /* compiler_state */) {
  if (!has_output_table()) {
    return CreateIRNodeError(
        "Cannot resolve type of GRPCSink unless it produces a final output result.");
  }
  DCHECK_EQ(1, parent_types().size());
  // When out_columns_ is empty, the GRPCSink just copies the parent type.
  if (out_columns_.size() == 0) {
    PL_ASSIGN_OR_RETURN(auto type_ptr, OperatorIR::DefaultResolveType(parent_types()));
    return SetResolvedType(type_ptr);
  }
  auto parent_table_type = std::static_pointer_cast<TableType>(parent_types()[0]);
  auto table = TableType::Create();
  for (const auto& col_name : out_columns_) {
    PL_ASSIGN_OR_RETURN(auto col_type, parent_table_type->GetColumnType(col_name));
    table->AddColumn(col_name, col_type);
  }
  return SetResolvedType(table);
}

Status DropIR::ResolveType(CompilerState* /* compiler_state */) {
  DCHECK_EQ(1, parent_types().size());
  auto new_table = std::static_pointer_cast<TableType>(parent_types()[0]->Copy());
  for (const auto& col_name : col_names_) {
    new_table->RemoveColumn(col_name);
  }
  return SetResolvedType(new_table);
}

Status BlockingAggIR::ResolveType(CompilerState* compiler_state) {
  DCHECK_EQ(1, parent_types().size());
  auto new_table = TableType::Create();
  for (const auto& group_col : groups()) {
    PL_RETURN_IF_ERROR(ResolveExpressionType(group_col, compiler_state, parent_types()));
    new_table->AddColumn(group_col->col_name(), group_col->resolved_type());
  }
  for (const auto& col_expr : aggregate_expressions_) {
    PL_RETURN_IF_ERROR(ResolveExpressionType(col_expr.node, compiler_state, parent_types()));
    new_table->AddColumn(col_expr.name, col_expr.node->resolved_type());
  }
  return SetResolvedType(new_table);
}

Status UnionIR::ResolveType(CompilerState* /* compiler_state */) {
  DCHECK_LE(1, parent_types().size());
  // Currently, null values are not supported so all input schemas to a union must be the same.
  auto type = parent_types()[0]->Copy();
  for (const auto& [parent_idx, parent_type] : Enumerate(parent_types())) {
    PL_UNUSED(parent_idx);
    PL_UNUSED(parent_type);
    // TODO(james, PP-2595): Add type checking here.
  }
  return SetResolvedType(type);
}

Status JoinIR::ResolveType(CompilerState* /* compiler_state */) {
  DCHECK_EQ(2, parent_types().size());
  const auto& [left_type, right_type] = left_right_table_types();
  const auto& [left_suffix, right_suffix] = left_right_suffixs();

  auto new_table = std::static_pointer_cast<TableType>(left_type->Copy());
  for (const auto& [col_name, col_type] : *right_type) {
    if (!new_table->HasColumn(col_name)) {
      new_table->AddColumn(col_name, col_type);
      continue;
    }

    // If the column already exists, we rename the column from the left table with the left suffix,
    // and and the column from the right with the right suffix.
    auto new_left_col_name = absl::Substitute("$0$1", col_name, left_suffix);
    const auto err_fmt_string =
        "duplicate column '$0' after merge. Change the specified suffixes ('$1','$2') to fix";
    // If the left_suffix is empty, then we can skip checking and renaming.
    if (new_left_col_name != col_name) {
      if (new_table->HasColumn(new_left_col_name)) {
        return CreateIRNodeError(err_fmt_string, new_left_col_name, left_suffix, right_suffix);
      }
      new_table->RenameColumn(col_name, new_left_col_name);
    }

    auto new_right_col_name = absl::Substitute("$0$1", col_name, right_suffix);
    if (new_table->HasColumn(new_right_col_name)) {
      return CreateIRNodeError(err_fmt_string, new_right_col_name, left_suffix, right_suffix);
    }
    new_table->AddColumn(new_right_col_name, col_type);
  }

  return SetResolvedType(new_table);
}

Status UDTFSourceIR::ResolveType(CompilerState* /* compiler_state */) {
  table_store::schema::Relation relation;
  PL_RETURN_IF_ERROR(relation.FromProto(&udtf_spec_.relation()));
  auto new_table = TableType::Create(relation);
  return SetResolvedType(new_table);
}

Status MemorySourceIR::ResolveType(CompilerState* compiler_state) {
  auto relation_it = compiler_state->relation_map()->find(table_name());
  if (relation_it == compiler_state->relation_map()->end()) {
    return CreateIRNodeError("Table '$0' not found", table_name_);
  }
  auto table_relation = relation_it->second;
  auto full_table_type = TableType::Create(table_relation);
  if (select_all()) {
    return SetResolvedType(full_table_type);
  }

  auto new_table = TableType::Create();
  for (const auto& col_name : column_names_) {
    PL_ASSIGN_OR_RETURN(auto col_type, full_table_type->GetColumnType(col_name));
    new_table->AddColumn(col_name, col_type);
  }
  return SetResolvedType(new_table);
}

StatusOr<TypePtr> OperatorIR::DefaultResolveType(const std::vector<TypePtr>& parent_types) {
  // Only one parent is supported by default. If your new OperatorIR needs more than one parent, you
  // must define a ResolveType function on it with the same signature as this function (same
  // return type and args, but should be an instance method not static).
  DCHECK_EQ(1, parent_types.size());
  return parent_types[0]->Copy();
}

Status ColumnIR::ResolveType(CompilerState* /* compiler_state */,
                             const std::vector<TypePtr>& parent_types) {
  DCHECK(container_op_parent_idx_set_);
  DCHECK_LT(container_op_parent_idx_, parent_types.size());
  auto parent_table = std::static_pointer_cast<TableType>(parent_types[container_op_parent_idx_]);
  PL_ASSIGN_OR_RETURN(auto type_, parent_table->GetColumnType(col_name_));
  return SetResolvedType(type_->Copy());
}

Status FuncIR::ResolveType(CompilerState* compiler_state,
                           const std::vector<TypePtr>& parent_types) {
  // Resolve the arg types first.
  std::vector<std::shared_ptr<ValueType>> arg_types;
  for (auto init_arg : init_args()) {
    // We may want to have semantic type resolution based on init arguments in the future. If so we
    // can change this to actually resolve the init args semantic type.
    arg_types.push_back(ValueType::Create(init_arg->EvaluatedDataType(), SemanticType::ST_NONE));
  }
  for (auto arg : args()) {
    PL_RETURN_IF_ERROR(ResolveExpressionType(arg, compiler_state, parent_types));
    // At the moment all expressions resolve to a ValueType, since UDFs can't take tables as args.
    auto primitive_type = std::static_pointer_cast<ValueType>(arg->resolved_type());
    arg_types.push_back(primitive_type);
  }
  PL_ASSIGN_OR_RETURN(auto type_,
                      compiler_state->registry_info()->ResolveUDFType(func_name(), arg_types));
  return SetResolvedType(type_);
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
