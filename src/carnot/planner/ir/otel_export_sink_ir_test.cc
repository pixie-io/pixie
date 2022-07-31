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
#include <queue>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_set.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <pypa/ast/ast.hh>
#include <sole.hpp>

#include "src/carnot/planner/compiler/analyzer/resolve_types_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/ir/ir_node.h"
#include "src/carnot/planner/ir/memory_source_ir.h"
#include "src/carnot/planner/ir/otel_export_sink_ir.h"
#include "src/carnot/planner/ir/pattern_match.h"
#include "src/carnot/planner/metadata/metadata_handler.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/testing/protobuf.h"
#include "src/shared/types/typespb/types.pb.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace planner {
using OTelExportSinkTest = ASTVisitorTest;

ColumnIR* CreateTypedColumn(IR* graph, const std::string& name,
                            table_store::schema::Relation* relation) {
  pypa::AstPtr ast = std::make_shared<pypa::Ast>(pypa::AstType::Bool);
  ast->line = 0;
  ast->column = 0;
  ColumnIR* column = graph->CreateNode<ColumnIR>(ast, name, 0).ConsumeValueOrDie();
  auto type =
      ValueType::Create(relation->GetColumnType(name), relation->GetColumnSemanticType(name));
  EXPECT_OK(column->SetResolvedType(type));
  return column;
}

struct MetricTestCase {
  std::string name;
  // The extra columns in the relation.
  table_store::schema::Relation extra_relation;
  std::string gauge_proto;
  std::function<OTelExportSinkIR*(IR* graph, OperatorIR* parent,
                                  table_store::schema::Relation* relation)>
      create_export_sink;
};

class OTelMetricTest : public OTelExportSinkTest,
                       public ::testing::WithParamInterface<MetricTestCase> {};

TEST_P(OTelMetricTest, to_proto) {
  auto tc = GetParam();
  auto src = MakeMemSource("table");
  OTelExportSinkIR* otel_sink = tc.create_export_sink(graph.get(), src, &tc.extra_relation);

  (*compiler_state_->relation_map())["table"] = tc.extra_relation;
  EXPECT_OK(src->ResolveType(compiler_state_.get()));
  otel_sink->PullParentTypes();
  EXPECT_OK(otel_sink->UpdateOpAfterParentTypesResolved());

  planpb::Operator otelpb;
  EXPECT_OK(otel_sink->ToProto(&otelpb));

  EXPECT_THAT(otelpb.otel_sink_op(), testing::proto::EqualsProto(tc.gauge_proto));
}

TEST_P(OTelMetricTest, required_input_columns) {
  auto tc = GetParam();
  OTelExportSinkIR* otel_sink =
      tc.create_export_sink(graph.get(), MakeMemSource("table"), &tc.extra_relation);

  ASSERT_OK_AND_ASSIGN(auto required_input_columns, otel_sink->RequiredInputColumns());
  ASSERT_EQ(required_input_columns.size(), 1);
  EXPECT_THAT(required_input_columns[0],
              ::testing::UnorderedElementsAreArray(tc.extra_relation.col_names()));
}

TEST_P(OTelMetricTest, copy_from_node) {
  auto tc = GetParam();
  auto src = MakeMemSource("table");
  OTelExportSinkIR* otel_sink = tc.create_export_sink(graph.get(), src, &tc.extra_relation);

  (*compiler_state_->relation_map())["table"] = tc.extra_relation;
  EXPECT_OK(src->ResolveType(compiler_state_.get()));
  otel_sink->PullParentTypes();
  EXPECT_OK(otel_sink->UpdateOpAfterParentTypesResolved());

  // Clone() calls CopyFromNodeImpl() and is often where CopyFromNodeImpl misses some cases.
  auto cloned_graph = graph->Clone().ConsumeValueOrDie();
  auto cloned_otel_sinks = cloned_graph->FindNodesOfType(IRNodeType::kOTelExportSink);
  ASSERT_EQ(cloned_otel_sinks.size(), 1);
  auto cloned_sink = static_cast<OTelExportSinkIR*>(cloned_otel_sinks[0]);

  // We make sure the cloned otel sink writes the correct proto.
  planpb::Operator otelpb;
  EXPECT_OK(cloned_sink->ToProto(&otelpb));

  EXPECT_THAT(otelpb.otel_sink_op(), testing::proto::EqualsProto(tc.gauge_proto));
}

INSTANTIATE_TEST_SUITE_P(
    GaugeTests, OTelMetricTest,
    ::testing::ValuesIn(std::vector<MetricTestCase>{
        {
            "endpoint_and_resource_configs",
            table_store::schema::Relation{
                {types::TIME64NS, types::STRING, types::STRING, types::FLOAT64},
                {"time_", "service", "req_method", "latency_ns"},
                {types::ST_NONE, types::ST_SERVICE_NAME, types::ST_NONE, types::ST_DURATION_NS}},
            R"pb(
            endpoint_config {
              url: "otlp.px.dev",
              headers {
                key: "api_key"
                value: "abcd"
              }
            }
            resource {
              attributes {
                name: "service.name"
                column {
                  column_type: STRING
                  column_index: 1
                  can_be_json_encoded_array: true
                }
              }
            }
            metrics {
              name: "http.resp.latency"
              description: "the latency"
              unit: "ns"
              attributes {
                name: "http.method"
                column {
                  column_type: STRING
                  column_index: 2
                }
              }
              time_column_index: 0
              gauge { float_column_index: 3 }
            })pb",
            [](IR* graph, OperatorIR* parent, table_store::schema::Relation* relation) {
              OTelData data;
              data.endpoint_config.set_url("otlp.px.dev");
              (*data.endpoint_config.mutable_headers())["api_key"] = "abcd";

              data.resource_attributes.push_back(
                  {"service.name", CreateTypedColumn(graph, "service", relation), ""});

              auto& metric = data.metrics.emplace_back();
              metric.name = "http.resp.latency";
              metric.description = "the latency";
              metric.time_column = CreateTypedColumn(graph, "time_", relation);
              metric.attributes = {
                  {"http.method", CreateTypedColumn(graph, "req_method", relation), ""}};

              auto latency_col = CreateTypedColumn(graph, "latency_ns", relation);
              metric.unit_column = latency_col;
              metric.metric = OTelMetricGauge{latency_col};
              return graph->CreateNode<OTelExportSinkIR>(parent->ast(), parent, data)
                  .ConsumeValueOrDie();
            },
        },
        {
            "gauge_int",
            table_store::schema::Relation{{types::TIME64NS, types::STRING, types::INT64},
                                          {"time_", "req_method", "latency_ns"},
                                          {types::ST_NONE, types::ST_NONE, types::ST_DURATION_NS}},
            R"pb(
            endpoint_config {}
            resource {}
            metrics {
              name: "http.resp.latency"
              unit: "ns"
              time_column_index: 0
              attributes {
                name: "http.method"
                column {
                  column_type: STRING
                  column_index: 1
                }
              }
              gauge {
                int_column_index: 2
              }
            })pb",
            [](IR* graph, OperatorIR* parent, table_store::schema::Relation* relation) {
              OTelData data;
              auto& metric = data.metrics.emplace_back();
              metric.name = "http.resp.latency";
              metric.time_column = CreateTypedColumn(graph, "time_", relation);
              metric.attributes = {
                  {"http.method", CreateTypedColumn(graph, "req_method", relation), ""}};

              auto latency_col = CreateTypedColumn(graph, "latency_ns", relation);
              metric.unit_column = latency_col;
              metric.metric = OTelMetricGauge{latency_col};
              return graph->CreateNode<OTelExportSinkIR>(parent->ast(), parent, data)
                  .ConsumeValueOrDie();
            },
        },
        {
            "time_is_not_first_column",
            table_store::schema::Relation{{types::INT64, types::TIME64NS},
                                          {"latency_ns", "time_"},
                                          {types::ST_DURATION_NS, types::ST_NONE}},
            R"pb(
            endpoint_config {}
            resource {}
            metrics {
              name: "http.resp.latency"
              unit: "ns"
              time_column_index: 1
              gauge {
                int_column_index: 0
              }
            })pb",
            [](IR* graph, OperatorIR* parent, table_store::schema::Relation* relation) {
              OTelData data;
              auto& metric = data.metrics.emplace_back();
              metric.name = "http.resp.latency";
              metric.time_column = CreateTypedColumn(graph, "time_", relation);

              auto latency_col = CreateTypedColumn(graph, "latency_ns", relation);
              metric.unit_column = latency_col;
              metric.metric = OTelMetricGauge{latency_col};
              return graph->CreateNode<OTelExportSinkIR>(parent->ast(), parent, data)
                  .ConsumeValueOrDie();
            },
        },

        {
            "multi_gauge",
            table_store::schema::Relation{{types::TIME64NS, types::INT64},
                                          {"time_", "latency_ns"},
                                          {types::ST_NONE, types::ST_DURATION_NS}},
            R"pb(
            endpoint_config {}
            resource {}
            metrics {
              name: "gauge0"
              unit: "ns"
              time_column_index: 0
              gauge {
                int_column_index: 1
              }
            }
            metrics {
              name: "gauge1"
              unit: "ns"
              time_column_index: 0
              gauge {
                int_column_index: 1
              }
            })pb",
            [](IR* graph, OperatorIR* parent, table_store::schema::Relation* relation) {
              OTelData data;
              for (int64_t i = 0; i < 2; i++) {
                auto& metric = data.metrics.emplace_back();
                metric.name = absl::Substitute("gauge$0", i);
                metric.time_column = CreateTypedColumn(graph, "time_", relation);

                auto latency_col = CreateTypedColumn(graph, "latency_ns", relation);
                metric.unit_column = latency_col;
                metric.metric = OTelMetricGauge{latency_col};
              }
              return graph->CreateNode<OTelExportSinkIR>(parent->ast(), parent, data)
                  .ConsumeValueOrDie();
            },
        },

        {
            "gauge_float64",
            table_store::schema::Relation{{types::TIME64NS, types::FLOAT64},
                                          {"time_", "latency_ns"},
                                          {types::ST_NONE, types::ST_DURATION_NS}},
            R"pb(
            endpoint_config {}
            resource {}
            metrics {
              name: "http.resp.latency"
              unit: "ns"
              time_column_index: 0
              gauge {
                float_column_index: 1
              }
            })pb",
            [](IR* graph, OperatorIR* parent, table_store::schema::Relation* relation) {
              OTelData data;

              auto& metric = data.metrics.emplace_back();
              metric.name = "http.resp.latency";
              metric.time_column = CreateTypedColumn(graph, "time_", relation);

              auto latency_col = CreateTypedColumn(graph, "latency_ns", relation);
              metric.unit_column = latency_col;
              metric.metric = OTelMetricGauge{latency_col};
              return graph->CreateNode<OTelExportSinkIR>(parent->ast(), parent, data)
                  .ConsumeValueOrDie();
            },
        },
        {
            "summary",
            table_store::schema::Relation{
                {types::TIME64NS, types::INT64, types::FLOAT64, types::FLOAT64, types::FLOAT64},
                {"time_", "count", "sum", "p50", "p99"},
                {types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_DURATION_NS,
                 types::ST_DURATION_NS}},
            R"pb(
            endpoint_config {}
            resource {}
            metrics {
              name: "http.resp.latency_distribution"
              unit: "ns"
              time_column_index: 0
              summary {
                count_column_index: 1
                sum_column_index: 2
                quantile_values {
                  quantile: 0.5
                  value_column_index: 3
                }
                quantile_values {
                  quantile: 0.99
                  value_column_index: 4
                }
              }
            })pb",
            [](IR* graph, OperatorIR* parent, table_store::schema::Relation* relation) {
              OTelData data;

              auto& metric = data.metrics.emplace_back();
              metric.name = "http.resp.latency_distribution";
              metric.time_column = CreateTypedColumn(graph, "time_", relation);

              auto p50_col = CreateTypedColumn(graph, "p50", relation);
              auto p99_col = CreateTypedColumn(graph, "p99", relation);
              metric.unit_column = p50_col;
              metric.metric = OTelMetricSummary{CreateTypedColumn(graph, "count", relation),
                                                CreateTypedColumn(graph, "sum", relation),
                                                {{0.5, p50_col}, {0.99, p99_col}}};
              return graph->CreateNode<OTelExportSinkIR>(parent->ast(), parent, data)
                  .ConsumeValueOrDie();
            },
        },
        {
            "span_all_ids_specified",
            table_store::schema::Relation{
                {types::TIME64NS, types::TIME64NS, types::STRING, types::STRING, types::STRING,
                 types::STRING},
                {"start_time", "end_time", "trace_id", "span_id", "parent_span_id", "req_method"},
                {types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_NONE,
                 types::ST_NONE}},
            R"pb(
            endpoint_config {}
            resource {}
            spans {
              name_string: "span"
              start_time_column_index: 0
              end_time_column_index: 1
              attributes {
                name: "req_method"
                column {
                  column_type: STRING
                  column_index: 5
                }
              }
              trace_id_column_index: 2
              span_id_column_index: 3
              parent_span_id_column_index: 4
              kind_value: 3
            }
            )pb",
            [](IR* graph, OperatorIR* parent, table_store::schema::Relation* relation) {
              OTelData data;

              auto& span = data.spans.emplace_back();
              span.name = "span";
              span.start_time_column = CreateTypedColumn(graph, "start_time", relation);
              span.end_time_column = CreateTypedColumn(graph, "end_time", relation);
              span.trace_id_column = CreateTypedColumn(graph, "trace_id", relation);
              span.span_id_column = CreateTypedColumn(graph, "span_id", relation);
              span.parent_span_id_column = CreateTypedColumn(graph, "parent_span_id", relation);
              span.attributes.push_back(
                  {"req_method", CreateTypedColumn(graph, "req_method", relation), ""});
              span.span_kind = 3;

              return graph->CreateNode<OTelExportSinkIR>(parent->ast(), parent, data)
                  .ConsumeValueOrDie();
            },
        },
        {
            "span_not_specified_and_name_col",
            table_store::schema::Relation{{types::TIME64NS, types::TIME64NS, types::STRING},
                                          {"start_time", "end_time", "name_column"},
                                          {types::ST_NONE, types::ST_NONE, types::ST_NONE}},
            R"pb(
            endpoint_config {}
            resource {}
            spans {
              name_column_index: 2
              start_time_column_index: 0
              end_time_column_index: 1
              trace_id_column_index: -1
              span_id_column_index: -1
              parent_span_id_column_index: -1
              kind_value: 2
            }
            )pb",
            [](IR* graph, OperatorIR* parent, table_store::schema::Relation* relation) {
              OTelData data;

              auto& span = data.spans.emplace_back();
              span.name = "span";
              span.start_time_column = CreateTypedColumn(graph, "start_time", relation);
              span.end_time_column = CreateTypedColumn(graph, "end_time", relation);
              span.name = CreateTypedColumn(graph, "name_column", relation);
              span.span_kind = 2;

              return graph->CreateNode<OTelExportSinkIR>(parent->ast(), parent, data)
                  .ConsumeValueOrDie();
            },
        },
        {
            "string_value_attributes",
            table_store::schema::Relation{{types::TIME64NS, types::INT64},
                                          {"time_", "latency_ns"},
                                          {types::ST_NONE, types::ST_NONE}},
            R"pb(
            endpoint_config {}
            resource {
              attributes {
                name: "pixie.cloud.addr"
                string_value: "localhost"
              }
            }
            metrics {
              name: "http.resp.latency"
              time_column_index: 0
              attributes {
                name: "req_method"
                string_value: "GET"
              }
              gauge {
                int_column_index: 1
              }
            })pb",
            [](IR* graph, OperatorIR* parent, table_store::schema::Relation* relation) {
              OTelData data;
              data.resource_attributes.push_back({"pixie.cloud.addr", nullptr, "localhost"});
              auto& metric = data.metrics.emplace_back();
              metric.name = "http.resp.latency";
              metric.time_column = CreateTypedColumn(graph, "time_", relation);
              metric.attributes = {{"req_method", nullptr, "GET"}};

              auto latency_col = CreateTypedColumn(graph, "latency_ns", relation);
              metric.unit_column = latency_col;
              metric.metric = OTelMetricGauge{latency_col};
              return graph->CreateNode<OTelExportSinkIR>(parent->ast(), parent, data)
                  .ConsumeValueOrDie();
            },
        },
    }),
    [](const ::testing::TestParamInfo<MetricTestCase>& info) { return info.param.name; });

struct WrongColumnTypesTestCase {
  std::string name;
  table_store::schema::Relation relation;
  std::string error_regex;
  std::function<OTelExportSinkIR*(IR* graph, OperatorIR* parent,
                                  table_store::schema::Relation* relation)>
      create_export_sink;
};

class WrongColumnTypesTest : public OTelExportSinkTest,
                             public ::testing::WithParamInterface<WrongColumnTypesTestCase> {};
TEST_P(WrongColumnTypesTest, wrong_args) {
  auto tc = GetParam();

  (*compiler_state_->relation_map())["table"] = tc.relation;

  auto src = MakeMemSource("table");
  OTelExportSinkIR* otel_sink = tc.create_export_sink(graph.get(), src, &tc.relation);
  EXPECT_OK(src->ResolveType(compiler_state_.get()));
  otel_sink->PullParentTypes();
  EXPECT_OK(otel_sink->UpdateOpAfterParentTypesResolved());

  planpb::Operator otelpb;
  EXPECT_COMPILER_ERROR(otel_sink->ToProto(&otelpb), tc.error_regex);
}

OTelExportSinkIR* CreateGauge(IR* graph, OperatorIR* parent,
                              table_store::schema::Relation* relation) {
  OTelData data;

  auto& metric = data.metrics.emplace_back();
  metric.name = "http.resp.latency";
  metric.time_column = CreateTypedColumn(graph, "time_", relation);

  auto latency_col = CreateTypedColumn(graph, "latency_ns", relation);
  metric.unit_column = latency_col;
  metric.metric = OTelMetricGauge{latency_col};
  return graph->CreateNode<OTelExportSinkIR>(parent->ast(), parent, data).ConsumeValueOrDie();
}

OTelExportSinkIR* CreateSummary(IR* graph, OperatorIR* parent,
                                table_store::schema::Relation* relation) {
  OTelData data;

  auto& metric = data.metrics.emplace_back();
  metric.name = "http.resp.latency_distribution";
  metric.time_column = CreateTypedColumn(graph, "time_", relation);

  auto p50_col = CreateTypedColumn(graph, "p50", relation);
  auto p99_col = CreateTypedColumn(graph, "p99", relation);
  metric.unit_column = p50_col;
  metric.metric = OTelMetricSummary{CreateTypedColumn(graph, "count", relation),
                                    CreateTypedColumn(graph, "sum", relation),
                                    {{0.5, p50_col}, {0.99, p99_col}}};
  return graph->CreateNode<OTelExportSinkIR>(parent->ast(), parent, data).ConsumeValueOrDie();
}

OTelExportSinkIR* CreateSpanWithNameString(IR* graph, OperatorIR* parent,
                                           table_store::schema::Relation* relation) {
  OTelData data;

  auto& span = data.spans.emplace_back();
  span.name = "http.name";
  span.start_time_column = CreateTypedColumn(graph, "start_time", relation);
  span.end_time_column = CreateTypedColumn(graph, "end_time", relation);
  span.trace_id_column = CreateTypedColumn(graph, "trace_id", relation);
  span.span_id_column = CreateTypedColumn(graph, "span_id", relation);
  span.parent_span_id_column = CreateTypedColumn(graph, "parent_span_id", relation);

  return graph->CreateNode<OTelExportSinkIR>(parent->ast(), parent, data).ConsumeValueOrDie();
}

INSTANTIATE_TEST_SUITE_P(
    ErrorTests, WrongColumnTypesTest,
    ::testing::ValuesIn(std::vector<WrongColumnTypesTestCase>{
        {
            "gauge_wrong_time_type",
            table_store::schema::Relation{
                {types::INT64, types::STRING, types::STRING, types::INT64},
                {"time_", "service", "req_method", "latency_ns"}},
            "Expected time column 'time_' to be TIME64NS, received INT64",
            &CreateGauge,
        },
        {
            "gauge_wrong_value_type",
            table_store::schema::Relation{
                {types::TIME64NS, types::STRING, types::STRING, types::STRING},
                {"time_", "service", "req_method", "latency_ns"}},
            "Expected value column 'latency_ns' to be INT64 or FLOAT64, received STRING",
            &CreateGauge,
        },
        {
            "summary_wrong_time_type",
            table_store::schema::Relation{
                {types::INT64, types::INT64, types::FLOAT64, types::FLOAT64, types::FLOAT64},
                {"time_", "count", "sum", "p50", "p99"},
                {types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_DURATION_NS,
                 types::ST_DURATION_NS}},
            "Expected time column 'time_' to be TIME64NS, received INT64",
            &CreateSummary,
        },
        {
            "summary_wrong_count_type",
            table_store::schema::Relation{
                {types::TIME64NS, types::FLOAT64, types::FLOAT64, types::FLOAT64, types::FLOAT64},
                {"time_", "count", "sum", "p50", "p99"},
                {types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_DURATION_NS,
                 types::ST_DURATION_NS}},
            "Expected count column 'count' to be INT64, received FLOAT64",
            &CreateSummary,
        },
        {
            "summary_wrong_sum_type",
            table_store::schema::Relation{
                {types::TIME64NS, types::INT64, types::INT64, types::FLOAT64, types::FLOAT64},
                {"time_", "count", "sum", "p50", "p99"},
                {types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_DURATION_NS,
                 types::ST_DURATION_NS}},
            "Expected sum column 'sum' to be FLOAT64, received INT64",
            &CreateSummary,
        },
        {
            "summary_wrong_quantile_value_type",
            table_store::schema::Relation{
                {types::TIME64NS, types::INT64, types::FLOAT64, types::INT64, types::INT64},
                {"time_", "count", "sum", "p50", "p99"},
                {types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_DURATION_NS,
                 types::ST_DURATION_NS}},
            "Expected quantile column 'p50' to be FLOAT64, received INT64",
            &CreateSummary,
        },
        {
            "unsupported_metric_attribute_type",
            table_store::schema::Relation{{types::TIME64NS, types::TIME64NS, types::INT64},
                                          {"time_", "req_method", "latency_ns"},
                                          {types::ST_NONE, types::ST_NONE, types::ST_DURATION_NS}},
            "Expected attribute column 'req_method' to be .* received TIME64NS",
            [](IR* graph, OperatorIR* parent, table_store::schema::Relation* relation) {
              OTelData data;

              auto& metric = data.metrics.emplace_back();
              metric.name = "http.resp.latency";
              metric.time_column = CreateTypedColumn(graph, "time_", relation);
              metric.attributes = {
                  {"http.method", CreateTypedColumn(graph, "req_method", relation), ""}};

              auto latency_col = CreateTypedColumn(graph, "latency_ns", relation);
              metric.unit_column = latency_col;
              metric.metric = OTelMetricGauge{latency_col};
              return graph->CreateNode<OTelExportSinkIR>(parent->ast(), parent, data)
                  .ConsumeValueOrDie();
            },
        },

        {
            "unsupported_resource_attribute_type",
            table_store::schema::Relation{{types::TIME64NS, types::TIME64NS, types::INT64},
                                          {"time_", "service", "latency_ns"},
                                          {types::ST_NONE, types::ST_NONE, types::ST_DURATION_NS}},
            "Expected attribute column 'service' to be .* received TIME64NS",
            [](IR* graph, OperatorIR* parent, table_store::schema::Relation* relation) {
              OTelData data;

              auto& metric = data.metrics.emplace_back();
              metric.name = "http.resp.latency";
              metric.time_column = CreateTypedColumn(graph, "time_", relation);
              data.resource_attributes.push_back(
                  {"service.name", CreateTypedColumn(graph, "service", relation), ""});

              auto latency_col = CreateTypedColumn(graph, "latency_ns", relation);
              metric.unit_column = latency_col;
              metric.metric = OTelMetricGauge{latency_col};
              return graph->CreateNode<OTelExportSinkIR>(parent->ast(), parent, data)
                  .ConsumeValueOrDie();
            },
        },
        {
            "span_start_time_wrong",
            table_store::schema::Relation{
                {types::INT64, types::TIME64NS, types::STRING, types::STRING, types::STRING},
                {"start_time", "end_time", "trace_id", "span_id", "parent_span_id"},
                {types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_NONE}},
            "Expected time column 'start_time' to be TIME64NS, received INT64",
            &CreateSpanWithNameString,
        },
        {
            "span_end_time_wrong",
            table_store::schema::Relation{
                {types::TIME64NS, types::INT64, types::STRING, types::STRING, types::STRING},
                {"start_time", "end_time", "trace_id", "span_id", "parent_span_id"},
                {types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_NONE}},
            "Expected time column 'end_time' to be TIME64NS, received INT64",
            &CreateSpanWithNameString,
        },
        {
            "span_id_column_wrong",
            table_store::schema::Relation{
                {types::TIME64NS, types::TIME64NS, types::STRING, types::INT64, types::STRING},
                {"start_time", "end_time", "trace_id", "span_id", "parent_span_id"},
                {types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_NONE}},
            "Expected span_id column 'span_id' to be STRING, received INT64",
            &CreateSpanWithNameString,
        },
        {
            "trace_id_column_wrong",
            table_store::schema::Relation{
                {types::TIME64NS, types::TIME64NS, types::INT64, types::STRING, types::STRING},
                {"start_time", "end_time", "trace_id", "span_id", "parent_span_id"},
                {types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_NONE}},
            "Expected trace_id column 'trace_id' to be STRING, received INT64",
            &CreateSpanWithNameString,
        },
        {
            "parent_span_id_column_wrong",
            table_store::schema::Relation{
                {types::TIME64NS, types::TIME64NS, types::STRING, types::STRING, types::INT64},
                {"start_time", "end_time", "trace_id", "span_id", "parent_span_id"},
                {types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_NONE}},
            "Expected parent_span_id column 'parent_span_id' to be STRING, received INT64",
            &CreateSpanWithNameString,
        },
        {
            "span_name_column_wrong",
            table_store::schema::Relation{{types::TIME64NS, types::TIME64NS, types::INT64},
                                          {"start_time", "end_time", "req_path"},
                                          {types::ST_NONE, types::ST_NONE, types::ST_NONE}},
            "Expected name column 'req_path' to be STRING, received INT64",
            [](IR* graph, OperatorIR* parent, table_store::schema::Relation* relation) {
              OTelData data;

              auto& span = data.spans.emplace_back();
              span.name = CreateTypedColumn(graph, "req_path", relation);
              span.start_time_column = CreateTypedColumn(graph, "start_time", relation);
              span.end_time_column = CreateTypedColumn(graph, "end_time", relation);
              return graph->CreateNode<OTelExportSinkIR>(parent->ast(), parent, data)
                  .ConsumeValueOrDie();
            },
        },
    }),
    [](const ::testing::TestParamInfo<WrongColumnTypesTestCase>& info) { return info.param.name; });
}  // namespace planner
}  // namespace carnot
}  // namespace px
