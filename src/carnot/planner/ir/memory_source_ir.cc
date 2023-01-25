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

#include "src/carnot/planner/ir/memory_source_ir.h"
#include "src/carnot/planner/ir/ir.h"

namespace px {
namespace carnot {
namespace planner {

std::string MemorySourceIR::DebugString() const {
  return absl::Substitute("$0(id=$1, table=$2, streaming=$3)", type_string(), id(), table_name_,
                          streaming_);
}

Status MemorySourceIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_mem_source_op();
  op->set_op_type(planpb::MEMORY_SOURCE_OPERATOR);
  pb->set_name(table_name_);

  if (!column_index_map_set()) {
    return error::InvalidArgument("MemorySource columns are not set.");
  }

  DCHECK(is_type_resolved());
  DCHECK_EQ(column_index_map_.size(), resolved_table_type()->ColumnNames().size());
  for (const auto& [idx, col_name] : Enumerate(resolved_table_type()->ColumnNames())) {
    pb->add_column_idxs(column_index_map_[idx]);
    pb->add_column_names(col_name);
    auto val_type = std::static_pointer_cast<ValueType>(
        resolved_table_type()->GetColumnType(col_name).ConsumeValueOrDie());
    pb->add_column_types(val_type->data_type());
  }

  if (IsTimeStartSet()) {
    auto start_time = new ::google::protobuf::Int64Value();
    start_time->set_value(time_start_ns());
    pb->set_allocated_start_time(start_time);
  }

  if (IsTimeStopSet()) {
    auto stop_time = new ::google::protobuf::Int64Value();
    stop_time->set_value(time_stop_ns());
    pb->set_allocated_stop_time(stop_time);
  }

  if (HasTablet()) {
    pb->set_tablet(tablet_value());
  }

  pb->set_streaming(streaming());
  return Status::OK();
}

Status MemorySourceIR::Init(const std::string& table_name,
                            const std::vector<std::string>& select_columns) {
  table_name_ = table_name;
  column_names_ = select_columns;
  return Status::OK();
}

StatusOr<absl::flat_hash_set<std::string>> MemorySourceIR::PruneOutputColumnsToImpl(
    const absl::flat_hash_set<std::string>& output_colnames) {
  DCHECK(column_index_map_set());
  DCHECK(is_type_resolved());
  std::vector<std::string> new_col_names;
  std::vector<int64_t> new_col_index_map;

  auto col_names = resolved_table_type()->ColumnNames();
  for (const auto& [idx, name] : Enumerate(col_names)) {
    if (output_colnames.contains(name)) {
      new_col_names.push_back(name);
      new_col_index_map.push_back(column_index_map_[idx]);
    }
  }
  if (new_col_names != resolved_table_type()->ColumnNames()) {
    column_names_ = new_col_names;
  }
  column_index_map_ = new_col_index_map;
  return output_colnames;
}

Status MemorySourceIR::CopyFromNodeImpl(const IRNode* node,
                                        absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const MemorySourceIR* source_ir = static_cast<const MemorySourceIR*>(node);

  table_name_ = source_ir->table_name_;
  time_start_ns_ = source_ir->time_start_ns_;
  time_stop_ns_ = source_ir->time_stop_ns_;
  column_names_ = source_ir->column_names_;
  column_index_map_set_ = source_ir->column_index_map_set_;
  column_index_map_ = source_ir->column_index_map_;
  streaming_ = source_ir->streaming_;

  return Status::OK();
}

Status MemorySourceIR::ResolveType(CompilerState* compiler_state) {
  auto relation_it = compiler_state->relation_map()->find(table_name());
  if (relation_it == compiler_state->relation_map()->end()) {
    return CreateIRNodeError("Table '$0' not found.", table_name_);
  }
  auto table_relation = relation_it->second;
  auto full_table_type = TableType::Create(table_relation);
  if (select_all()) {
    std::vector<int64_t> column_indices;
    for (int64_t i = 0; i < static_cast<int64_t>(table_relation.NumColumns()); ++i) {
      column_indices.push_back(i);
    }
    SetColumnIndexMap(column_indices);
    return SetResolvedType(full_table_type);
  }

  std::vector<int64_t> column_indices;
  auto new_table = TableType::Create();
  for (const auto& col_name : column_names_) {
    PX_ASSIGN_OR_RETURN(auto col_type, full_table_type->GetColumnType(col_name));
    new_table->AddColumn(col_name, col_type);
    column_indices.push_back(table_relation.GetColumnIndex(col_name));
  }

  SetColumnIndexMap(column_indices);
  return SetResolvedType(new_table);
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
