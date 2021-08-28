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

#include "src/carnot/planner/ir/join_ir.h"
#include "src/carnot/planner/ir/column_ir.h"
#include "src/carnot/planner/ir/ir.h"

namespace px {
namespace carnot {
namespace planner {

Status JoinIR::CopyFromNodeImpl(const IRNode* node,
                                absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const JoinIR* join_node = static_cast<const JoinIR*>(node);
  join_type_ = join_node->join_type_;

  std::vector<ColumnIR*> new_output_columns;
  for (const ColumnIR* col : join_node->output_columns_) {
    PL_ASSIGN_OR_RETURN(ColumnIR * new_node, graph()->CopyNode(col, copied_nodes_map));
    new_output_columns.push_back(new_node);
  }
  PL_RETURN_IF_ERROR(SetOutputColumns(join_node->column_names_, new_output_columns));

  std::vector<ColumnIR*> new_left_columns;
  for (const ColumnIR* col : join_node->left_on_columns_) {
    PL_ASSIGN_OR_RETURN(ColumnIR * new_node, graph()->CopyNode(col, copied_nodes_map));
    new_left_columns.push_back(new_node);
  }

  std::vector<ColumnIR*> new_right_columns;
  for (const ColumnIR* col : join_node->right_on_columns_) {
    PL_ASSIGN_OR_RETURN(ColumnIR * new_node, graph()->CopyNode(col, copied_nodes_map));
    new_right_columns.push_back(new_node);
  }

  PL_RETURN_IF_ERROR(SetJoinColumns(new_left_columns, new_right_columns));
  suffix_strs_ = join_node->suffix_strs_;
  return Status::OK();
}

StatusOr<JoinIR::JoinType> JoinIR::GetJoinEnum(const std::string& join_type_str) const {
  // TODO(philkuz) (PL-1136) convert to enum library friendly version.
  absl::flat_hash_map<std::string, JoinType> join_key_mapping = {{"inner", JoinType::kInner},
                                                                 {"left", JoinType::kLeft},
                                                                 {"outer", JoinType::kOuter},
                                                                 {"right", JoinType::kRight}};
  auto iter = join_key_mapping.find(join_type_str);

  // If the join type is not found, then return an error.
  if (iter == join_key_mapping.end()) {
    std::vector<std::string> valid_join_keys;
    for (auto kv : join_key_mapping) {
      valid_join_keys.push_back(kv.first);
    }
    return CreateIRNodeError("'$0' join type not supported. Only {$1} are available.",
                             join_type_str, absl::StrJoin(valid_join_keys, ","));
  }
  return iter->second;
}

planpb::JoinOperator::JoinType JoinIR::GetPbJoinEnum(JoinType join_type) {
  absl::flat_hash_map<JoinType, planpb::JoinOperator::JoinType> join_key_mapping = {
      {JoinType::kInner, planpb::JoinOperator_JoinType_INNER},
      {JoinType::kLeft, planpb::JoinOperator_JoinType_LEFT_OUTER},
      {JoinType::kOuter, planpb::JoinOperator_JoinType_FULL_OUTER}};
  auto join_key_iter = join_key_mapping.find(join_type);
  CHECK(join_key_iter != join_key_mapping.end()) << "Received an unexpected enum value.";
  return join_key_iter->second;
}

Status JoinIR::ToProto(planpb::Operator* op) const {
  planpb::JoinOperator::JoinType join_enum_type = GetPbJoinEnum(join_type_);
  DCHECK_EQ(left_on_columns_.size(), right_on_columns_.size());
  auto pb = op->mutable_join_op();
  op->set_op_type(planpb::JOIN_OPERATOR);
  pb->set_type(join_enum_type);
  for (int64_t i = 0; i < static_cast<int64_t>(left_on_columns_.size()); i++) {
    auto eq_condition = pb->add_equality_conditions();
    PL_ASSIGN_OR_RETURN(auto left_index, left_on_columns_[i]->GetColumnIndex());
    PL_ASSIGN_OR_RETURN(auto right_index, right_on_columns_[i]->GetColumnIndex());
    eq_condition->set_left_column_index(left_index);
    eq_condition->set_right_column_index(right_index);
  }

  for (ColumnIR* col : output_columns_) {
    auto* parent_col = pb->add_output_columns();
    int64_t parent_idx = col->container_op_parent_idx();
    DCHECK_LT(parent_idx, 2);
    parent_col->set_parent_index(col->container_op_parent_idx());
    DCHECK(col->IsDataTypeEvaluated()) << "Column not evaluated";
    PL_ASSIGN_OR_RETURN(auto index, col->GetColumnIndex());
    parent_col->set_column_index(index);
  }

  for (const auto& col_name : column_names_) {
    *(pb->add_column_names()) = col_name;
  }
  // NOTE: not setting value as this is set in the execution engine. Keeping this here in case it
  // needs to be modified in the future.
  // pb->set_rows_per_batch(1024);

  return Status::OK();
}

Status JoinIR::Init(const std::vector<OperatorIR*>& parents, const std::string& how_type,
                    const std::vector<ColumnIR*>& left_on_cols,
                    const std::vector<ColumnIR*>& right_on_cols,
                    const std::vector<std::string>& suffix_strs) {
  if (left_on_cols.size() != right_on_cols.size()) {
    return CreateIRNodeError("'left_on' and 'right_on' must contain the same number of elements.");
  }

  // Support joining a table against itself by calling HandleDuplicateParents.
  PL_ASSIGN_OR_RETURN(auto transformed_parents, HandleDuplicateParents(parents));
  for (auto* p : transformed_parents) {
    PL_RETURN_IF_ERROR(AddParent(p));
  }

  PL_RETURN_IF_ERROR(SetJoinColumns(left_on_cols, right_on_cols));

  suffix_strs_ = suffix_strs;
  return SetJoinType(how_type);
}

Status JoinIR::SetJoinColumns(const std::vector<ColumnIR*>& left_columns,
                              const std::vector<ColumnIR*>& right_columns) {
  DCHECK(left_on_columns_.empty());
  DCHECK(right_on_columns_.empty());
  left_on_columns_.resize(left_columns.size());
  for (size_t i = 0; i < left_columns.size(); ++i) {
    PL_ASSIGN_OR_RETURN(left_on_columns_[i],
                        graph()->OptionallyCloneWithEdge(this, left_columns[i]));
  }
  right_on_columns_.resize(right_columns.size());
  for (size_t i = 0; i < right_columns.size(); ++i) {
    PL_ASSIGN_OR_RETURN(right_on_columns_[i],
                        graph()->OptionallyCloneWithEdge(this, right_columns[i]));
  }
  key_columns_set_ = true;
  return Status::OK();
}

StatusOr<std::vector<absl::flat_hash_set<std::string>>> JoinIR::RequiredInputColumns() const {
  DCHECK(key_columns_set_);
  DCHECK(!output_columns_.empty());

  std::vector<absl::flat_hash_set<std::string>> ret(2);

  for (ColumnIR* col : output_columns_) {
    DCHECK(col->container_op_parent_idx_set());
    ret[col->container_op_parent_idx()].insert(col->col_name());
  }
  for (ColumnIR* col : left_on_columns_) {
    DCHECK(col->container_op_parent_idx_set());
    ret[col->container_op_parent_idx()].insert(col->col_name());
  }
  for (ColumnIR* col : right_on_columns_) {
    DCHECK(col->container_op_parent_idx_set());
    ret[col->container_op_parent_idx()].insert(col->col_name());
  }

  return ret;
}

Status JoinIR::SetOutputColumns(const std::vector<std::string>& column_names,
                                const std::vector<ColumnIR*>& columns) {
  DCHECK_EQ(column_names.size(), columns.size());
  auto old_output_cols = output_columns_;

  output_columns_ = columns;
  column_names_ = column_names;

  for (auto old_col : old_output_cols) {
    PL_RETURN_IF_ERROR(graph()->DeleteEdge(this, old_col));
  }
  for (auto new_col : output_columns_) {
    PL_RETURN_IF_ERROR(graph()->AddEdge(this, new_col));
  }
  for (auto old_col : old_output_cols) {
    PL_RETURN_IF_ERROR(graph()->DeleteOrphansInSubtree(old_col->id()));
  }
  return Status::OK();
}

StatusOr<absl::flat_hash_set<std::string>> JoinIR::PruneOutputColumnsToImpl(
    const absl::flat_hash_set<std::string>& kept_columns) {
  DCHECK(!column_names_.empty());
  std::vector<ColumnIR*> new_output_cols;
  std::vector<std::string> new_output_names;
  for (const auto& [col_idx, col_name] : Enumerate(column_names_)) {
    if (kept_columns.contains(col_name)) {
      new_output_names.push_back(col_name);
      new_output_cols.push_back(output_columns_[col_idx]);
    }
  }
  PL_RETURN_IF_ERROR(SetOutputColumns(new_output_names, new_output_cols));
  return kept_columns;
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
}  // namespace planner
}  // namespace carnot
}  // namespace px
