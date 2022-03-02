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

#include <absl/strings/ascii.h>
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
struct TestColumnConfig {
  std::string column_name;
  // Types that work for the column.
  std::vector<types::DataType> working_types;
  // Types that don't work.
  types::DataType bad_type;
};
struct TestCase {
  std::string name;
  std::string otel_export_expression;
  std::string otel_export_pb;
  std::vector<TestColumnConfig> allowed_column_types;
};

class OTelExportTest : public QLObjectTest, public ::testing::WithParamInterface<TestCase> {
 protected:
  void SetUp() override {
    QLObjectTest::SetUp();
    var_table = VarTable::Create();

    ASSERT_OK_AND_ASSIGN(auto oteltrace, OTelTraceModule::Create(ast_visitor.get()));
    ASSERT_OK_AND_ASSIGN(auto otelmetric, OTelMetricsModule::Create(ast_visitor.get()));
    ASSERT_OK_AND_ASSIGN(auto endpoint, EndpointConfig::Create(ast_visitor.get(), "", {}));
    var_table->Add("oteltrace", oteltrace);
    var_table->Add("otelmetric", otelmetric);
    var_table->Add("Endpoint", endpoint);
  }

  StatusOr<QLObjectPtr> ParseExpression(const std::string& expr) {
    std::string var = "sp";
    std::string script =
        absl::Substitute("$0 = $1", var, std::string(absl::StripLeadingAsciiWhitespace(expr)));
    PL_RETURN_IF_ERROR(ParseScript(var_table, script));
    return var_table->Lookup(var);
  }

  StatusOr<OTelExportSinkIR*> ParseOutOTelExportIR(const std::string& otel_export_expression,
                                                   table_store::schema::Relation relation) {
    (*compiler_state->relation_map())["table"] = std::move(relation);
    PL_ASSIGN_OR_RETURN(auto sp, ParseExpression(otel_export_expression));
    auto exporter = static_cast<Exporter*>(sp.get());
    auto src = MakeMemSource("table");
    auto df = Dataframe::Create(src, ast_visitor.get()).ConsumeValueOrDie();
    PL_RETURN_IF_ERROR(exporter->Export(ast, df.get()));
    auto child = src->Children();
    auto otel_sink = static_cast<OTelExportSinkIR*>(child[0]);
    PL_RETURN_IF_ERROR(src->ResolveType(compiler_state.get()));
    otel_sink->PullParentTypes();
    return otel_sink;
  }

  std::shared_ptr<VarTable> var_table;
};

TEST_P(OTelExportTest, parse_expression_and_output_protobuf) {
  auto tc = GetParam();
  ASSERT_OK_AND_ASSIGN(auto sp, ParseExpression(tc.otel_export_expression));
  ASSERT_TRUE(Exporter::IsExporter(sp));
  auto exporter = static_cast<Exporter*>(sp.get());
  auto src = MakeMemSource("table");
  auto df = Dataframe::Create(src, ast_visitor.get()).ConsumeValueOrDie();
  ASSERT_OK(exporter->Export(ast, df.get()));
  auto otel_sink = static_cast<OTelExportSinkIR*>(src->Children()[0]);

  planpb::Operator op;
  ASSERT_OK(otel_sink->ToProto(&op));
  EXPECT_THAT(op, testing::proto::EqualsProto(tc.otel_export_pb));
}

TEST_P(OTelExportTest, succeed_on_correct_columns) {
  auto tc = GetParam();
  for (size_t i = 0; i < tc.allowed_column_types.size(); ++i) {
    // Skip those columns where there are no alternative types. Those are tested by other tests.
    for (size_t k = 0; k < tc.allowed_column_types[i].working_types.size(); ++k) {
      // Create a relation where the ith column
      auto relation = table_store::schema::Relation();
      for (const auto& [j, type] : Enumerate(tc.allowed_column_types)) {
        if (j == i) {
          relation.AddColumn(type.working_types[k], type.column_name);
          continue;
        }
        relation.AddColumn(type.working_types[0], type.column_name);
      }
      ASSERT_OK_AND_ASSIGN(auto otel_sink,
                           ParseOutOTelExportIR(tc.otel_export_expression, std::move(relation)));

      EXPECT_OK(otel_sink->UpdateOpAfterParentTypesResolved());
    }
  }
}

TEST_P(OTelExportTest, error_on_missing_columns) {
  auto tc = GetParam();
  for (size_t i = 0; i < tc.allowed_column_types.size(); ++i) {
    // Create a relation where the ith column is not included.
    auto relation = table_store::schema::Relation();
    for (const auto& [j, type] : Enumerate(tc.allowed_column_types)) {
      if (j == i) {
        continue;
      }
      relation.AddColumn(type.working_types[0], type.column_name);
    }
    ASSERT_OK_AND_ASSIGN(auto otel_sink,
                         ParseOutOTelExportIR(tc.otel_export_expression, std::move(relation)));

    EXPECT_COMPILER_ERROR(
        otel_sink->UpdateOpAfterParentTypesResolved(),
        absl::Substitute("Column '$0' not found.*", tc.allowed_column_types[i].column_name));
  }
}

TEST_P(OTelExportTest, error_on_wrong_column_types) {
  auto tc = GetParam();
  // Remove one column at a time to make sure all checks work.
  for (size_t i = 0; i < tc.allowed_column_types.size(); ++i) {
    // Create a relation where the ith column has a type that won't work.
    auto relation = table_store::schema::Relation();
    for (const auto& [j, type] : Enumerate(tc.allowed_column_types)) {
      if (j == i) {
        relation.AddColumn(type.bad_type, type.column_name);
        continue;
      }
      relation.AddColumn(type.working_types[0], type.column_name);
    }
    ASSERT_OK_AND_ASSIGN(auto otel_sink,
                         ParseOutOTelExportIR(tc.otel_export_expression, std::move(relation)));

    EXPECT_COMPILER_ERROR(otel_sink->UpdateOpAfterParentTypesResolved(),
                          absl::Substitute("Expected .* to be.*, '$0' is of type.*",
                                           tc.allowed_column_types[i].column_name));
  }
}

constexpr char kOTelSpanExpression[] = R"pxl(
oteltrace.Span(
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
))pxl";

constexpr char kOTelSpanProto[] = R"pb(
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

constexpr char kOTelMetricGaugeExpression[] = R"pxl(
otelmetric.Metric(
  name='gc_value',
  description='The amount of program time spent in the GC',
  endpoint=Endpoint(
    url='0.0.0.0:55690',
    attributes={
      'apikey': '12345',
    }
  ),
  attributes={
      'gc' : 'gc_type',
  },
  data=otelmetric.Gauge(
    start_time_unix_nano='time_',
    time_unix_nano='end_time',
    value='gc_time'
  ),
))pxl";

constexpr char kOTelMetricGaugeProto[] = R"pb(
  op_type: OTEL_EXPORT_SINK_OPERATOR
  otel_sink_op {
    endpoint_config{
      url:'0.0.0.0:55690',
      attributes{
        key: 'apikey'
        value: '12345'
      }
    }
    metric {
      name: "gc_value"
      description:"The amount of program time spent in the GC"
      attributes {
        name: "gc"
        value_column: "gc_type"
      }
      start_time_unix_nano_column: "time_"
      time_unix_nano_column: "end_time"
      gauge {
        value_column: "gc_time"
      }
    }
  })pb";

constexpr char kOTelMetricSummaryExpression[] = R"pxl(
otelmetric.Metric(
  name='latency',
  description='The latency distribution for each time window',
  endpoint=Endpoint(
    url='0.0.0.0:55690',
    attributes={
      'apikey': '12345',
    }
  ),
  attributes={
      'k8s.service' : 'service',
  },
  data=otelmetric.Summary(
    start_time_unix_nano='time_',
    time_unix_nano='end_time',
    count='count',
    sum='sum',
    quantile_values={
      0.5: 'p50',
      0.99: 'p99',
    }
  ),
))pxl";

constexpr char kOTelMetricSummaryProto[] = R"pb(
  op_type: OTEL_EXPORT_SINK_OPERATOR
  otel_sink_op {
  endpoint_config {
    url: "0.0.0.0:55690"
    attributes {
      key: "apikey"
      value: "12345"
    }
  }
  metric {
    name: "latency"
    description: "The latency distribution for each time window"
    attributes {
      name: "k8s.service"
      value_column: "service"
    }
    start_time_unix_nano_column: "time_"
    time_unix_nano_column: "end_time"
    summary {
      count_column: "count"
      sum_column: "sum"
      quantile_values {
        quantile: 0.5
        value_column: "p50"
      }
      quantile_values {
        quantile: 0.99
        value_column: "p99"
      }
    }
  }
})pb";

INSTANTIATE_TEST_SUITE_P(ParseAndCompareProtoTestSuites, OTelExportTest,
                         ::testing::ValuesIn(std::vector<TestCase>{
                             {"Span",
                              kOTelSpanExpression,
                              kOTelSpanProto,
                              {
                                  {"req_method", {types::STRING}, types::FLOAT64},
                                  {"trace_id", {types::STRING}, types::FLOAT64},
                                  {"span_id", {types::STRING}, types::FLOAT64},
                                  {"parent_span_id", {types::STRING}, types::FLOAT64},
                                  {"time_", {types::TIME64NS}, types::FLOAT64},
                                  {"end_time", {types::TIME64NS}, types::FLOAT64},
                                  {"status", {types::INT64}, types::FLOAT64},
                              }},
                             {"Gauge",
                              kOTelMetricGaugeExpression,
                              kOTelMetricGaugeProto,
                              {
                                  {"gc_type", {types::STRING}, types::FLOAT64},
                                  {"time_", {types::TIME64NS}, types::FLOAT64},
                                  {"end_time", {types::TIME64NS}, types::FLOAT64},
                                  {"gc_time", {types::FLOAT64, types::INT64}, types::STRING},
                              }},
                             {"Summary",
                              kOTelMetricSummaryExpression,
                              kOTelMetricSummaryProto,
                              {
                                  {"service", {types::STRING}, types::FLOAT64},
                                  {"time_", {types::TIME64NS}, types::FLOAT64},
                                  {"end_time", {types::TIME64NS}, types::STRING},
                                  {"count", {types::FLOAT64}, types::STRING},
                                  {"sum", {types::FLOAT64}, types::STRING},
                                  {"p50", {types::FLOAT64}, types::STRING},
                                  {"p99", {types::FLOAT64}, types::STRING},
                              }},
                         }),
                         [](const ::testing::TestParamInfo<TestCase>& info) {
                           return info.param.name;
                         });

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
