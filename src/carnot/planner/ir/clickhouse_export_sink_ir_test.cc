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

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>

#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/ir/clickhouse_export_sink_ir.h"
#include "src/carnot/planner/ir/memory_source_ir.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/testing/protobuf.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace planner {

using ClickHouseExportSinkTest = ASTVisitorTest;

TEST_F(ClickHouseExportSinkTest, basic_export) {
  // Create a simple relation with some columns
  table_store::schema::Relation relation{
      {types::TIME64NS, types::STRING, types::INT64, types::FLOAT64},
      {"time_", "hostname", "count", "latency"},
      {types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_DURATION_NS}};

  (*compiler_state_->relation_map())["table"] = relation;

  auto src = MakeMemSource("table");
  EXPECT_OK(src->ResolveType(compiler_state_.get()));

  std::string clickhouse_dsn = "default:test_password@localhost:9000/default";
  ASSERT_OK_AND_ASSIGN(auto clickhouse_sink,
                       graph->CreateNode<ClickHouseExportSinkIR>(src->ast(), src, "http_events", clickhouse_dsn));

  clickhouse_sink->PullParentTypes();
  EXPECT_OK(clickhouse_sink->UpdateOpAfterParentTypesResolved());

  // ResolveType will try to get config from compiler state, but we'll set it directly
  // by creating a new CompilerState with ClickHouse config
  auto new_relation_map = std::make_unique<RelationMap>();
  (*new_relation_map)["table"] = relation;

  auto clickhouse_config = std::make_unique<planpb::ClickHouseConfig>();
  clickhouse_config->set_host("localhost");
  clickhouse_config->set_port(9000);
  clickhouse_config->set_username("default");
  clickhouse_config->set_password("test_password");
  clickhouse_config->set_database("default");

  auto new_compiler_state = std::make_unique<CompilerState>(
      std::move(new_relation_map),
      SensitiveColumnMap{},
      compiler_state_->registry_info(),
      compiler_state_->time_now(),
      0,  // max_output_rows_per_table
      "",  // result_address
      "",  // result_ssl_targetname
      RedactionOptions{},
      nullptr,  // endpoint_config
      nullptr,  // plugin_config
      DebugInfo{},
      std::move(clickhouse_config));

  // ResolveType will copy the config from compiler state
  EXPECT_OK(clickhouse_sink->ResolveType(new_compiler_state.get()));

  planpb::Operator pb;
  EXPECT_OK(clickhouse_sink->ToProto(&pb));

  EXPECT_EQ(pb.op_type(), planpb::CLICKHOUSE_EXPORT_SINK_OPERATOR);
  EXPECT_EQ(pb.clickhouse_sink_op().table_name(), "http_events");
  EXPECT_EQ(pb.clickhouse_sink_op().column_mappings_size(), 4);

  // Verify column mappings
  EXPECT_EQ(pb.clickhouse_sink_op().column_mappings(0).input_column_index(), 0);
  EXPECT_EQ(pb.clickhouse_sink_op().column_mappings(0).clickhouse_column_name(), "time_");
  EXPECT_EQ(pb.clickhouse_sink_op().column_mappings(0).column_type(), types::TIME64NS);

  EXPECT_EQ(pb.clickhouse_sink_op().column_mappings(1).input_column_index(), 1);
  EXPECT_EQ(pb.clickhouse_sink_op().column_mappings(1).clickhouse_column_name(), "hostname");
  EXPECT_EQ(pb.clickhouse_sink_op().column_mappings(1).column_type(), types::STRING);
}

TEST_F(ClickHouseExportSinkTest, required_input_columns) {
  table_store::schema::Relation relation{
      {types::TIME64NS, types::STRING, types::INT64},
      {"time_", "hostname", "count"},
      {types::ST_NONE, types::ST_NONE, types::ST_NONE}};

  (*compiler_state_->relation_map())["table"] = relation;

  auto src = MakeMemSource("table");
  EXPECT_OK(src->ResolveType(compiler_state_.get()));

  std::string clickhouse_dsn = "default:test_password@localhost:9000/default";
  ASSERT_OK_AND_ASSIGN(auto clickhouse_sink,
                       graph->CreateNode<ClickHouseExportSinkIR>(src->ast(), src, "http_events", clickhouse_dsn));

  clickhouse_sink->PullParentTypes();
  EXPECT_OK(clickhouse_sink->UpdateOpAfterParentTypesResolved());

  // Need to call ResolveType to populate required_column_names_
  auto clickhouse_config = std::make_unique<planpb::ClickHouseConfig>();
  clickhouse_config->set_host("localhost");
  clickhouse_config->set_port(9000);
  clickhouse_config->set_username("default");
  clickhouse_config->set_password("test_password");
  clickhouse_config->set_database("default");

  auto new_relation_map = std::make_unique<RelationMap>();
  (*new_relation_map)["table"] = relation;

  auto new_compiler_state = std::make_unique<CompilerState>(
      std::move(new_relation_map),
      SensitiveColumnMap{},
      compiler_state_->registry_info(),
      compiler_state_->time_now(),
      0,
      "",
      "",
      RedactionOptions{},
      nullptr,
      nullptr,
      DebugInfo{},
      std::move(clickhouse_config));

  EXPECT_OK(clickhouse_sink->ResolveType(new_compiler_state.get()));

  ASSERT_OK_AND_ASSIGN(auto required_input_columns, clickhouse_sink->RequiredInputColumns());
  ASSERT_EQ(required_input_columns.size(), 1);
  EXPECT_THAT(required_input_columns[0],
              ::testing::UnorderedElementsAre("time_", "hostname", "count"));
}


}  // namespace planner
}  // namespace carnot
}  // namespace px
