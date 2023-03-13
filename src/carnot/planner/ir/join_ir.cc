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
#include <map>
#include <memory>

#include "src/carnot/planner/ir/column_ir.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/join_ir.h"

namespace px {
namespace carnot {
namespace planner {

Status JoinIR::CopyFromNodeImpl(const IRNode* node,
                                absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const JoinIR* join_node = static_cast<const JoinIR*>(node);
  join_type_ = join_node->join_type_;

  std::vector<ColumnIR*> new_output_columns;
  for (const ColumnIR* col : join_node->output_columns_) {
    PX_ASSIGN_OR_RETURN(ColumnIR * new_node, graph()->CopyNode(col, copied_nodes_map));
    new_output_columns.push_back(new_node);
  }
  PX_RETURN_IF_ERROR(SetOutputColumns(join_node->column_names_, new_output_columns));

  std::vector<ColumnIR*> new_left_columns;
  for (const ColumnIR* col : join_node->left_on_columns_) {
    PX_ASSIGN_OR_RETURN(ColumnIR * new_node, graph()->CopyNode(col, copied_nodes_map));
    new_left_columns.push_back(new_node);
  }

  std::vector<ColumnIR*> new_right_columns;
  for (const ColumnIR* col : join_node->right_on_columns_) {
    PX_ASSIGN_OR_RETURN(ColumnIR * new_node, graph()->CopyNode(col, copied_nodes_map));
    new_right_columns.push_back(new_node);
  }

  PX_RETURN_IF_ERROR(SetJoinColumns(new_left_columns, new_right_columns));
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
    PX_ASSIGN_OR_RETURN(auto left_index, left_on_columns_[i]->GetColumnIndex());
    PX_ASSIGN_OR_RETURN(auto right_index, right_on_columns_[i]->GetColumnIndex());
    eq_condition->set_left_column_index(left_index);
    eq_condition->set_right_column_index(right_index);
  }

  for (ColumnIR* col : output_columns_) {
    auto* parent_col = pb->add_output_columns();
    int64_t parent_idx = col->container_op_parent_idx();
    DCHECK_LT(parent_idx, 2);
    parent_col->set_parent_index(col->container_op_parent_idx());
    DCHECK(col->IsDataTypeEvaluated()) << "Column not evaluated";
    PX_ASSIGN_OR_RETURN(auto index, col->GetColumnIndex());
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
  PX_ASSIGN_OR_RETURN(auto transformed_parents, HandleDuplicateParents(parents));
  for (auto* p : transformed_parents) {
    PX_RETURN_IF_ERROR(AddParent(p));
  }

  PX_RETURN_IF_ERROR(SetJoinColumns(left_on_cols, right_on_cols));

  suffix_strs_ = suffix_strs;
  return SetJoinType(how_type);
}

Status JoinIR::SetJoinColumns(const std::vector<ColumnIR*>& left_columns,
                              const std::vector<ColumnIR*>& right_columns) {
  DCHECK(left_on_columns_.empty());
  DCHECK(right_on_columns_.empty());
  left_on_columns_.resize(left_columns.size());
  for (size_t i = 0; i < left_columns.size(); ++i) {
    PX_ASSIGN_OR_RETURN(left_on_columns_[i],
                        graph()->OptionallyCloneWithEdge(this, left_columns[i]));
  }
  right_on_columns_.resize(right_columns.size());
  for (size_t i = 0; i < right_columns.size(); ++i) {
    PX_ASSIGN_OR_RETURN(right_on_columns_[i],
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
    PX_RETURN_IF_ERROR(graph()->DeleteEdge(this, old_col));
  }
  for (auto new_col : output_columns_) {
    PX_RETURN_IF_ERROR(graph()->AddEdge(this, new_col));
  }
  for (auto old_col : old_output_cols) {
    PX_RETURN_IF_ERROR(graph()->DeleteOrphansInSubtree(old_col->id()));
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
  PX_RETURN_IF_ERROR(SetOutputColumns(new_output_names, new_output_cols));
  return kept_columns;
}

Status JoinIR::UpdateOpAfterParentTypesResolvedImpl() {
  DCHECK_EQ(2UL, parents().size());
  const auto& [left_type, right_type] = left_right_table_types();
  const auto& [left_suffix, right_suffix] = left_right_suffixs();

  absl::flat_hash_set<std::string> left_column_names(left_type->ColumnNames().begin(),
                                                     left_type->ColumnNames().end());
  DCHECK_EQ(left_column_names.size(), left_type->ColumnNames().size())
      << "Left table type has duplicate columns, should have caught this earlier.";

  absl::flat_hash_set<std::string> duplicate_column_names;
  struct LeftRightIndex {
    size_t index;
    bool is_left;
  };
  std::multimap<std::string, LeftRightIndex> column_name_to_index;
  size_t idx = 0;
  for (auto col_name : left_type->ColumnNames()) {
    column_name_to_index.emplace(col_name, LeftRightIndex{idx, true});
    idx++;
  }
  for (auto col_name : right_type->ColumnNames()) {
    column_name_to_index.emplace(col_name, LeftRightIndex{idx, false});
    idx++;

    if (left_column_names.contains(col_name)) {
      duplicate_column_names.insert(col_name);
    }
  }

  // Resolve any of the duplicates, check to see if there are duplicates afterwards
  for (auto dup_name : duplicate_column_names) {
    // Since dup_name is always in column_name_to_index, left_index, and right_index can never be
    // uninitialized after the for loop below, but gcc isn't smart enough to know this so we have to
    // initialize them.
    size_t left_index = 0;
    size_t right_index = 0;
    auto index_range = column_name_to_index.equal_range(dup_name);
    for (auto it = index_range.first; it != index_range.second; ++it) {
      if (it->second.is_left) {
        left_index = it->second.index;
      } else {
        right_index = it->second.index;
      }
    }
    column_name_to_index.erase(dup_name);

    std::string left_column = absl::Substitute("$0$1", dup_name, left_suffix);
    std::string right_column = absl::Substitute("$0$1", dup_name, right_suffix);

    std::string err_string = absl::Substitute(
        "duplicate column '$0' after merge. Change the specified suffixes ('$1','$2') to fix "
        "this",
        "$0", left_suffix, right_suffix);

    // Make sure that the new left_column doesn't already exist in the column names.
    if (column_name_to_index.count(left_column)) {
      return CreateIRNodeError(err_string, left_column);
    }
    // Insert before checking right column to make sure left_column != right_column. Saves a check.
    column_name_to_index.emplace(left_column, LeftRightIndex{left_index, true});
    if (column_name_to_index.count(right_column)) {
      return CreateIRNodeError(err_string, right_column);
    }
    column_name_to_index.emplace(right_column, LeftRightIndex{right_index, false});
  }
  std::vector<std::string> output_column_names;
  output_column_names.resize(column_name_to_index.size());
  for (const auto& [col_name, lr_index] : column_name_to_index) {
    output_column_names[lr_index.index] = col_name;
  }

  int64_t left_idx = 0;
  int64_t right_idx = 1;
  if (specified_as_right()) {
    left_idx = 1;
    right_idx = 0;
  }
  std::vector<ColumnIR*> output_columns;
  for (auto col_name : left_type->ColumnNames()) {
    PX_ASSIGN_OR_RETURN(ColumnIR * col, graph()->CreateNode<ColumnIR>(ast(), col_name, left_idx));
    output_columns.push_back(col);
  }
  for (auto col_name : right_type->ColumnNames()) {
    PX_ASSIGN_OR_RETURN(ColumnIR * col, graph()->CreateNode<ColumnIR>(ast(), col_name, right_idx));
    output_columns.push_back(col);
  }

  return SetOutputColumns(output_column_names, output_columns);
}

Status JoinIR::ResolveType(CompilerState* compiler_state) {
  DCHECK_EQ(2U, parent_types().size());

  // Check that the join_on columns have the same types.
  for (const auto& [idx, left_col] : Enumerate(left_on_columns_)) {
    // Init checks that `left_on` and `right_on` are the same length.
    auto right_col = right_on_columns_[idx];
    PX_RETURN_IF_ERROR(ResolveExpressionType(left_col, compiler_state, parent_types()));
    PX_RETURN_IF_ERROR(ResolveExpressionType(right_col, compiler_state, parent_types()));
    auto left_col_dt = std::static_pointer_cast<ValueType>(left_col->resolved_type())->data_type();
    auto right_col_dt =
        std::static_pointer_cast<ValueType>(right_col->resolved_type())->data_type();
    if (left_col_dt != right_col_dt) {
      return CreateIRNodeError(
          R"(join columns specified in `left_on` and `right_on` must have the same datatype, but the $0-th columns disagree. ["$1" ($2) vs "$3" ($4)])",
          idx, left_col->col_name(), magic_enum::enum_name(left_col_dt), right_col->col_name(),
          magic_enum::enum_name(right_col_dt));
    }
  }

  auto new_table = TableType::Create();
  for (const auto& [idx, col] : Enumerate(output_columns_)) {
    PX_RETURN_IF_ERROR(ResolveExpressionType(col, compiler_state, parent_types()));
    auto col_name = column_names_[idx];
    new_table->AddColumn(col_name, col->resolved_type());
  }

  return SetResolvedType(new_table);
}
}  // namespace planner
}  // namespace carnot
}  // namespace px
