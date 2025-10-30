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

#include "src/carnot/planner/ir/clickhouse_export_sink_ir.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planpb/plan.pb.h"

namespace px {
namespace carnot {
namespace planner {

StatusOr<std::vector<absl::flat_hash_set<std::string>>>
ClickHouseExportSinkIR::RequiredInputColumns() const {
  return std::vector<absl::flat_hash_set<std::string>>{required_column_names_};
}

Status ClickHouseExportSinkIR::ToProto(planpb::Operator* op) const {
  op->set_op_type(planpb::CLICKHOUSE_EXPORT_SINK_OPERATOR);
  auto clickhouse_op = op->mutable_clickhouse_sink_op();

  // ClickHouse config must be set before calling ToProto
  if (clickhouse_config_ == nullptr) {
    return error::InvalidArgument("ClickHouse config not set");
  }

  // Set the ClickHouse configuration
  *clickhouse_op->mutable_clickhouse_config() = *clickhouse_config_;
  clickhouse_op->set_table_name(table_name_);

  // Map all input columns to ClickHouse columns
  DCHECK_EQ(1U, parent_types().size());
  auto parent_table_type = std::static_pointer_cast<TableType>(parent_types()[0]);

  for (const auto& [idx, col_name] : Enumerate(parent_table_type->ColumnNames())) {
    auto column_mapping = clickhouse_op->add_column_mappings();
    column_mapping->set_input_column_index(idx);
    column_mapping->set_clickhouse_column_name(col_name);

    PX_ASSIGN_OR_RETURN(auto col_type, parent_table_type->GetColumnType(col_name));
    auto value_type = std::static_pointer_cast<ValueType>(col_type);
    column_mapping->set_column_type(value_type->data_type());
  }

  return Status::OK();
}

Status ClickHouseExportSinkIR::CopyFromNodeImpl(
    const IRNode* node, absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const ClickHouseExportSinkIR* source = static_cast<const ClickHouseExportSinkIR*>(node);
  table_name_ = source->table_name_;
  required_column_names_ = source->required_column_names_;
  if (source->clickhouse_config_ != nullptr) {
    clickhouse_config_ = std::make_unique<planpb::ClickHouseConfig>(*source->clickhouse_config_);
  }
  return Status::OK();
}

Status ClickHouseExportSinkIR::ResolveType(CompilerState* compiler_state) {
  DCHECK_EQ(1U, parent_types().size());

  auto parent_table_type = std::static_pointer_cast<TableType>(parent_types()[0]);

  // Store ClickHouse config from compiler state
  if (compiler_state->clickhouse_config() != nullptr) {
    clickhouse_config_ = std::make_unique<planpb::ClickHouseConfig>(*compiler_state->clickhouse_config());
  }

  // Populate required column names
  for (const auto& col_name : parent_table_type->ColumnNames()) {
    required_column_names_.insert(col_name);
  }

  // Export sink passes through the input schema
  return SetResolvedType(parent_table_type);
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
