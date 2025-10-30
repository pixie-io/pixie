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

#include "src/carnot/exec/clickhouse_export_sink_node.h"

#include <memory>
#include <string>
#include <vector>

#include <absl/strings/substitute.h>
#include "glog/logging.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/macros.h"
#include "src/shared/types/typespb/types.pb.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;

std::string ClickHouseExportSinkNode::DebugStringImpl() {
  return absl::Substitute("Exec::ClickHouseExportSinkNode: $0", plan_node_->DebugString());
}

Status ClickHouseExportSinkNode::InitImpl(const plan::Operator& plan_node) {
  CHECK(plan_node.op_type() == planpb::OperatorType::CLICKHOUSE_EXPORT_SINK_OPERATOR);
  if (input_descriptors_.size() != 1) {
    return error::InvalidArgument(
        "ClickHouse Export operator expects a single input relation, got $0",
        input_descriptors_.size());
  }

  input_descriptor_ = std::make_unique<RowDescriptor>(input_descriptors_[0]);
  const auto* sink_plan_node = static_cast<const plan::ClickHouseExportSinkOperator*>(&plan_node);
  plan_node_ = std::make_unique<plan::ClickHouseExportSinkOperator>(*sink_plan_node);
  return Status::OK();
}

Status ClickHouseExportSinkNode::PrepareImpl(ExecState*) { return Status::OK(); }

Status ClickHouseExportSinkNode::OpenImpl(ExecState* /*exec_state*/) {
  // Connect to ClickHouse using config from plan node
  const auto& config = plan_node_->clickhouse_config();

  clickhouse::ClientOptions options;
  options.SetHost(config.host());
  options.SetPort(config.port());
  options.SetUser(config.username());
  options.SetPassword(config.password());
  options.SetDefaultDatabase(config.database());

  clickhouse_client_ = std::make_unique<clickhouse::Client>(options);

  return Status::OK();
}

Status ClickHouseExportSinkNode::CloseImpl(ExecState* exec_state) {
  if (sent_eos_) {
    return Status::OK();
  }

  LOG(INFO) << absl::Substitute(
      "Closing ClickHouseExportSinkNode $0 in query $1 before receiving EOS", plan_node_->id(),
      exec_state->query_id().str());

  return Status::OK();
}

Status ClickHouseExportSinkNode::ConsumeNextImpl(ExecState* /*exec_state*/, const RowBatch& rb,
                                                   size_t /*parent_index*/) {
  // Skip insertion if the batch is empty
  if (rb.num_rows() == 0) {
    if (rb.eos()) {
      sent_eos_ = true;
    }
    return Status::OK();
  }

  // Build an INSERT query with the data from the row batch
  clickhouse::Block block;

  // Create columns based on the column mappings
  for (const auto& mapping : plan_node_->column_mappings()) {
    auto arrow_col = rb.ColumnAt(mapping.input_column_index());
    int64_t num_rows = arrow_col->length();

    // Create ClickHouse column based on data type
    switch (mapping.column_type()) {
      case types::INT64: {
        auto col = std::make_shared<clickhouse::ColumnInt64>();
        for (int64_t i = 0; i < num_rows; ++i) {
          col->Append(types::GetValueFromArrowArray<types::INT64>(arrow_col.get(), i));
        }
        block.AppendColumn(mapping.clickhouse_column_name(), col);
        break;
      }
      case types::FLOAT64: {
        auto col = std::make_shared<clickhouse::ColumnFloat64>();
        for (int64_t i = 0; i < num_rows; ++i) {
          col->Append(types::GetValueFromArrowArray<types::FLOAT64>(arrow_col.get(), i));
        }
        block.AppendColumn(mapping.clickhouse_column_name(), col);
        break;
      }
      case types::STRING: {
        auto col = std::make_shared<clickhouse::ColumnString>();
        for (int64_t i = 0; i < num_rows; ++i) {
          col->Append(types::GetValueFromArrowArray<types::STRING>(arrow_col.get(), i));
        }
        block.AppendColumn(mapping.clickhouse_column_name(), col);
        break;
      }
      case types::TIME64NS: {
        auto col = std::make_shared<clickhouse::ColumnDateTime64>(9);
        for (int64_t i = 0; i < num_rows; ++i) {
          int64_t ns_val = types::GetValueFromArrowArray<types::TIME64NS>(arrow_col.get(), i);
          col->Append(ns_val);
        }
        block.AppendColumn(mapping.clickhouse_column_name(), col);
        break;
      }
      case types::BOOLEAN: {
        auto col = std::make_shared<clickhouse::ColumnUInt8>();
        for (int64_t i = 0; i < num_rows; ++i) {
          col->Append(types::GetValueFromArrowArray<types::BOOLEAN>(arrow_col.get(), i) ? 1 : 0);
        }
        block.AppendColumn(mapping.clickhouse_column_name(), col);
        break;
      }
      default:
        return error::InvalidArgument("Unsupported data type for ClickHouse export: $0",
                                      types::ToString(mapping.column_type()));
    }
  }

  // Insert the block into ClickHouse
  clickhouse_client_->Insert(plan_node_->table_name(), block);

  if (rb.eos()) {
    sent_eos_ = true;
  }

  return Status::OK();
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
