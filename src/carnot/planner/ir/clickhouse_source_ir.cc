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

#include "src/carnot/planner/ir/clickhouse_source_ir.h"

#include <clickhouse/client.h>

#include "src/carnot/planner/ir/ir.h"

namespace px {
namespace carnot {
namespace planner {

std::string ClickHouseSourceIR::DebugString() const {
  return absl::Substitute("$0(id=$1, table=$2)", type_string(), id(), table_name_);
}

Status ClickHouseSourceIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_clickhouse_source_op();
  op->set_op_type(planpb::CLICKHOUSE_SOURCE_OPERATOR);

  // Set ClickHouse connection parameters from stored values
  pb->set_host(host_);
  pb->set_port(port_);
  pb->set_username(username_);
  pb->set_password(password_);
  pb->set_database(database_);

  // Build the query
  pb->set_query(absl::Substitute("SELECT * FROM $0", table_name_));

  if (!column_index_map_set()) {
    return error::InvalidArgument("ClickHouseSource columns are not set.");
  }

  DCHECK(is_type_resolved());
  DCHECK_EQ(column_index_map_.size(), resolved_table_type()->ColumnNames().size());
  for (const auto& [idx, col_name] : Enumerate(resolved_table_type()->ColumnNames())) {
    pb->add_column_names(col_name);
    auto val_type = std::static_pointer_cast<ValueType>(
        resolved_table_type()->GetColumnType(col_name).ConsumeValueOrDie());
    pb->add_column_types(val_type->data_type());
  }

  if (IsTimeStartSet()) {
    pb->set_start_time(time_start_ns());
  }

  if (IsTimeStopSet()) {
    pb->set_end_time(time_stop_ns());
  }

  // Set batch size
  pb->set_batch_size(1024);

  // Set timestamp and partition columns from stored values
  pb->set_timestamp_column(timestamp_column_);
  pb->set_partition_column("hostname");

  return Status::OK();
}

Status ClickHouseSourceIR::Init(const std::string& table_name,
                                const std::vector<std::string>& select_columns,
                                const std::string& host, int port,
                                const std::string& username, const std::string& password,
                                const std::string& database,
                                const std::string& timestamp_column) {
  table_name_ = table_name;
  column_names_ = select_columns;
  host_ = host;
  port_ = port;
  username_ = username;
  password_ = password;
  database_ = database;
  timestamp_column_ = timestamp_column;
  return Status::OK();
}

StatusOr<absl::flat_hash_set<std::string>> ClickHouseSourceIR::PruneOutputColumnsToImpl(
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

Status ClickHouseSourceIR::CopyFromNodeImpl(const IRNode* node,
                                            absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const ClickHouseSourceIR* source_ir = static_cast<const ClickHouseSourceIR*>(node);

  table_name_ = source_ir->table_name_;
  time_start_ns_ = source_ir->time_start_ns_;
  time_stop_ns_ = source_ir->time_stop_ns_;
  column_names_ = source_ir->column_names_;
  column_index_map_set_ = source_ir->column_index_map_set_;
  column_index_map_ = source_ir->column_index_map_;

  username_ = source_ir->username_;
  password_ = source_ir->password_;
  database_ = source_ir->database_;
  port_ = source_ir->port_;
  host_ = source_ir->host_;

  return Status::OK();
}

StatusOr<types::DataType> ClickHouseSourceIR::ClickHouseTypeToPixieType(
    const std::string& ch_type_name) {
  // Integer types - Pixie only supports INT64
  if (ch_type_name == "UInt8" || ch_type_name == "UInt16" || ch_type_name == "UInt32" ||
      ch_type_name == "UInt64" || ch_type_name == "Int8" || ch_type_name == "Int16" ||
      ch_type_name == "Int32" || ch_type_name == "Int64") {
    return types::DataType::INT64;
  }
  // UInt128
  if (ch_type_name == "UInt128") {
    return types::DataType::UINT128;
  }
  // Floating point types - Pixie only supports FLOAT64
  if (ch_type_name == "Float32" || ch_type_name == "Float64") {
    return types::DataType::FLOAT64;
  }
  // String types
  if (ch_type_name == "String" || ch_type_name == "FixedString" ||
      absl::StartsWith(ch_type_name, "FixedString(")) {
    return types::DataType::STRING;
  }
  // Date/time types
  if (ch_type_name == "DateTime" || absl::StartsWith(ch_type_name, "DateTime64")) {
    return types::DataType::TIME64NS;
  }
  // Boolean type (stored as UInt8 in ClickHouse)
  if (ch_type_name == "Bool") {
    return types::DataType::BOOLEAN;
  }
  // Default to String for unsupported types
  return types::DataType::STRING;
}

StatusOr<table_store::schema::Relation> ClickHouseSourceIR::InferRelationFromClickHouse(
    CompilerState* compiler_state, const std::string& table_name) {
  // Check if ClickHouse config is available
  // TODO(ddelnano): Add this check in when the configuration plumbing is done.
  auto* ch_config = compiler_state->clickhouse_config();
  PX_UNUSED(ch_config);

  // Use stored connection parameters from Init()

  clickhouse::ClientOptions options;
  options.SetHost(host_);
  options.SetPort(port_);
  options.SetUser(username_);
  options.SetPassword(password_);
  options.SetDefaultDatabase(database_);

  // Create ClickHouse client
  std::unique_ptr<clickhouse::Client> client;
  try {
    client = std::make_unique<clickhouse::Client>(options);
  } catch (const std::exception& e) {
    return error::Internal("Failed to connect to ClickHouse at $0:$1 - $2",
                          host_, port_, e.what());
  }

  // Query ClickHouse for table schema using DESCRIBE TABLE
  std::string describe_query = absl::Substitute("DESCRIBE TABLE $0", table_name);

  table_store::schema::Relation relation;
  bool query_executed = false;

  try {
    client->Select(describe_query, [&](const clickhouse::Block& block) {
      query_executed = true;
      // DESCRIBE TABLE returns columns: name, type, default_type, default_expression, comment,
      // codec_expression, ttl_expression
      size_t num_rows = block.GetRowCount();

      if (num_rows == 0) {
        return;
      }

      // Get the column name and type columns
      auto name_column = block[0]->As<clickhouse::ColumnString>();
      auto type_column = block[1]->As<clickhouse::ColumnString>();

      for (size_t i = 0; i < num_rows; ++i) {
        std::string col_name = std::string(name_column->At(i));
        std::string col_type = std::string(type_column->At(i));

        // Convert ClickHouse type to Pixie type
        auto pixie_type_or = ClickHouseTypeToPixieType(col_type);
        if (!pixie_type_or.ok()) {
          LOG(WARNING) << "Failed to convert ClickHouse type '" << col_type
                       << "' for column '" << col_name << "'. Using STRING as fallback.";
          relation.AddColumn(types::DataType::STRING, col_name, types::SemanticType::ST_NONE);
        } else {
          types::DataType pixie_type = pixie_type_or.ConsumeValueOrDie();
          // Determine semantic type based on column name or type
          types::SemanticType semantic_type = types::SemanticType::ST_NONE;
          if (pixie_type == types::DataType::TIME64NS) {
            semantic_type = types::SemanticType::ST_TIME_NS;
          }
          relation.AddColumn(pixie_type, col_name, semantic_type);
        }
      }
    });
  } catch (const std::exception& e) {
    return error::Internal("Failed to query ClickHouse table schema for '$0': $1",
                          table_name, e.what());
  }

  if (!query_executed || relation.NumColumns() == 0) {
    return error::Internal("Table '$0' not found in ClickHouse or has no columns.", table_name);
  }

  return relation;
}

Status ClickHouseSourceIR::ResolveType(CompilerState* compiler_state) {
  table_store::schema::Relation table_relation;

  auto existing_relation = false;
  auto relation_it = compiler_state->relation_map()->find(table_name());
  if (relation_it == compiler_state->relation_map()->end()) {
    // Table not found in relation_map, try to infer from ClickHouse
    LOG(INFO) << absl::Substitute("Table '$0' not found in relation_map. Attempting to infer schema from ClickHouse...", table_name());

    auto relation_or = InferRelationFromClickHouse(compiler_state, table_name());
    if (!relation_or.ok()) {
      return CreateIRNodeError("Table '$0' not found in relation_map and failed to infer from ClickHouse: $1",
                              table_name_, relation_or.status().msg());
    }

    table_relation = relation_or.ConsumeValueOrDie();
  } else {
    table_relation = relation_it->second;
    existing_relation = true;
  }
  auto full_table_type = TableType::Create(table_relation);
  if (select_all()) {
    // For select_all, add all table columns plus ClickHouse-added columns (hostname, event_time)
    std::vector<int64_t> column_indices;
    int64_t table_column_count = static_cast<int64_t>(table_relation.NumColumns());

    // Add all table columns
    for (int64_t i = 0; i < table_column_count; ++i) {
      column_indices.push_back(i);
    }

    // Add ClickHouse-added columns
    if (existing_relation) {
      full_table_type->AddColumn("hostname", ValueType::Create(types::DataType::STRING, types::SemanticType::ST_NONE));
      column_indices.push_back(table_column_count);  // hostname is after all table columns

      full_table_type->AddColumn("event_time", ValueType::Create(types::DataType::TIME64NS, types::SemanticType::ST_TIME_NS));
      column_indices.push_back(table_column_count + 1);  // event_time is after hostname
    }

    SetColumnIndexMap(column_indices);
    return SetResolvedType(full_table_type);
  }

  std::vector<int64_t> column_indices;
  auto new_table = TableType::Create();

  // Calculate the index offset for ClickHouse-added columns (after all table columns)
  int64_t table_column_count = static_cast<int64_t>(table_relation.NumColumns());
  auto next_count = 0;

  for (const auto& col_name : column_names_) {
    // Handle special ClickHouse-added columns that don't exist in the source table
    if (col_name == "hostname") {
      new_table->AddColumn(col_name, ValueType::Create(types::DataType::STRING, types::SemanticType::ST_NONE));
      // hostname is added by ClickHouse after all table columns
      column_indices.push_back(table_column_count + (next_count++));
      continue;
    }
    if (col_name == "event_time") {
      new_table->AddColumn(col_name, ValueType::Create(types::DataType::TIME64NS, types::SemanticType::ST_TIME_NS));
      // event_time is added by ClickHouse after hostname
      column_indices.push_back(table_column_count + (next_count++));
      continue;
    }

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
