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

#pragma once

#include <clickhouse/client.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/plan/operators.h"
#include "src/common/base/base.h"
#include "src/common/base/status.h"
#include "src/shared/types/types.h"
#include "src/table_store/schema/row_batch.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;

class ClickHouseSourceNode : public SourceNode {
 public:
  ClickHouseSourceNode() = default;
  virtual ~ClickHouseSourceNode() = default;

  bool NextBatchReady() override;

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(const plan::Operator& plan_node) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status GenerateNextImpl(ExecState* exec_state) override;

 private:
  // Convert ClickHouse column types to Pixie data types
  StatusOr<types::DataType> ClickHouseTypeToPixieType(const clickhouse::TypeRef& ch_type);

  // Convert ClickHouse block to Pixie RowBatch
  StatusOr<std::unique_ptr<RowBatch>> ConvertClickHouseBlockToRowBatch(
      const clickhouse::Block& block, bool is_last_block);

  // Execute a batch query
  Status ExecuteBatchQuery();

  // Build the query with time filtering and pagination
  std::string BuildQuery();

  // Connection information
  std::string host_;
  int port_;
  std::string username_;
  std::string password_;
  std::string database_;
  std::string base_query_;

  // Batch size and cursor tracking
  size_t batch_size_ = 1024;
  size_t current_offset_ = 0;
  bool has_more_data_ = true;

  // Time filtering
  std::optional<int64_t> start_time_;
  std::optional<int64_t> end_time_;
  std::string timestamp_column_;  // Column to use for timestamp-based filtering and ordering
  std::string partition_column_;  // Column used for partitioning

  // ClickHouse client
  std::unique_ptr<clickhouse::Client> client_;

  // Current batch results
  std::vector<clickhouse::Block> current_batch_blocks_;
  size_t current_block_index_ = 0;

  // Streaming support
  bool streaming_ = false;

  // Plan node
  std::unique_ptr<plan::ClickHouseSourceOperator> plan_node_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace px
