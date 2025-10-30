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

#include "src/carnot/exec/clickhouse_source_node.h"

#include <arrow/array.h>
#include <arrow/builder.h>
#include <algorithm>
#include <cctype>

#include <absl/strings/str_join.h>
#include <absl/strings/substitute.h>

#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace exec {

std::string ClickHouseSourceNode::DebugStringImpl() {
  return absl::Substitute("Exec::ClickHouseSourceNode: <query: $0, output: $1>", base_query_,
                          output_descriptor_->DebugString());
}

Status ClickHouseSourceNode::InitImpl(const plan::Operator& plan_node) {
  CHECK(plan_node.op_type() == planpb::OperatorType::CLICKHOUSE_SOURCE_OPERATOR);
  const auto* source_plan_node = static_cast<const plan::ClickHouseSourceOperator*>(&plan_node);

  // Copy the plan node to local object
  plan_node_ = std::make_unique<plan::ClickHouseSourceOperator>(*source_plan_node);

  // Extract connection parameters from plan node
  host_ = plan_node_->host();
  port_ = plan_node_->port();
  username_ = plan_node_->username();
  password_ = plan_node_->password();
  database_ = plan_node_->database();
  base_query_ = plan_node_->query();
  batch_size_ = plan_node_->batch_size();
  streaming_ = plan_node_->streaming();

  // Initialize cursor state
  current_offset_ = 0;
  has_more_data_ = true;
  current_block_index_ = 0;

  // Extract time filtering parameters from plan node
  timestamp_column_ = plan_node_->timestamp_column();
  partition_column_ = plan_node_->partition_column();

  // Convert start/end times from nanoseconds to seconds for ClickHouse DateTime
  if (plan_node_->start_time() > 0) {
    start_time_ = plan_node_->start_time() / 1000000000LL;  // Convert ns to seconds
  }
  if (plan_node_->end_time() > 0) {
    end_time_ = plan_node_->end_time() / 1000000000LL;  // Convert ns to seconds
  }

  return Status::OK();
}

Status ClickHouseSourceNode::PrepareImpl(ExecState*) { return Status::OK(); }

Status ClickHouseSourceNode::OpenImpl(ExecState*) {
  // Create ClickHouse client
  clickhouse::ClientOptions options;
  options.SetHost(host_);
  options.SetPort(port_);
  options.SetUser(username_);
  options.SetPassword(password_);
  options.SetDefaultDatabase(database_);

  try {
    client_ = std::make_unique<clickhouse::Client>(options);
  } catch (const std::exception& e) {
    return error::Internal("Failed to create ClickHouse client: $0", e.what());
  }

  return Status::OK();
}

Status ClickHouseSourceNode::CloseImpl(ExecState*) {
  client_.reset();
  current_batch_blocks_.clear();

  // Reset cursor state
  current_offset_ = 0;
  current_block_index_ = 0;
  has_more_data_ = true;

  return Status::OK();
}

StatusOr<types::DataType> ClickHouseSourceNode::ClickHouseTypeToPixieType(
    const clickhouse::TypeRef& ch_type) {
  const auto& type_name = ch_type->GetName();

  // Integer types - Pixie only supports INT64
  if (type_name == "UInt8" || type_name == "UInt16" || type_name == "UInt32" ||
      type_name == "UInt64" || type_name == "Int8" || type_name == "Int16" ||
      type_name == "Int32" || type_name == "Int64") {
    return types::DataType::INT64;
  }

  // UInt128
  if (type_name == "UInt128") {
    return types::DataType::UINT128;
  }

  // Floating point types - Pixie only supports FLOAT64
  if (type_name == "Float32" || type_name == "Float64") {
    return types::DataType::FLOAT64;
  }

  // String types
  if (type_name == "String" || type_name == "FixedString") {
    return types::DataType::STRING;
  }

  // Date/time types
  if (type_name == "DateTime" || type_name.find("DateTime64") == 0) {
    return types::DataType::TIME64NS;
  }

  // Boolean
  if (type_name == "Bool") {
    return types::DataType::BOOLEAN;
  }

  return error::InvalidArgument("Unsupported ClickHouse type: $0", type_name);
}

StatusOr<std::unique_ptr<RowBatch>> ClickHouseSourceNode::ConvertClickHouseBlockToRowBatch(
    const clickhouse::Block& block, bool /*is_last_block*/) {
  auto num_rows = block.GetRowCount();
  auto num_cols = block.GetColumnCount();

  // Create output row descriptor if this is the first block
  if (current_block_index_ == 0) {
    std::vector<types::DataType> col_types;
    for (size_t i = 0; i < num_cols; ++i) {
      PX_ASSIGN_OR_RETURN(auto pixie_type, ClickHouseTypeToPixieType(block[i]->Type()));
      col_types.push_back(pixie_type);
    }
    // Note: In a real implementation, we would get column names from the plan
    // or from ClickHouse metadata
  }

  auto row_batch = std::make_unique<RowBatch>(*output_descriptor_, num_rows);

  // Convert each column
  for (size_t col_idx = 0; col_idx < num_cols; ++col_idx) {
    const auto& ch_column = block[col_idx];
    const auto& type_name = ch_column->Type()->GetName();

    // Check what the expected output type is for this column
    auto expected_type = output_descriptor_->type(col_idx);

    // For now, implement conversion for common types
    // This is where column type inference happens

    // Special case: String in ClickHouse that should be UINT128 in Pixie
    if (type_name == "String" && expected_type == types::DataType::UINT128) {
      auto typed_col = ch_column->As<clickhouse::ColumnString>();
      auto builder = types::MakeArrowBuilder(types::DataType::UINT128, arrow::default_memory_pool());
      PX_RETURN_IF_ERROR(builder->Reserve(num_rows));

      for (size_t i = 0; i < num_rows; ++i) {
        std::string value(typed_col->At(i));

        // Parse "high:low" format
        size_t colon_pos = value.find(':');
        if (colon_pos == std::string::npos) {
          return error::InvalidArgument("Invalid UINT128 string format: $0 (expected high:low)", value);
        }

        uint64_t high = std::stoull(value.substr(0, colon_pos));
        uint64_t low = std::stoull(value.substr(colon_pos + 1));
        absl::uint128 uint128_val = absl::MakeUint128(high, low);

        PX_RETURN_IF_ERROR(table_store::schema::CopyValue<types::DataType::UINT128>(builder.get(), uint128_val));
      }

      std::shared_ptr<arrow::Array> array;
      PX_RETURN_IF_ERROR(builder->Finish(&array));
      PX_RETURN_IF_ERROR(row_batch->AddColumn(array));

      continue;
    }

    // Integer types - all map to INT64 in Pixie

    // TODO(ddelnano): UInt8 is a special case since it can map to Pixie's boolean type.
    // Figure out how to handle that properly
    if (type_name == "UInt8") {
      auto typed_col = ch_column->As<clickhouse::ColumnUInt8>();
      arrow::BooleanBuilder builder;
      PX_RETURN_IF_ERROR(builder.Reserve(num_rows));
      for (size_t i = 0; i < num_rows; ++i) {
        builder.UnsafeAppend(typed_col->At(i) != 0);
      }
      std::shared_ptr<arrow::Array> array;
      PX_RETURN_IF_ERROR(builder.Finish(&array));
      PX_RETURN_IF_ERROR(row_batch->AddColumn(array));
    } else if (type_name == "UInt16") {
      auto typed_col = ch_column->As<clickhouse::ColumnUInt16>();
      arrow::Int64Builder builder;
      PX_RETURN_IF_ERROR(builder.Reserve(num_rows));
      for (size_t i = 0; i < num_rows; ++i) {
        builder.UnsafeAppend(static_cast<int64_t>(typed_col->At(i)));
      }
      std::shared_ptr<arrow::Array> array;
      PX_RETURN_IF_ERROR(builder.Finish(&array));
      PX_RETURN_IF_ERROR(row_batch->AddColumn(array));
    } else if (type_name == "UInt32") {
      auto typed_col = ch_column->As<clickhouse::ColumnUInt32>();
      arrow::Int64Builder builder;
      PX_RETURN_IF_ERROR(builder.Reserve(num_rows));
      for (size_t i = 0; i < num_rows; ++i) {
        builder.UnsafeAppend(static_cast<int64_t>(typed_col->At(i)));
      }
      std::shared_ptr<arrow::Array> array;
      PX_RETURN_IF_ERROR(builder.Finish(&array));
      PX_RETURN_IF_ERROR(row_batch->AddColumn(array));
    } else if (type_name == "UInt64") {
      auto typed_col = ch_column->As<clickhouse::ColumnUInt64>();
      arrow::Int64Builder builder;
      PX_RETURN_IF_ERROR(builder.Reserve(num_rows));
      for (size_t i = 0; i < num_rows; ++i) {
        builder.UnsafeAppend(static_cast<int64_t>(typed_col->At(i)));
      }
      std::shared_ptr<arrow::Array> array;
      PX_RETURN_IF_ERROR(builder.Finish(&array));
      PX_RETURN_IF_ERROR(row_batch->AddColumn(array));
    } else if (type_name == "Int8") {
      auto typed_col = ch_column->As<clickhouse::ColumnInt8>();
      arrow::Int64Builder builder;
      PX_RETURN_IF_ERROR(builder.Reserve(num_rows));
      for (size_t i = 0; i < num_rows; ++i) {
        builder.UnsafeAppend(static_cast<int64_t>(typed_col->At(i)));
      }
      std::shared_ptr<arrow::Array> array;
      PX_RETURN_IF_ERROR(builder.Finish(&array));
      PX_RETURN_IF_ERROR(row_batch->AddColumn(array));
    } else if (type_name == "Int16") {
      auto typed_col = ch_column->As<clickhouse::ColumnInt16>();
      arrow::Int64Builder builder;
      PX_RETURN_IF_ERROR(builder.Reserve(num_rows));
      for (size_t i = 0; i < num_rows; ++i) {
        builder.UnsafeAppend(static_cast<int64_t>(typed_col->At(i)));
      }
      std::shared_ptr<arrow::Array> array;
      PX_RETURN_IF_ERROR(builder.Finish(&array));
      PX_RETURN_IF_ERROR(row_batch->AddColumn(array));
    } else if (type_name == "Int32") {
      auto typed_col = ch_column->As<clickhouse::ColumnInt32>();
      arrow::Int64Builder builder;
      PX_RETURN_IF_ERROR(builder.Reserve(num_rows));
      for (size_t i = 0; i < num_rows; ++i) {
        builder.UnsafeAppend(static_cast<int64_t>(typed_col->At(i)));
      }
      std::shared_ptr<arrow::Array> array;
      PX_RETURN_IF_ERROR(builder.Finish(&array));
      PX_RETURN_IF_ERROR(row_batch->AddColumn(array));
    } else if (type_name == "Int64") {
      auto typed_col = ch_column->As<clickhouse::ColumnInt64>();
      arrow::Int64Builder builder;
      PX_RETURN_IF_ERROR(builder.Reserve(num_rows));
      for (size_t i = 0; i < num_rows; ++i) {
        builder.UnsafeAppend(typed_col->At(i));
      }
      std::shared_ptr<arrow::Array> array;
      PX_RETURN_IF_ERROR(builder.Finish(&array));
      PX_RETURN_IF_ERROR(row_batch->AddColumn(array));
    } else if (type_name == "String") {
      auto typed_col = ch_column->As<clickhouse::ColumnString>();
      arrow::StringBuilder builder;
      PX_RETURN_IF_ERROR(builder.Reserve(num_rows));

      for (size_t i = 0; i < num_rows; ++i) {
        // Convert string_view to string
        std::string value(typed_col->At(i));
        PX_RETURN_IF_ERROR(builder.Append(value));
      }

      std::shared_ptr<arrow::Array> array;
      PX_RETURN_IF_ERROR(builder.Finish(&array));
      PX_RETURN_IF_ERROR(row_batch->AddColumn(array));

    } else if (type_name == "Float32") {
      auto typed_col = ch_column->As<clickhouse::ColumnFloat32>();
      arrow::DoubleBuilder builder;
      PX_RETURN_IF_ERROR(builder.Reserve(num_rows));
      for (size_t i = 0; i < num_rows; ++i) {
        builder.UnsafeAppend(static_cast<double>(typed_col->At(i)));
      }
      std::shared_ptr<arrow::Array> array;
      PX_RETURN_IF_ERROR(builder.Finish(&array));
      PX_RETURN_IF_ERROR(row_batch->AddColumn(array));
    } else if (type_name == "Float64") {
      auto typed_col = ch_column->As<clickhouse::ColumnFloat64>();
      arrow::DoubleBuilder builder;
      PX_RETURN_IF_ERROR(builder.Reserve(num_rows));
      for (size_t i = 0; i < num_rows; ++i) {
        builder.UnsafeAppend(typed_col->At(i));
      }
      std::shared_ptr<arrow::Array> array;
      PX_RETURN_IF_ERROR(builder.Finish(&array));
      PX_RETURN_IF_ERROR(row_batch->AddColumn(array));
    } else if (type_name == "Bool") {
      auto typed_col = ch_column->As<clickhouse::ColumnUInt8>();
      arrow::BooleanBuilder builder;
      PX_RETURN_IF_ERROR(builder.Reserve(num_rows));
      for (size_t i = 0; i < num_rows; ++i) {
        builder.UnsafeAppend(typed_col->At(i) != 0);
      }
      std::shared_ptr<arrow::Array> array;
      PX_RETURN_IF_ERROR(builder.Finish(&array));
      PX_RETURN_IF_ERROR(row_batch->AddColumn(array));
    } else if (type_name == "DateTime") {
      auto typed_col = ch_column->As<clickhouse::ColumnDateTime>();
      arrow::Time64Builder builder(arrow::time64(arrow::TimeUnit::NANO),
                                   arrow::default_memory_pool());
      PX_RETURN_IF_ERROR(builder.Reserve(num_rows));

      for (size_t i = 0; i < num_rows; ++i) {
        // Convert DateTime (seconds since epoch) to nanoseconds
        int64_t ns = static_cast<int64_t>(typed_col->At(i)) * 1000000000LL;
        builder.UnsafeAppend(ns);
      }

      std::shared_ptr<arrow::Array> array;
      PX_RETURN_IF_ERROR(builder.Finish(&array));
      PX_RETURN_IF_ERROR(row_batch->AddColumn(array));

    } else if (type_name.find("DateTime64") == 0) {
      auto typed_col = ch_column->As<clickhouse::ColumnDateTime64>();
      arrow::Time64Builder builder(arrow::time64(arrow::TimeUnit::NANO),
                                   arrow::default_memory_pool());
      PX_RETURN_IF_ERROR(builder.Reserve(num_rows));

      for (size_t i = 0; i < num_rows; ++i) {
        // DateTime64 stores time with sub-second precision
        // The value is already in the correct precision (e.g., nanoseconds for DateTime64(9))
        // We need to convert to nanoseconds if it's not already
        int64_t value = typed_col->At(i);

        // Extract precision from type name (e.g., "DateTime64(9)" -> 9)
        size_t precision = 3;  // default to milliseconds
        size_t start = type_name.find('(');
        if (start != std::string::npos) {
          size_t end = type_name.find(')', start);
          if (end != std::string::npos) {
            precision = std::stoi(type_name.substr(start + 1, end - start - 1));
          }
        }

        // Convert to nanoseconds based on precision
        int64_t ns = value;
        if (precision < 9) {
          // Scale up to nanoseconds
          int64_t multiplier = 1;
          for (size_t p = precision; p < 9; p++) {
            multiplier *= 10;
          }
          ns = value * multiplier;
        } else if (precision > 9) {
          // Scale down to nanoseconds
          int64_t divisor = 1;
          for (size_t p = 9; p < precision; p++) {
            divisor *= 10;
          }
          ns = value / divisor;
        }

        builder.UnsafeAppend(ns);
      }

      std::shared_ptr<arrow::Array> array;
      PX_RETURN_IF_ERROR(builder.Finish(&array));
      PX_RETURN_IF_ERROR(row_batch->AddColumn(array));

    } else {
      return error::InvalidArgument("Unsupported ClickHouse type for conversion: $0", type_name);
    }
  }

  // Set end-of-window and end-of-stream flags
  // Don't set them here - they should be set in GenerateNextImpl
  row_batch->set_eow(false);
  row_batch->set_eos(false);

  return row_batch;
}

std::string ClickHouseSourceNode::BuildQuery() {
  std::string query = base_query_;
  std::vector<std::string> conditions;

  // Add time filtering if start/end times are specified and timestamp column is set
  if (!timestamp_column_.empty()) {
    if (start_time_.has_value()) {
      conditions.push_back(absl::Substitute("$0 >= $1", timestamp_column_, start_time_.value()));
    }
    if (end_time_.has_value()) {
      conditions.push_back(absl::Substitute("$0 <= $1", timestamp_column_, end_time_.value()));
    }
  }

  // Add partition column filtering if specified
  if (!partition_column_.empty()) {
    // Get the current hostname for partition filtering
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    conditions.push_back(absl::Substitute("$0 = '$1'", partition_column_, hostname));
  }

  // Parse the base query to find WHERE and ORDER BY positions
  std::string lower_query = query;
  std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);

  size_t where_pos = lower_query.find(" where ");
  size_t order_by_pos = lower_query.find(" order by ");
  size_t limit_pos = lower_query.find(" limit ");

  // Determine insertion point for conditions
  if (!conditions.empty()) {
    std::string conditions_clause = absl::StrJoin(conditions, " AND ");

    if (where_pos != std::string::npos) {
      // Query already has WHERE clause
      size_t insert_pos = std::string::npos;

      // Find where to insert the additional conditions
      if (order_by_pos != std::string::npos && order_by_pos > where_pos) {
        insert_pos = order_by_pos;
      } else if (limit_pos != std::string::npos && limit_pos > where_pos) {
        insert_pos = limit_pos;
      } else {
        insert_pos = query.length();
      }

      query.insert(insert_pos, " AND " + conditions_clause);
    } else {
      // No WHERE clause, need to add one
      size_t insert_pos = std::string::npos;

      if (order_by_pos != std::string::npos) {
        insert_pos = order_by_pos;
      } else if (limit_pos != std::string::npos) {
        insert_pos = limit_pos;
      } else {
        insert_pos = query.length();
      }

      query.insert(insert_pos, " WHERE " + conditions_clause);
    }
  }

  // Update lower_query after modifications
  lower_query = query;
  std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);

  // Add ORDER BY clause if needed
  if (lower_query.find(" order by ") == std::string::npos) {
    if (!timestamp_column_.empty()) {
      query += absl::Substitute(" ORDER BY $0", timestamp_column_);
    } else {
      // Fall back to ordering by first column for consistent pagination
      query += " ORDER BY 1";
    }
  }

  // Add LIMIT and OFFSET for pagination
  query += absl::Substitute(" LIMIT $0 OFFSET $1", batch_size_, current_offset_);

  return query;
}

Status ClickHouseSourceNode::ExecuteBatchQuery() {
  // Clear previous batch results
  current_batch_blocks_.clear();
  current_block_index_ = 0;

  if (!has_more_data_) {
    return Status::OK();
  }

  std::string query = BuildQuery();
  VLOG(1) << "Executing ClickHouse query: " << query;

  try {
    size_t rows_received = 0;
    client_->Select(query, [this, &rows_received](const clickhouse::Block& block) {
      // Only store non-empty blocks
      if (block.GetRowCount() > 0) {
        VLOG(1) << "Received block with " << block.GetRowCount() << " rows";
        current_batch_blocks_.push_back(block);
        rows_received += block.GetRowCount();
      }
    });

    VLOG(1) << "Total rows received: " << rows_received << ", batch size: " << batch_size_;

    // Update cursor state
    current_offset_ += rows_received;
    if (rows_received < batch_size_) {
      // We got fewer rows than requested, so no more data available
      has_more_data_ = false;
    }
  } catch (const std::exception& e) {
    return error::Internal("Failed to execute ClickHouse batch query: $0", e.what());
  }

  return Status::OK();
}

Status ClickHouseSourceNode::GenerateNextImpl(ExecState* exec_state) {
  // If we need to fetch more data
  if (current_block_index_ >= current_batch_blocks_.size()) {
    current_block_index_ = 0;
    current_batch_blocks_.clear();

    if (!has_more_data_) {
      // No more data available - send empty batch with eos=true
      PX_ASSIGN_OR_RETURN(auto empty_batch,
                          RowBatch::WithZeroRows(*output_descriptor_, true, true));
      PX_RETURN_IF_ERROR(SendRowBatchToChildren(exec_state, *empty_batch));
      return Status::OK();
    }

    // Fetch next batch from ClickHouse
    PX_RETURN_IF_ERROR(ExecuteBatchQuery());

    // If still no blocks after fetching, we're done
    if (current_batch_blocks_.empty()) {
      PX_ASSIGN_OR_RETURN(auto empty_batch,
                          RowBatch::WithZeroRows(*output_descriptor_, true, true));
      PX_RETURN_IF_ERROR(SendRowBatchToChildren(exec_state, *empty_batch));
      return Status::OK();
    }
  }

  // Calculate total rows in all blocks
  size_t total_rows = 0;
  for (const auto& block : current_batch_blocks_) {
    total_rows += block.GetRowCount();
  }

  // Create a merged RowBatch
  auto merged_batch = std::make_unique<RowBatch>(*output_descriptor_, total_rows);

  // Process each column
  for (size_t col_idx = 0; col_idx < output_descriptor_->size(); ++col_idx) {
    // Get the data type from output descriptor
    auto data_type = output_descriptor_->type(col_idx);

    // Create appropriate builder based on data type
    std::shared_ptr<arrow::ArrayBuilder> builder;
    switch (data_type) {
      case types::DataType::INT64:
        builder = std::make_shared<arrow::Int64Builder>();
        break;
      case types::DataType::UINT128:
        builder = types::MakeArrowBuilder(types::DataType::UINT128, arrow::default_memory_pool());
        break;
      case types::DataType::FLOAT64:
        builder = std::make_shared<arrow::DoubleBuilder>();
        break;
      case types::DataType::STRING:
        builder = std::make_shared<arrow::StringBuilder>();
        break;
      case types::DataType::BOOLEAN:
        builder = std::make_shared<arrow::BooleanBuilder>();
        break;
      case types::DataType::TIME64NS:
        builder = std::make_shared<arrow::Time64Builder>(arrow::time64(arrow::TimeUnit::NANO),
                                                         arrow::default_memory_pool());
        break;
      default:
        return error::InvalidArgument("Unsupported data type for column $0", col_idx);
    }

    // Reserve space for all rows
    PX_RETURN_IF_ERROR(builder->Reserve(total_rows));

    // Append data from all blocks
    for (const auto& block : current_batch_blocks_) {
      PX_ASSIGN_OR_RETURN(auto row_batch, ConvertClickHouseBlockToRowBatch(block, false));
      auto array = row_batch->ColumnAt(col_idx);

      // Append values from this block's array
      switch (data_type) {
        case types::DataType::INT64: {
          auto typed_array = std::static_pointer_cast<arrow::Int64Array>(array);
          auto typed_builder = std::static_pointer_cast<arrow::Int64Builder>(builder);
          for (int i = 0; i < typed_array->length(); i++) {
            if (typed_array->IsNull(i)) {
              PX_RETURN_IF_ERROR(typed_builder->AppendNull());
            } else {
              typed_builder->UnsafeAppend(typed_array->Value(i));
            }
          }
          break;
        }
        case types::DataType::UINT128: {
          auto typed_array = std::static_pointer_cast<arrow::FixedSizeBinaryArray>(array);
          for (int i = 0; i < typed_array->length(); i++) {
            if (typed_array->IsNull(i)) {
              PX_RETURN_IF_ERROR(builder->AppendNull());
            } else {
              auto val = types::GetValueFromArrowArray<types::DataType::UINT128>(array.get(), i);
              PX_RETURN_IF_ERROR(table_store::schema::CopyValue<types::DataType::UINT128>(builder.get(), val));
            }
          }
          break;
        }
        case types::DataType::TIME64NS: {
          auto typed_array = std::static_pointer_cast<arrow::Time64Array>(array);
          auto typed_builder = std::static_pointer_cast<arrow::Time64Builder>(builder);
          for (int i = 0; i < typed_array->length(); i++) {
            if (typed_array->IsNull(i)) {
              PX_RETURN_IF_ERROR(typed_builder->AppendNull());
            } else {
              typed_builder->UnsafeAppend(typed_array->Value(i));
            }
          }
          break;
        }
        case types::DataType::FLOAT64: {
          auto typed_array = std::static_pointer_cast<arrow::DoubleArray>(array);
          auto typed_builder = std::static_pointer_cast<arrow::DoubleBuilder>(builder);
          for (int i = 0; i < typed_array->length(); i++) {
            if (typed_array->IsNull(i)) {
              PX_RETURN_IF_ERROR(typed_builder->AppendNull());
            } else {
              typed_builder->UnsafeAppend(typed_array->Value(i));
            }
          }
          break;
        }
        case types::DataType::STRING: {
          auto typed_array = std::static_pointer_cast<arrow::StringArray>(array);
          auto typed_builder = std::static_pointer_cast<arrow::StringBuilder>(builder);
          for (int i = 0; i < typed_array->length(); i++) {
            if (typed_array->IsNull(i)) {
              PX_RETURN_IF_ERROR(typed_builder->AppendNull());
            } else {
              PX_RETURN_IF_ERROR(typed_builder->Append(typed_array->GetString(i)));
            }
          }
          break;
        }
        case types::DataType::BOOLEAN: {
          auto typed_array = std::static_pointer_cast<arrow::BooleanArray>(array);
          auto typed_builder = std::static_pointer_cast<arrow::BooleanBuilder>(builder);
          for (int i = 0; i < typed_array->length(); i++) {
            if (typed_array->IsNull(i)) {
              PX_RETURN_IF_ERROR(typed_builder->AppendNull());
            } else {
              typed_builder->UnsafeAppend(typed_array->Value(i));
            }
          }
          break;
        }
        default:
          return error::InvalidArgument("Unsupported data type for column $0", col_idx);
      }
    }

    // Finish building and add column
    std::shared_ptr<arrow::Array> merged_array;
    PX_RETURN_IF_ERROR(builder->Finish(&merged_array));
    PX_RETURN_IF_ERROR(merged_batch->AddColumn(merged_array));
  }

  // Set proper end-of-window and end-of-stream flags
  bool is_last_batch = !has_more_data_;
  if (is_last_batch) {
    merged_batch->set_eow(true);
    merged_batch->set_eos(true);
  } else {
    merged_batch->set_eow(false);
    merged_batch->set_eos(false);
  }

  // Update stats
  rows_processed_ += merged_batch->num_rows();
  bytes_processed_ += merged_batch->NumBytes();

  // Send to children
  PX_RETURN_IF_ERROR(SendRowBatchToChildren(exec_state, *merged_batch));

  // Mark all blocks as processed
  current_block_index_ = current_batch_blocks_.size();

  return Status::OK();
}

bool ClickHouseSourceNode::NextBatchReady() {
  // We're ready if we have blocks in current batch or if we can fetch more data
  return (current_block_index_ < current_batch_blocks_.size()) || has_more_data_;
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
