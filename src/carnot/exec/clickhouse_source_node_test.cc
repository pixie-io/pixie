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

#include <clickhouse/client.h>

#include <arrow/memory_pool.h>
#include <chrono>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <absl/strings/substitute.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sole.hpp>

#include "src/carnot/exec/test_utils.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf/registry.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/shared/types/typespb/types.pb.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::Table;
using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;
using ::testing::_;

class ClickHouseSourceNodeTest : public ::testing::Test {
 protected:
  static constexpr char kClickHouseImage[] =
      "src/stirling/source_connectors/socket_tracer/testing/container_images/clickhouse.tar";
  static constexpr char kClickHouseReadyMessage[] = "Ready for connections";
  static constexpr int kClickHousePort = 9000;

  void SetUp() override {
    // Set up function registry and exec state
    func_registry_ = std::make_unique<udf::Registry>("test_registry");
    auto table_store = std::make_shared<table_store::TableStore>();
    exec_state_ = std::make_unique<ExecState>(
        func_registry_.get(), table_store, MockResultSinkStubGenerator, MockMetricsStubGenerator,
        MockTraceStubGenerator, MockLogStubGenerator, sole::uuid4(), nullptr);

    // Start ClickHouse container
    clickhouse_server_ =
        std::make_unique<ContainerRunner>(px::testing::BazelRunfilePath(kClickHouseImage),
                                          "clickhouse_test", kClickHouseReadyMessage);

    std::vector<std::string> options = {
        absl::Substitute("--publish=$0:$0", kClickHousePort),
        "--env=CLICKHOUSE_PASSWORD=test_password",
        "--network=host",
    };

    ASSERT_OK(clickhouse_server_->Run(std::chrono::seconds{60}, options, {}, true,
                                      std::chrono::seconds{300}));

    // Give ClickHouse time to initialize
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Create ClickHouse client for test data setup
    SetupClickHouseClient();
    CreateTestTable();
  }

  void TearDown() override {
    if (client_) {
      client_.reset();
    }
  }

  void SetupClickHouseClient() {
    clickhouse::ClientOptions client_options;
    client_options.SetHost("localhost");
    client_options.SetPort(kClickHousePort);
    client_options.SetUser("default");
    client_options.SetPassword("test_password");
    client_options.SetDefaultDatabase("default");

    const int kMaxRetries = 5;
    for (int i = 0; i < kMaxRetries; ++i) {
      LOG(INFO) << "Attempting to connect to ClickHouse (attempt " << (i + 1) << "/" << kMaxRetries
                << ")...";
      try {
        client_ = std::make_unique<clickhouse::Client>(client_options);
        client_->Execute("SELECT 1");
        break;
      } catch (const std::exception& e) {
        LOG(WARNING) << "Failed to connect: " << e.what();
        if (i < kMaxRetries - 1) {
          std::this_thread::sleep_for(std::chrono::seconds(2));
        } else {
          throw;
        }
      }
    }
  }

  void CreateTestTable() {
    try {
      client_->Execute("DROP TABLE IF EXISTS test_table");

      client_->Execute(R"(
        CREATE TABLE test_table (
          id UInt64,
          name String,
          value Float64,
          timestamp DateTime,
          partition_key String
        ) ENGINE = MergeTree()
        PARTITION BY (timestamp, partition_key)
        ORDER BY timestamp
      )");

      auto id_col = std::make_shared<clickhouse::ColumnUInt64>();
      auto name_col = std::make_shared<clickhouse::ColumnString>();
      auto value_col = std::make_shared<clickhouse::ColumnFloat64>();
      auto timestamp_col = std::make_shared<clickhouse::ColumnDateTime>();
      auto partition_key_col = std::make_shared<clickhouse::ColumnString>();

      // Add test data with increasing timestamps
      std::time_t base_time = std::time(nullptr) - 3600;  // Start 1 hour ago
      id_col->Append(1);
      name_col->Append("test1");
      value_col->Append(10.5);
      timestamp_col->Append(base_time);
      partition_key_col->Append("partition_a");

      id_col->Append(2);
      name_col->Append("test2");
      value_col->Append(20.5);
      timestamp_col->Append(base_time + 1800);  // 30 minutes later
      partition_key_col->Append("partition_a");

      id_col->Append(3);
      name_col->Append("test3");
      value_col->Append(30.5);
      timestamp_col->Append(base_time + 3600);  // 1 hour later
      partition_key_col->Append("partition_b");

      clickhouse::Block block;
      block.AppendColumn("id", id_col);
      block.AppendColumn("name", name_col);
      block.AppendColumn("value", value_col);
      block.AppendColumn("timestamp", timestamp_col);
      block.AppendColumn("partition_key", partition_key_col);

      client_->Insert("test_table", block);

      LOG(INFO) << "Test table created and populated successfully";
    } catch (const std::exception& e) {
      LOG(ERROR) << "Failed to create test table: " << e.what();
      throw;
    }
  }

  std::unique_ptr<ContainerRunner> clickhouse_server_;
  std::unique_ptr<clickhouse::Client> client_;
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::Registry> func_registry_;
};

TEST_F(ClickHouseSourceNodeTest, BasicQuery) {
  // Create ClickHouse source operator proto
  auto op_proto = planpb::testutils::CreateClickHouseSourceOperatorPB();
  std::unique_ptr<plan::Operator> plan_node =
      plan::ClickHouseSourceOperator::FromProto(op_proto, 1);

  // Define expected output schema
  RowDescriptor output_rd(
      {types::DataType::INT64, types::DataType::STRING, types::DataType::FLOAT64});

  // Create node tester
  auto tester = exec::ExecNodeTester<ClickHouseSourceNode, plan::ClickHouseSourceOperator>(
      *plan_node, output_rd, std::vector<RowDescriptor>({}), exec_state_.get());

  // Verify state machine behavior
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());

  // First batch should return 2 rows (batch_size = 2)
  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 2, /*eow*/ false, /*eos*/ false)
          .AddColumn<types::Int64Value>({1, 2})
          .AddColumn<types::StringValue>({"test1", "test2"})
          .AddColumn<types::Float64Value>({10.5, 20.5})
          .get());

  // Second batch should return remaining 1 row with eos
  EXPECT_TRUE(tester.node()->HasBatchesRemaining());
  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 1, /*eow*/ true, /*eos*/ true)
          .AddColumn<types::Int64Value>({3})
          .AddColumn<types::StringValue>({"test3"})
          .AddColumn<types::Float64Value>({30.5})
          .get());

  EXPECT_FALSE(tester.node()->HasBatchesRemaining());
  tester.Close();

  // Verify metrics
  EXPECT_EQ(3, tester.node()->RowsProcessed());
  EXPECT_GT(tester.node()->BytesProcessed(), 0);
}

TEST_F(ClickHouseSourceNodeTest, EmptyResultSet) {
  // Create a table with no data
  client_->Execute("DROP TABLE IF EXISTS empty_table");
  client_->Execute(R"(
    CREATE TABLE empty_table (
      id UInt64,
      name String,
      value Float64,
      timestamp DateTime,
      partition_key String
    ) ENGINE = MergeTree()
    PARTITION BY (timestamp, partition_key)
    ORDER BY timestamp
  )");

  // Create operator that queries empty table
  planpb::Operator op;
  op.set_op_type(planpb::OperatorType::CLICKHOUSE_SOURCE_OPERATOR);
  auto* ch_op = op.mutable_clickhouse_source_op();
  ch_op->set_host("localhost");
  ch_op->set_port(kClickHousePort);
  ch_op->set_username("default");
  ch_op->set_password("test_password");
  ch_op->set_database("default");
  ch_op->set_query("SELECT id, name, value FROM empty_table");
  ch_op->set_batch_size(1024);
  ch_op->set_streaming(false);
  ch_op->add_column_names("id");
  ch_op->add_column_names("name");
  ch_op->add_column_names("value");
  ch_op->add_column_types(types::DataType::INT64);
  ch_op->add_column_types(types::DataType::STRING);
  ch_op->add_column_types(types::DataType::FLOAT64);
  ch_op->set_timestamp_column("timestamp");
  ch_op->set_start_time(1000000000000000000LL);  // Year 2001 in nanoseconds
  ch_op->set_end_time(9223372036854775807LL);    // Max int64

  std::unique_ptr<plan::Operator> plan_node = plan::ClickHouseSourceOperator::FromProto(op, 1);
  RowDescriptor output_rd(
      {types::DataType::INT64, types::DataType::STRING, types::DataType::FLOAT64});

  auto tester = exec::ExecNodeTester<ClickHouseSourceNode, plan::ClickHouseSourceOperator>(
      *plan_node, output_rd, std::vector<RowDescriptor>({}), exec_state_.get());

  EXPECT_TRUE(tester.node()->HasBatchesRemaining());

  // Should return empty batch with eos=true
  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 0, /*eow*/ true, /*eos*/ true)
          .AddColumn<types::Int64Value>({})
          .AddColumn<types::StringValue>({})
          .AddColumn<types::Float64Value>({})
          .get());

  EXPECT_FALSE(tester.node()->HasBatchesRemaining());
  tester.Close();

  EXPECT_EQ(0, tester.node()->RowsProcessed());
  EXPECT_EQ(0, tester.node()->BytesProcessed());
}

TEST_F(ClickHouseSourceNodeTest, FilteredQuery) {
  // Create operator with WHERE clause
  planpb::Operator op;
  op.set_op_type(planpb::OperatorType::CLICKHOUSE_SOURCE_OPERATOR);
  auto* ch_op = op.mutable_clickhouse_source_op();
  ch_op->set_host("localhost");
  ch_op->set_port(kClickHousePort);
  ch_op->set_username("default");
  ch_op->set_password("test_password");
  ch_op->set_database("default");
  ch_op->set_query("SELECT id, name, value FROM test_table WHERE value > 15.0 ORDER BY id");
  ch_op->set_batch_size(1024);
  ch_op->set_streaming(false);
  ch_op->add_column_names("id");
  ch_op->add_column_names("name");
  ch_op->add_column_names("value");
  ch_op->add_column_types(types::DataType::INT64);
  ch_op->add_column_types(types::DataType::STRING);
  ch_op->add_column_types(types::DataType::FLOAT64);
  ch_op->set_timestamp_column("timestamp");
  ch_op->set_start_time(1000000000000000000LL);  // Year 2001 in nanoseconds
  ch_op->set_end_time(9223372036854775807LL);    // Max int64

  std::unique_ptr<plan::Operator> plan_node = plan::ClickHouseSourceOperator::FromProto(op, 1);
  RowDescriptor output_rd(
      {types::DataType::INT64, types::DataType::STRING, types::DataType::FLOAT64});

  auto tester = exec::ExecNodeTester<ClickHouseSourceNode, plan::ClickHouseSourceOperator>(
      *plan_node, output_rd, std::vector<RowDescriptor>({}), exec_state_.get());

  EXPECT_TRUE(tester.node()->HasBatchesRemaining());

  // Should return all filtered results in one batch (2 rows < batch_size)
  tester.GenerateNextResult().ExpectRowBatch(
      RowBatchBuilder(output_rd, 2, /*eow*/ true, /*eos*/ true)
          .AddColumn<types::Int64Value>({2, 3})
          .AddColumn<types::StringValue>({"test2", "test3"})
          .AddColumn<types::Float64Value>({20.5, 30.5})
          .get());

  EXPECT_FALSE(tester.node()->HasBatchesRemaining());
  tester.Close();

  EXPECT_EQ(2, tester.node()->RowsProcessed());
  EXPECT_GT(tester.node()->BytesProcessed(), 0);
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
