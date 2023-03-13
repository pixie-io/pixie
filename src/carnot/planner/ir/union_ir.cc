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

#include "src/carnot/planner/ir/union_ir.h"
#include "src/carnot/planner/ir/column_ir.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/ir_node.h"

namespace px {
namespace carnot {
namespace planner {

Status UnionIR::CopyFromNodeImpl(const IRNode* node,
                                 absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const UnionIR* union_ir = static_cast<const UnionIR*>(node);
  std::vector<InputColumnMapping> dest_mappings;
  for (const InputColumnMapping& src_mapping : union_ir->column_mappings_) {
    InputColumnMapping dest_mapping;
    for (ColumnIR* src_col : src_mapping) {
      PX_ASSIGN_OR_RETURN(ColumnIR * dest_col, graph()->CopyNode(src_col, copied_nodes_map));
      dest_mapping.push_back(dest_col);
    }
    dest_mappings.push_back(dest_mapping);
  }
  return SetColumnMappings(dest_mappings);
}

Status UnionIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_union_op();
  op->set_op_type(planpb::UNION_OPERATOR);

  auto names = resolved_table_type()->ColumnNames();
  DCHECK(is_type_resolved());
  for (auto col_name : names) {
    pb->add_column_names(col_name);
  }
  if (default_column_mapping_) {
    for (size_t parent_i = 0; parent_i < parents().size(); ++parent_i) {
      auto* pb_column_mapping = pb->add_column_mappings();
      for (size_t col_i = 0; col_i < names.size(); ++col_i) {
        pb_column_mapping->add_column_indexes(col_i);
      }
    }
    return Status::OK();
  }
  DCHECK_EQ(parents().size(), column_mappings_.size()) << "parents and column_mappings disagree.";
  DCHECK(HasColumnMappings());

  for (const auto& column_mapping : column_mappings_) {
    auto* pb_column_mapping = pb->add_column_mappings();
    for (const ColumnIR* col : column_mapping) {
      PX_ASSIGN_OR_RETURN(auto index, col->GetColumnIndex());
      pb_column_mapping->add_column_indexes(index);
    }
  }

  // NOTE: not setting value as this is set in the execution engine. Keeping this here in case it
  // needs to be modified in the future.
  // pb->set_rows_per_batch(1024);

  return Status::OK();
}

Status UnionIR::AddColumnMapping(const InputColumnMapping& column_mapping) {
  InputColumnMapping cloned_mapping;
  for (ColumnIR* col : column_mapping) {
    PX_ASSIGN_OR_RETURN(auto cloned, graph()->OptionallyCloneWithEdge(this, col));
    cloned_mapping.push_back(cloned);
  }

  column_mappings_.push_back(cloned_mapping);
  return Status::OK();
}

Status UnionIR::SetColumnMappings(const std::vector<InputColumnMapping>& column_mappings) {
  auto old_mappings = column_mappings_;
  column_mappings_.clear();
  for (const auto& old_mapping : old_mappings) {
    for (ColumnIR* col : old_mapping) {
      PX_RETURN_IF_ERROR(graph()->DeleteEdge(this, col));
    }
  }
  for (const auto& new_mapping : column_mappings) {
    PX_RETURN_IF_ERROR(AddColumnMapping(new_mapping));
  }
  for (const auto& old_mapping : old_mappings) {
    for (ColumnIR* col : old_mapping) {
      PX_RETURN_IF_ERROR(graph()->DeleteOrphansInSubtree(col->id()));
    }
  }
  return Status::OK();
}

Status UnionIR::SetDefaultColumnMapping() {
  if (!is_type_resolved()) {
    return CreateIRNodeError("Type is not initialized yet");
  }
  DCHECK(!HasColumnMappings())
      << "Trying to set default column mapping on a Union that has a default column mapping.";
  default_column_mapping_ = true;
  return Status::OK();
}

StatusOr<std::vector<absl::flat_hash_set<std::string>>> UnionIR::RequiredInputColumns() const {
  DCHECK(HasColumnMappings());
  std::vector<absl::flat_hash_set<std::string>> ret;
  for (const auto& column_mapping : column_mappings_) {
    absl::flat_hash_set<std::string> parent_names;
    for (const ColumnIR* input_col : column_mapping) {
      parent_names.insert(input_col->col_name());
    }
    ret.push_back(parent_names);
  }
  return ret;
}

Status UnionIR::Init(const std::vector<OperatorIR*>& parents) {
  // Support joining a table against itself by calling HandleDuplicateParents.
  PX_ASSIGN_OR_RETURN(auto transformed_parents, HandleDuplicateParents(parents));
  for (auto p : transformed_parents) {
    PX_RETURN_IF_ERROR(AddParent(p));
  }
  return Status::OK();
}

StatusOr<absl::flat_hash_set<std::string>> UnionIR::PruneOutputColumnsToImpl(
    const absl::flat_hash_set<std::string>& kept_columns) {
  std::vector<InputColumnMapping> new_column_mappings;
  for (const auto& parent_column_mapping : column_mappings_) {
    InputColumnMapping new_parent_mapping;
    for (ColumnIR* parent_col : parent_column_mapping) {
      // Union matches parent columns with each other by name, but they may be in different orders
      // in the parents.
      if (kept_columns.contains(parent_col->col_name())) {
        new_parent_mapping.push_back(parent_col);
      }
    }
    new_column_mappings.push_back(new_parent_mapping);
  }
  PX_RETURN_IF_ERROR(SetColumnMappings(new_column_mappings));
  return kept_columns;
}

static inline std::string TypeToOldStyleDebugString(std::shared_ptr<TableType> type) {
  std::vector<std::string> type_parts;
  for (const auto& [col_name, col_type] : *type) {
    DCHECK(col_type->IsValueType());
    auto val_type = std::static_pointer_cast<ValueType>(col_type);
    std::string data_type(ToString(val_type->data_type()));
    type_parts.push_back(col_name + ":" + data_type);
  }
  return "[" + absl::StrJoin(type_parts, ", ") + "]";
}

Status UnionIR::UpdateOpAfterParentTypesResolvedImpl() {
  DCHECK_GT(parents().size(), 0U);
  auto base_parent_type = parents()[0]->resolved_table_type();

  std::vector<InputColumnMapping> mappings;
  for (const auto& [parent_idx, parent] : Enumerate(parents())) {
    auto parent_type = parent->resolved_table_type();

    if (parent_type->ColumnNames().size() != base_parent_type->ColumnNames().size()) {
      return CreateIRNodeError(
          "Table schema disagreement between parent ops $0 and $1 of $2. $0: $3 vs $1: $4. $5",
          parents()[0]->DebugString(), parent->DebugString(), DebugString(),
          TypeToOldStyleDebugString(base_parent_type), TypeToOldStyleDebugString(parent_type),
          "Column count wrong.");
    }

    InputColumnMapping column_mapping;
    for (const auto& [col_name, col_type] : *base_parent_type) {
      DCHECK(col_type->IsValueType());
      auto base_col_val_type = std::static_pointer_cast<ValueType>(col_type);
      if (!parent_type->HasColumn(col_name)) {
        return CreateIRNodeError(
            "Table schema disagreement between parent ops $0 and $1 of $2. $0: $3 vs $1: $4. $5",
            parents()[0]->DebugString(), parent->DebugString(), DebugString(),
            TypeToOldStyleDebugString(base_parent_type), TypeToOldStyleDebugString(parent_type),
            absl::Substitute("Missing '$0'.", col_name));
      }
      auto col_val_type = std::static_pointer_cast<ValueType>(
          parent_type->GetColumnType(col_name).ConsumeValueOrDie());
      if (col_val_type->data_type() != base_col_val_type->data_type()) {
        return CreateIRNodeError(
            "Table schema disagreement between parent ops $0 and $1 of $2. $0: $3 vs $1: $4. $5",
            parents()[0]->DebugString(), parent->DebugString(), DebugString(),
            TypeToOldStyleDebugString(base_parent_type), TypeToOldStyleDebugString(parent_type),
            absl::Substitute("wrong type for '$0'.", col_name));
      }
      PX_ASSIGN_OR_RETURN(auto col_node, graph()->CreateNode<ColumnIR>(
                                             ast(), col_name, /*parent_op_idx*/ parent_idx));
      column_mapping.push_back(col_node);
    }
    mappings.push_back(column_mapping);
  }
  return SetColumnMappings(mappings);
}

Status UnionIR::ResolveType(CompilerState* /* compiler_state */) {
  DCHECK_LE(1U, parent_types().size());
  // The types were checked in UpdateOpAfterParentTypesResolved, so no need to check here.
  auto type = parent_types()[0]->Copy();
  return SetResolvedType(type);
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
