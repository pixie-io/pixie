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

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/ir/clickhouse_source_ir.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace planner {

using ClickHouseSourceTest = ASTVisitorTest;

namespace {
// Seeds the compiler state's relation map with a small http_events-like table so
// ResolveType resolves against it instead of connecting to a live ClickHouse
// server (schema inference via DESCRIBE TABLE is exercised end-to-end by the
// clickhouse_source_node exec test).
table_store::schema::Relation HttpEventsRelation() {
  return table_store::schema::Relation{
      {types::STRING, types::INT64}, {"req_path", "resp_status"}, {types::ST_NONE, types::ST_NONE}};
}
}  // namespace

// Explicitly-selected columns, including the ClickHouse-provided hostname and
// event_time columns, serialize to a ClickHouseSourceOperator with matching
// column names/types and an explicit SELECT query.
TEST_F(ClickHouseSourceTest, explicit_columns_to_proto) {
  (*compiler_state_->relation_map())["http_events"] = HttpEventsRelation();

  std::vector<std::string> columns = {"req_path", "hostname", "event_time"};
  ASSERT_OK_AND_ASSIGN(auto* source, graph->CreateNode<ClickHouseSourceIR>(
                                         ast, "http_events", columns, "localhost", 9000, "default",
                                         "test_password", "default", "event_time"));

  EXPECT_OK(source->ResolveType(compiler_state_.get()));

  planpb::Operator pb;
  ASSERT_OK(source->ToProto(&pb));

  EXPECT_EQ(pb.op_type(), planpb::CLICKHOUSE_SOURCE_OPERATOR);
  const auto& ch = pb.clickhouse_source_op();
  EXPECT_EQ(ch.host(), "localhost");
  EXPECT_EQ(ch.port(), 9000);
  EXPECT_EQ(ch.username(), "default");
  EXPECT_EQ(ch.password(), "test_password");
  EXPECT_EQ(ch.database(), "default");
  EXPECT_EQ(ch.timestamp_column(), "event_time");
  EXPECT_EQ(ch.partition_column(), "hostname");

  ASSERT_EQ(ch.column_names_size(), 3);
  EXPECT_EQ(ch.column_names(0), "req_path");
  EXPECT_EQ(ch.column_names(1), "hostname");
  EXPECT_EQ(ch.column_names(2), "event_time");

  ASSERT_EQ(ch.column_types_size(), 3);
  EXPECT_EQ(ch.column_types(0), types::STRING);
  EXPECT_EQ(ch.column_types(1), types::STRING);
  EXPECT_EQ(ch.column_types(2), types::TIME64NS);

  EXPECT_EQ(ch.query(), "SELECT req_path, hostname, event_time FROM http_events");
}

// An empty column selection resolves to all table columns plus the
// ClickHouse-provided hostname and event_time columns.
TEST_F(ClickHouseSourceTest, select_all_to_proto) {
  (*compiler_state_->relation_map())["http_events"] = HttpEventsRelation();

  std::vector<std::string> columns = {};
  ASSERT_OK_AND_ASSIGN(auto* source, graph->CreateNode<ClickHouseSourceIR>(
                                         ast, "http_events", columns, "localhost", 9000, "default",
                                         "test_password", "default", "event_time"));

  EXPECT_OK(source->ResolveType(compiler_state_.get()));

  planpb::Operator pb;
  ASSERT_OK(source->ToProto(&pb));

  const auto& ch = pb.clickhouse_source_op();
  ASSERT_EQ(ch.column_names_size(), 4);
  EXPECT_EQ(ch.column_names(0), "req_path");
  EXPECT_EQ(ch.column_names(1), "resp_status");
  EXPECT_EQ(ch.column_names(2), "hostname");
  EXPECT_EQ(ch.column_names(3), "event_time");
  EXPECT_EQ(ch.query(), "SELECT req_path, resp_status, hostname, event_time FROM http_events");
}

// Start/stop times set on the IR are propagated into the operator proto.
TEST_F(ClickHouseSourceTest, time_range_to_proto) {
  (*compiler_state_->relation_map())["http_events"] = HttpEventsRelation();

  std::vector<std::string> columns = {"req_path", "hostname", "event_time"};
  ASSERT_OK_AND_ASSIGN(auto* source, graph->CreateNode<ClickHouseSourceIR>(
                                         ast, "http_events", columns, "localhost", 9000, "default",
                                         "test_password", "default", "event_time"));
  source->SetTimeStartNS(1000);
  source->SetTimeStopNS(2000);

  EXPECT_OK(source->ResolveType(compiler_state_.get()));

  planpb::Operator pb;
  ASSERT_OK(source->ToProto(&pb));

  EXPECT_EQ(pb.clickhouse_source_op().start_time(), 1000);
  EXPECT_EQ(pb.clickhouse_source_op().end_time(), 2000);
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
