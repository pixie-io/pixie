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

#include <gtest/gtest.h>
#include <memory>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/planner/compiler/compiler.h"
#include "src/carnot/planner/objects/dataframe.h"
#include "src/carnot/planner/objects/exporter.h"
#include "src/carnot/planner/objects/otel.h"
#include "src/carnot/planner/objects/test_utils.h"
#include "src/carnot/planner/objects/var_table.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/shared/types/typespb/types.pb.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {
class OTelExportTest : public QLObjectTest {
 protected:
  void SetUp() override {
    QLObjectTest::SetUp();
    var_table = VarTable::Create();

    ASSERT_OK_AND_ASSIGN(auto oteltrace, OTelTraceModule::Create(ast_visitor.get()));
    ASSERT_OK_AND_ASSIGN(auto endpoint, EndpointConfig::Create(ast_visitor.get(), "", {}));
    var_table->Add("oteltrace", oteltrace);
    var_table->Add("Endpoint", endpoint);
  }

  std::shared_ptr<VarTable> var_table;
};

TEST_F(OTelExportTest, create_and_export_span) {
  std::string outpb = R"pb(
  op_type: OTEL_EXPORT_SINK_OPERATOR
  otel_sink_op {
    endpoint_config{
      url:'0.0.0.0:55690',
      attributes{
        key: 'apikey'
        value: '12345'
      }
    }
    span {
      name: "spans"
      attributes {
        name: "http.method"
        value_column: "req_method"
      }
      trace_id_column: "trace_id"
      span_id_column: "span_id"
      parent_span_id_column: "parent_span_id"
      start_time_unix_nano_column: "time_"
      end_time_unix_nano_column: "end_time"
      kind: SPAN_KIND_INTERNAL
      status_column: "status"
    }
  })pb";

  std::string script = R"(
sp = oteltrace.Span(
  name='spans',
  endpoint=Endpoint(
    url='0.0.0.0:55690',
    attributes={
      'apikey': '12345',
    }
  ),
  attributes={
      'http.method' : 'req_method',
  },
  trace_id='trace_id',
  span_id='span_id',
  parent_span_id='parent_span_id',
  start_time_unix_nano='time_',
  end_time_unix_nano='end_time',
  kind=1,
  status='status',
))";
  ASSERT_OK(ParseScript(var_table, script));
  auto sp = var_table->Lookup("sp");
  ASSERT_TRUE(Exporter::IsExporter(sp));
  auto exporter = static_cast<Exporter*>(sp.get());
  auto src = MakeMemSource();
  ASSERT_OK_AND_ASSIGN(auto df, Dataframe::Create(src, ast_visitor.get()));
  ASSERT_OK(exporter->Export(ast, df.get()));
  auto child = src->Children();
  ASSERT_EQ(child.size(), 1);
  ASSERT_MATCH(child[0], OTelExportSink());

  planpb::Operator op;
  auto otel_sink = static_cast<OTelExportSinkIR*>(child[0]);
  ASSERT_OK(otel_sink->ToProto(&op));
  EXPECT_THAT(op, testing::proto::EqualsProto(outpb));
}

TEST_F(OTelExportTest, test_verify_columns_missing_columns) {
  std::string script = R"(
sp = oteltrace.Span(
  name='spans',
  endpoint=Endpoint(
    url='0.0.0.0:55690',
    attributes={
      'apikey': '12345',
    }
  ),
  attributes={
      'http.method' : 'req_method',
  },
  trace_id='trace_id',
  span_id='span_id',
  parent_span_id='parent_span_id',
  start_time_unix_nano='time_',
  end_time_unix_nano='end_time',
  kind=1,
  status='status',
))";
  std::vector<std::pair<types::DataType, std::string>> coltypes = {
      {types::STRING, "req_method"},     {types::STRING, "trace_id"}, {types::STRING, "span_id"},
      {types::STRING, "parent_span_id"}, {types::TIME64NS, "time_"},  {types::TIME64NS, "end_time"},
      {types::INT64, "status"},
  };

  // Remove one column at a time to make sure all checks work.
  for (size_t i = 0; i < coltypes.size() + 1; ++i) {
    ASSERT_OK(ParseScript(var_table, script));
    auto sp = var_table->Lookup("sp");
    ASSERT_TRUE(Exporter::IsExporter(sp));
    auto exporter = static_cast<Exporter*>(sp.get());
    auto src = MakeMemSource("table");
    ASSERT_OK_AND_ASSIGN(auto df, Dataframe::Create(src, ast_visitor.get()));
    ASSERT_OK(exporter->Export(ast, df.get()));
    auto child = src->Children();
    ASSERT_EQ(child.size(), 1);
    auto otel_sink = static_cast<OTelExportSinkIR*>(child[0]);
    ASSERT_MATCH(child[0], OTelExportSink());
    auto relation = table_store::schema::Relation();
    for (const auto& [j, type] : Enumerate(coltypes)) {
      if (j == i) {
        continue;
      }
      relation.AddColumn(type.first, type.second);
    }
    (*compiler_state->relation_map())["table"] = relation;
    EXPECT_OK(src->ResolveType(compiler_state.get()));
    otel_sink->PullParentTypes();
    // The last iteration of the loop will have no missing columns and should be successful.
    if (i == coltypes.size()) {
      EXPECT_OK(otel_sink->UpdateOpAfterParentTypesResolved());
      continue;
    }
    EXPECT_COMPILER_ERROR(otel_sink->UpdateOpAfterParentTypesResolved(),
                          absl::Substitute("Column '$0' not found.*", coltypes[i].second));
  }
}

TEST_F(OTelExportTest, test_verify_columns_wrong_columns_type) {
  std::string script = R"(
sp = oteltrace.Span(
  name='spans',
  endpoint=Endpoint(
    url='0.0.0.0:55690',
    attributes={
      'apikey': '12345',
    }
  ),
  attributes={
      'http.method' : 'req_method',
  },
  trace_id='trace_id',
  span_id='span_id',
  parent_span_id='parent_span_id',
  start_time_unix_nano='time_',
  end_time_unix_nano='end_time',
  kind=1,
  status='status',
))";

  std::vector<std::pair<types::DataType, std::string>> coltypes = {
      {types::STRING, "req_method"},     {types::STRING, "trace_id"}, {types::STRING, "span_id"},
      {types::STRING, "parent_span_id"}, {types::TIME64NS, "time_"},  {types::TIME64NS, "end_time"},
      {types::INT64, "status"},
  };

  // Remove one column at a time to make sure all checks work.
  for (size_t i = 0; i < coltypes.size() + 1; ++i) {
    ASSERT_OK(ParseScript(var_table, script));
    auto sp = var_table->Lookup("sp");
    ASSERT_TRUE(Exporter::IsExporter(sp));
    auto exporter = static_cast<Exporter*>(sp.get());
    auto src = MakeMemSource("table");
    ASSERT_OK_AND_ASSIGN(auto df, Dataframe::Create(src, ast_visitor.get()));
    ASSERT_OK(exporter->Export(ast, df.get()));
    auto child = src->Children();
    ASSERT_EQ(child.size(), 1);
    auto otel_sink = static_cast<OTelExportSinkIR*>(child[0]);
    ASSERT_MATCH(child[0], OTelExportSink());
    auto relation = table_store::schema::Relation();
    for (const auto& [j, type] : Enumerate(coltypes)) {
      if (j == i) {
        // We switch to a type that no column uses. If a future version of
        // this test uses a FLOAT64 legitimately, the test must change.
        relation.AddColumn(types::FLOAT64, type.second);
        continue;
      }
      relation.AddColumn(type.first, type.second);
    }
    (*compiler_state->relation_map())["table"] = relation;
    EXPECT_OK(src->ResolveType(compiler_state.get()));
    otel_sink->PullParentTypes();
    // The last iteration of the loop will have no missing columns and should be successful.
    if (i == coltypes.size()) {
      EXPECT_OK(otel_sink->UpdateOpAfterParentTypesResolved());
      continue;
    }
    EXPECT_COMPILER_ERROR(otel_sink->UpdateOpAfterParentTypesResolved(),
                          absl::Substitute("Expected .* to be a $1, '$0' is of type.*",
                                           coltypes[i].second, types::ToString(coltypes[i].first)));
  }
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
