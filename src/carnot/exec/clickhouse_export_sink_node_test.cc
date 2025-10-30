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
#include "src/carnot/plan/operators.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/udf/registry.h"
#include "src/common/event/time_system.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;
using ::testing::_;

class ClickHouseExportSinkNodeTest : public ::testing::Test {
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

    // Create a minimal agent metadata state for test execution
    auto metadata_state = std::make_shared<md::AgentMetadataState>(
        "test_host",      // hostname
        1,                // asid
        getpid(),         // pid
        0,                // start_time
        sole::uuid4(),    // agent_id
        "",               // pod_name
        sole::uuid4(),    // vizier_id
        "test_vizier",    // vizier_name
        "",               // vizier_namespace
        time_system_.get()); // time_system
    exec_state_->set_metadata_state(metadata_state);

    // Start ClickHouse container
    clickhouse_server_ =
        std::make_unique<ContainerRunner>(px::testing::BazelRunfilePath(kClickHouseImage),
                                          "clickhouse_export_test", kClickHouseReadyMessage);

    std::vector<std::string> options = {
        absl::Substitute("--publish=$0:$0", kClickHousePort),
        "--env=CLICKHOUSE_PASSWORD=test_password",
        "--network=host",
    };

    ASSERT_OK(clickhouse_server_->Run(std::chrono::seconds{60}, options, {}, true,
                                      std::chrono::seconds{300}));

    // Give ClickHouse time to initialize
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Create ClickHouse client for verification
    SetupClickHouseClient();
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

  void CreateExportTable(const std::string& table_name) {
    try {
      client_->Execute(absl::Substitute("DROP TABLE IF EXISTS $0", table_name));

      client_->Execute(absl::Substitute(R"(
        CREATE TABLE $0 (
          time_ DateTime64(9),
          hostname String,
          count Int64,
          latency Float64
        ) ENGINE = MergeTree()
        ORDER BY time_
      )", table_name));

      LOG(INFO) << "Export table created successfully: " << table_name;
    } catch (const std::exception& e) {
      LOG(ERROR) << "Failed to create export table: " << e.what();
      throw;
    }
  }

  std::vector<std::vector<std::string>> QueryTable(const std::string& query) {
    std::vector<std::vector<std::string>> results;

    try {
      client_->Select(query, [&](const clickhouse::Block& block) {
        for (size_t row_idx = 0; row_idx < block.GetRowCount(); ++row_idx) {
          std::vector<std::string> row;
          for (size_t col_idx = 0; col_idx < block.GetColumnCount(); ++col_idx) {
            auto col = block[col_idx];
            std::string value;

            if (auto int_col = col->As<clickhouse::ColumnInt64>()) {
              value = std::to_string((*int_col)[row_idx]);
            } else if (auto uint_col = col->As<clickhouse::ColumnUInt64>()) {
              value = std::to_string((*uint_col)[row_idx]);
            } else if (auto float_col = col->As<clickhouse::ColumnFloat64>()) {
              value = std::to_string((*float_col)[row_idx]);
            } else if (auto str_col = col->As<clickhouse::ColumnString>()) {
              value = (*str_col)[row_idx];
            } else if (auto dt_col = col->As<clickhouse::ColumnDateTime64>()) {
              value = std::to_string((*dt_col)[row_idx]);
            } else {
              value = "<unsupported_type>";
            }

            row.push_back(value);
          }
          results.push_back(row);
        }
      });
    } catch (const std::exception& e) {
      LOG(ERROR) << "Failed to query table: " << e.what();
      throw;
    }

    return results;
  }

  std::unique_ptr<plan::ClickHouseExportSinkOperator> CreatePlanNode(
      const std::string& table_name) {
    planpb::Operator op;
    op.set_op_type(planpb::CLICKHOUSE_EXPORT_SINK_OPERATOR);
    auto* ch_op = op.mutable_clickhouse_sink_op();

    auto* config = ch_op->mutable_clickhouse_config();
    config->set_host("localhost");
    config->set_port(kClickHousePort);
    config->set_username("default");
    config->set_password("test_password");
    config->set_database("default");

    ch_op->set_table_name(table_name);

    // Add column mappings
    auto* mapping0 = ch_op->add_column_mappings();
    mapping0->set_input_column_index(0);
    mapping0->set_clickhouse_column_name("time_");
    mapping0->set_column_type(types::TIME64NS);

    auto* mapping1 = ch_op->add_column_mappings();
    mapping1->set_input_column_index(1);
    mapping1->set_clickhouse_column_name("hostname");
    mapping1->set_column_type(types::STRING);

    auto* mapping2 = ch_op->add_column_mappings();
    mapping2->set_input_column_index(2);
    mapping2->set_clickhouse_column_name("count");
    mapping2->set_column_type(types::INT64);

    auto* mapping3 = ch_op->add_column_mappings();
    mapping3->set_input_column_index(3);
    mapping3->set_clickhouse_column_name("latency");
    mapping3->set_column_type(types::FLOAT64);

    auto plan_node = std::make_unique<plan::ClickHouseExportSinkOperator>(1);
    EXPECT_OK(plan_node->Init(op.clickhouse_sink_op()));

    return plan_node;
  }

  std::unique_ptr<ContainerRunner> clickhouse_server_;
  std::unique_ptr<clickhouse::Client> client_;
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::Registry> func_registry_;
  std::unique_ptr<event::TimeSystem> time_system_ = std::make_unique<event::RealTimeSystem>();
};

TEST_F(ClickHouseExportSinkNodeTest, BasicExport) {
  const std::string table_name = "export_test_basic";
  CreateExportTable(table_name);

  auto plan_node = CreatePlanNode(table_name);

  // Define input schema
  RowDescriptor input_rd({types::TIME64NS, types::STRING, types::INT64, types::FLOAT64});

  // Create node tester
  auto tester = exec::ExecNodeTester<ClickHouseExportSinkNode, plan::ClickHouseExportSinkOperator>(
      *plan_node, RowDescriptor({}), {input_rd}, exec_state_.get());

  // Create test data
  auto rb1 = RowBatchBuilder(input_rd, 2, /*eow*/ false, /*eos*/ false)
      .AddColumn<types::Time64NSValue>({1000000000000000000LL, 2000000000000000000LL})
      .AddColumn<types::StringValue>({"host1", "host2"})
      .AddColumn<types::Int64Value>({100, 200})
      .AddColumn<types::Float64Value>({1.5, 2.5})
      .get();

  auto rb2 = RowBatchBuilder(input_rd, 1, /*eow*/ true, /*eos*/ true)
      .AddColumn<types::Time64NSValue>({3000000000000000000LL})
      .AddColumn<types::StringValue>({"host3"})
      .AddColumn<types::Int64Value>({300})
      .AddColumn<types::Float64Value>({3.5})
      .get();

  // Send data to sink
  tester.ConsumeNext(rb1, 0, 0);
  tester.ConsumeNext(rb2, 0, 0);
  tester.Close();

  // Verify data was inserted
  auto results = QueryTable(absl::Substitute("SELECT hostname, count, latency FROM $0 ORDER BY time_", table_name));

  ASSERT_EQ(results.size(), 3);
  EXPECT_EQ(results[0][0], "host1");
  EXPECT_EQ(results[0][1], "100");
  EXPECT_THAT(results[0][2], ::testing::StartsWith("1.5"));

  EXPECT_EQ(results[1][0], "host2");
  EXPECT_EQ(results[1][1], "200");
  EXPECT_THAT(results[1][2], ::testing::StartsWith("2.5"));

  EXPECT_EQ(results[2][0], "host3");
  EXPECT_EQ(results[2][1], "300");
  EXPECT_THAT(results[2][2], ::testing::StartsWith("3.5"));
}

TEST_F(ClickHouseExportSinkNodeTest, EmptyBatch) {
  const std::string table_name = "export_test_empty";
  CreateExportTable(table_name);

  auto plan_node = CreatePlanNode(table_name);

  RowDescriptor input_rd({types::TIME64NS, types::STRING, types::INT64, types::FLOAT64});

  auto tester = exec::ExecNodeTester<ClickHouseExportSinkNode, plan::ClickHouseExportSinkOperator>(
      *plan_node, RowDescriptor({}), {input_rd}, exec_state_.get());

  // Send only EOS batch
  auto rb = RowBatchBuilder(input_rd, 0, /*eow*/ true, /*eos*/ true)
      .AddColumn<types::Time64NSValue>({})
      .AddColumn<types::StringValue>({})
      .AddColumn<types::Int64Value>({})
      .AddColumn<types::Float64Value>({})
      .get();

  tester.ConsumeNext(rb, 0, 0);
  tester.Close();

  // Verify no data was inserted
  auto results = QueryTable(absl::Substitute("SELECT COUNT(*) FROM $0", table_name));

  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0][0], "0");
}

TEST_F(ClickHouseExportSinkNodeTest, MultipleBatches) {
  const std::string table_name = "export_test_multiple";
  CreateExportTable(table_name);

  auto plan_node = CreatePlanNode(table_name);

  RowDescriptor input_rd({types::TIME64NS, types::STRING, types::INT64, types::FLOAT64});

  auto tester = exec::ExecNodeTester<ClickHouseExportSinkNode, plan::ClickHouseExportSinkOperator>(
      *plan_node, RowDescriptor({}), {input_rd}, exec_state_.get());

  // Send multiple batches
  for (int i = 0; i < 5; ++i) {
    bool is_last = (i == 4);
    auto rb = RowBatchBuilder(input_rd, 1, /*eow*/ is_last, /*eos*/ is_last)
        .AddColumn<types::Time64NSValue>({(i + 1) * 1000000000000000000LL})
        .AddColumn<types::StringValue>({absl::Substitute("host$0", i)})
        .AddColumn<types::Int64Value>({i * 100})
        .AddColumn<types::Float64Value>({i * 1.5})
        .get();

    tester.ConsumeNext(rb, 0, 0);
  }

  tester.Close();

  // Verify all batches were inserted
  auto results = QueryTable(absl::Substitute("SELECT COUNT(*) FROM $0", table_name));

  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0][0], "5");

  // Verify data order
  auto ordered_results = QueryTable(absl::Substitute("SELECT hostname FROM $0 ORDER BY time_", table_name));

  ASSERT_EQ(ordered_results.size(), 5);
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(ordered_results[i][0], absl::Substitute("host$0", i));
  }
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
