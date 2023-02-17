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
#include "src/carnot/planner/compiler_state/compiler_state.h"
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

    ASSERT_OK_AND_ASSIGN(auto otel,
                         OTelModule::Create(compiler_state.get(), ast_visitor.get(), graph.get()));
    ASSERT_OK_AND_ASSIGN(auto otelmetric, OTelMetrics::Create(ast_visitor.get(), graph.get()));
    ASSERT_OK_AND_ASSIGN(auto oteltrace, OTelTrace::Create(ast_visitor.get(), graph.get()));
    var_table->Add("otel", otel);
    var_table->Add("otelmetric", otelmetric);
    var_table->Add("oteltrace", oteltrace);
  }

  StatusOr<OTelExportSinkIR*> ParseOutOTelExportIR(const std::string& otel_export_expression,
                                                   const table_store::schema::Relation& relation) {
    (*compiler_state->relation_map())["table"] = relation;
    auto src = MakeMemSource("table");
    auto df = Dataframe::Create(compiler_state.get(), src, ast_visitor.get()).ConsumeValueOrDie();
    var_table->Add("df", df);
    PX_ASSIGN_OR_RETURN(auto sp, ParseExpression(otel_export_expression));
    if (!Exporter::IsExporter(sp)) {
      return error::InvalidArgument("Expected exporter, received $0", sp->name());
    }
    auto exporter = static_cast<Exporter*>(sp.get());
    PX_RETURN_IF_ERROR(exporter->Export(ast, df.get()));
    auto child = src->Children();
    auto otel_sink = static_cast<OTelExportSinkIR*>(child[0]);
    PX_RETURN_IF_ERROR(src->ResolveType(compiler_state.get()));
    otel_sink->PullParentTypes();
    PX_RETURN_IF_ERROR(otel_sink->ResolveType(compiler_state.get()));
    return otel_sink;
  }
};

struct SuccessTestCase {
  std::string name;
  std::string otel_export_expression;
  table_store::schema::Relation relation;
  std::string otel_export_pb;
};

class OTelSuccessTests : public OTelExportTest,
                         public ::testing::WithParamInterface<SuccessTestCase> {};

TEST_P(OTelSuccessTests, parse_expression_and_output_protobuf) {
  auto tc = GetParam();
  ASSERT_OK_AND_ASSIGN(auto otel_export_sink,
                       ParseOutOTelExportIR(tc.otel_export_expression, tc.relation));
  planpb::Operator op;
  ASSERT_OK(otel_export_sink->ToProto(&op));
  EXPECT_THAT(op, testing::proto::EqualsProto(tc.otel_export_pb));
}

INSTANTIATE_TEST_SUITE_P(
    ParseAndCompareProtoTestSuites, OTelSuccessTests,
    ::testing::ValuesIn(std::vector<SuccessTestCase>{
        {"Gauge",
         R"pxl(
otel.Data(
  endpoint=otel.Endpoint(
    url='0.0.0.0:55690',
    headers={
      'apikey': '12345',
    }
  ),
  resource={
      'service.name' : df.service,
  },
  data=[
    otelmetric.Gauge(
      name='runtime.jvm.gc.collection',
      description='the gc collection',
      value=df.young_gc_time,
      attributes={'gc': df.young}
    )
  ]
))pxl",
         table_store::schema::Relation{
             {types::TIME64NS, types::STRING, types::STRING, types::INT64},
             {"time_", "service", "young", "young_gc_time"},
             {types::ST_NONE, types::ST_SERVICE_NAME, types::ST_NONE, types::ST_DURATION_NS},
         },
         R"pb(
op_type: OTEL_EXPORT_SINK_OPERATOR
otel_sink_op {
  endpoint_config {
    url: "0.0.0.0:55690"
    headers {
      key: "apikey"
      value: "12345"
    }
    timeout: 5
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
    name: "runtime.jvm.gc.collection"
    description: "the gc collection"
    attributes {
      name: "gc"
      column {
        column_type: STRING
        column_index: 2
      }
    }
    time_column_index: 0
    unit: "ns"
    gauge {
      int_column_index: 3
    }
  }
})pb"},
        {"time_not_first",
         R"pxl(
otel.Data(
  endpoint=otel.Endpoint(
    url='0.0.0.0:55690',
  ),
  resource={
      'service.name' : df.service,
  },
  data=[
    otelmetric.Gauge(
      name='runtime.jvm.gc.collection',
      description='the gc collection',
      value=df.young_gc_time,
    )
  ]
))pxl",
         table_store::schema::Relation{
             {types::STRING, types::TIME64NS, types::INT64},
             {"service", "time_", "young_gc_time"},
             {types::ST_SERVICE_NAME, types::ST_NONE, types::ST_DURATION_NS},
         },
         R"pb(
op_type: OTEL_EXPORT_SINK_OPERATOR
otel_sink_op {
  endpoint_config {
    url: "0.0.0.0:55690"
    timeout: 5
  }
  resource {
    attributes {
      name: "service.name"
      column {
        column_type: STRING
        column_index: 0
        can_be_json_encoded_array: true
      }
    }
  }
  metrics {
    name: "runtime.jvm.gc.collection"
    description: "the gc collection"
    time_column_index: 1
    unit: "ns"
    gauge {
      int_column_index: 2
    }
  }
})pb"},
        {"Summary",
         R"pxl(
otel.Data(
  endpoint=otel.Endpoint(
    url='0.0.0.0:55690',
  ),
  resource={
      'service.name' : df.service,
  },
  data=[
    otelmetric.Summary(
      name='http.resp.latency',
      description='the latencies that occur',
      count=df.count,
      sum=df.sum,
      quantile_values={
        0.5: df.p50,
        0.99: df.p99,
      },
      attributes={'status': df.status}
    )
  ]
))pxl",
         table_store::schema::Relation{
             {types::TIME64NS, types::STRING, types::STRING, types::INT64, types::FLOAT64,
              types::FLOAT64, types::FLOAT64},
             {"time_", "service", "status", "count", "sum", "p50", "p99"},
             {types::ST_NONE, types::ST_SERVICE_NAME, types::ST_NONE, types::ST_NONE,
              types::ST_NONE, types::ST_DURATION_NS, types::ST_DURATION_NS}},
         R"pb(
op_type: OTEL_EXPORT_SINK_OPERATOR
otel_sink_op {
  endpoint_config {
    url: "0.0.0.0:55690"
    timeout: 5
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
    description: "the latencies that occur"
    attributes {
      name: "status"
      column {
        column_type: STRING
        column_index: 2
      }
    }
    time_column_index: 0
    unit: "ns"
    summary {
      count_column_index: 3
      sum_column_index: 4
      quantile_values {
        quantile: 0.5
        value_column_index: 5
      }
      quantile_values {
        quantile: 0.99
        value_column_index: 6
      }
    }
  }
})pb"},
        {"Multiple_Data_Configs",
         R"pxl(
otel.Data(
  endpoint=otel.Endpoint(
    url='0.0.0.0:55690',
  ),
  resource={
      'service.name' : df.service,
  },
  data=[
    otelmetric.Gauge(
      name='runtime.jvm.gc.collection',
      value=df.young_gc_time,
      attributes={'gc': df.young}
    ),
    otelmetric.Gauge(
      name='runtime.jvm.gc.collection',
      value=df.full_gc_time,
      attributes={'gc': df.full}
    )
  ]
))pxl",
         table_store::schema::Relation{
             {types::TIME64NS, types::STRING, types::STRING, types::STRING, types::INT64,
              types::INT64},
             {"time_", "service", "young", "full", "young_gc_time", "full_gc_time"},
             {types::ST_NONE, types::ST_SERVICE_NAME, types::ST_NONE, types::ST_NONE,
              types::ST_DURATION_NS, types::ST_DURATION_NS},
         },
         R"pb(
op_type: OTEL_EXPORT_SINK_OPERATOR
otel_sink_op {
  endpoint_config {
    url: "0.0.0.0:55690"
    timeout: 5
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
    name: "runtime.jvm.gc.collection"
    attributes {
      name: "gc"
      column {
        column_type: STRING
        column_index: 2
      }
    }
    time_column_index: 0
    unit: "ns"
    gauge {
      int_column_index: 4
    }
  }
  metrics {
    name: "runtime.jvm.gc.collection"
    attributes {
      name: "gc"
      column {
        column_type: STRING
        column_index: 3
      }
    }
    time_column_index: 0
    unit: "ns"
    gauge {
      int_column_index: 5
    }
  }
})pb"},
        {"span_name_string",
         R"pxl(
otel.Data(
  endpoint=otel.Endpoint(
    url='0.0.0.0:55690',
  ),
  resource={
      'service.name' : df.service,
  },
  data=[
    oteltrace.Span(
      name='svc',
      start_time=df.start_time,
      end_time=df.end_time,
      attributes={'gc': df.young},
      kind=oteltrace.SPAN_KIND_CLIENT,
    ),
  ]
))pxl",
         table_store::schema::Relation{
             {types::TIME64NS, types::TIME64NS, types::STRING, types::STRING},
             {"start_time", "end_time", "service", "young"},
             {types::ST_NONE, types::ST_NONE, types::ST_SERVICE_NAME, types::ST_NONE},
         },
         R"pb(
op_type: OTEL_EXPORT_SINK_OPERATOR
otel_sink_op {
  endpoint_config {
    url: "0.0.0.0:55690"
    timeout: 5
  }
  resource {
    attributes {
      name: "service.name"
      column {
        column_type: STRING
        column_index: 2
        can_be_json_encoded_array: true
      }
    }
  }
  spans {
    name_string: "svc"
    attributes {
      name: "gc"
      column {
        column_type: STRING
        column_index: 3
      }
    }
    start_time_column_index: 0
    end_time_column_index: 1
    trace_id_column_index: -1
    span_id_column_index: -1
    parent_span_id_column_index: -1
    kind_value: 3
  }
})pb"},

        {"span_name_column",
         R"pxl(
otel.Data(
  endpoint=otel.Endpoint(
    url='0.0.0.0:55690',
  ),
  resource={
      'service.name' : df.service,
  },
  data=[
    oteltrace.Span(
      name=df.span_name,
      start_time=df.start_time,
      end_time=df.end_time,
      trace_id=df.trace_id,
      parent_span_id=df.parent_span_id,
      span_id=df.span_id,
    ),
  ]
))pxl",
         table_store::schema::Relation{
             {types::TIME64NS, types::TIME64NS, types::STRING, types::STRING, types::STRING,
              types::STRING, types::STRING, types::STRING},
             {"start_time", "end_time", "service", "young", "span_name", "trace_id", "span_id",
              "parent_span_id"},
             {types::ST_NONE, types::ST_NONE, types::ST_SERVICE_NAME, types::ST_NONE,
              types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_NONE},
         },
         R"pb(
op_type: OTEL_EXPORT_SINK_OPERATOR
otel_sink_op {
  endpoint_config {
    url: "0.0.0.0:55690"
    timeout: 5
  }
  resource {
    attributes {
      name: "service.name"
      column {
        column_type: STRING
        column_index: 2
        can_be_json_encoded_array: true
      }
    }
  }
  spans {
    name_column_index: 4
    start_time_column_index: 0
    end_time_column_index: 1
    trace_id_column_index: 5
    span_id_column_index: 6
    parent_span_id_column_index: 7
    kind_value: 2
  }
})pb"},
        {"all_attribute_types",
         R"pxl(
otel.Data(
  endpoint=otel.Endpoint(
    url='0.0.0.0:55690',
    headers={
      'apikey': '12345',
    }
  ),
  resource={
      'service.name' : df.service,
  },
  data=[
    otelmetric.Gauge(
      name='http.resp.data',
      value=df.young_gc_time,
      attributes={
        'http.status_code': df.status_code,
        'http.method': df.req_method,
        'http.proportion': df.proportion,
        'http.success': df.success,
        'http.version': '1.1',
      }
    )
  ]
))pxl",
         table_store::schema::Relation{
             {types::TIME64NS, types::STRING, types::STRING, types::INT64, types::INT64,
              types::FLOAT64, types::BOOLEAN},
             {"time_", "service", "req_method", "young_gc_time", "status_code", "proportion",
              "success"},
             {types::ST_NONE, types::ST_SERVICE_NAME, types::ST_NONE, types::ST_DURATION_NS,
              types::ST_NONE, types::ST_NONE, types::ST_NONE},
         },
         R"pb(
op_type: OTEL_EXPORT_SINK_OPERATOR
otel_sink_op {
  endpoint_config {
    url: "0.0.0.0:55690"
    headers {
      key: "apikey"
      value: "12345"
    }
    timeout: 5
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
    name: "http.resp.data"
    description: ""
    attributes {
      name: "http.status_code"
      column {
        column_type: INT64
        column_index: 4
      }
    }
    attributes {
      name: "http.method"
      column {
        column_type: STRING
        column_index: 2
      }
    }
    attributes {
      name: "http.proportion"
      column {
        column_type: FLOAT64
        column_index: 5
      }
    }
    attributes {
      name: "http.success"
      column {
        column_type: BOOLEAN
        column_index: 6
      }
    }
    attributes {
      name: "http.version"
      string_value: "1.1"
    }
    time_column_index: 0
    unit: "ns"
    gauge {
      int_column_index: 3
    }
  }
})pb"},

        {"insecure_endpoint",
         R"pxl(
otel.Data(
  endpoint=otel.Endpoint(
    url='0.0.0.0:55690',
    insecure=True,
  ),
  resource={
      'service.name' : df.service,
  },
  data=[
    otelmetric.Gauge(
      name='runtime.jvm.gc.collection',
      value=df.young_gc_time,
    )
  ]
))pxl",
         table_store::schema::Relation{
             {types::STRING, types::TIME64NS, types::INT64},
             {"service", "time_", "young_gc_time"},
             {types::ST_SERVICE_NAME, types::ST_NONE, types::ST_DURATION_NS},
         },
         R"pb(
op_type: OTEL_EXPORT_SINK_OPERATOR
otel_sink_op {
  endpoint_config {
    url: "0.0.0.0:55690"
    insecure: true
    timeout: 5
  }
  resource {
    attributes {
      name: "service.name"
      column {
        column_type: STRING
        column_index: 0
        can_be_json_encoded_array: true
      }
    }
  }
  metrics {
    name: "runtime.jvm.gc.collection"
    time_column_index: 1
    unit: "ns"
    gauge {
      int_column_index: 2
    }
  }
})pb"},
        {"timeout_endpoint",
         R"pxl(
otel.Data(
  endpoint=otel.Endpoint(
    url='0.0.0.0:55690',
    timeout=5,
  ),
  resource={
      'service.name' : df.service,
  },
  data=[
    otelmetric.Gauge(
      name='runtime.jvm.gc.collection',
      value=df.young_gc_time,
    )
  ]
))pxl",
         table_store::schema::Relation{
             {types::STRING, types::TIME64NS, types::INT64},
             {"service", "time_", "young_gc_time"},
             {types::ST_SERVICE_NAME, types::ST_NONE, types::ST_DURATION_NS},
         },
         R"pb(
op_type: OTEL_EXPORT_SINK_OPERATOR
otel_sink_op {
  endpoint_config {
    url: "0.0.0.0:55690"
    timeout: 5
  }
  resource {
    attributes {
      name: "service.name"
      column {
        column_type: STRING
        column_index: 0
        can_be_json_encoded_array: true
      }
    }
  }
  metrics {
    name: "runtime.jvm.gc.collection"
    time_column_index: 1
    unit: "ns"
    gauge {
      int_column_index: 2
    }
  }
})pb"},
        {"manually_specify_gauge_unit",
         R"pxl(
otel.Data(
  endpoint=otel.Endpoint(
    url='0.0.0.0:55690',
  ),
  resource={
      'service.name' : df.service,
  },
  data=[
    otelmetric.Gauge(
      name='runtime.jvm.gc.collection',
      unit='special_unit',
      value=df.young_gc_time,
    )
  ]
))pxl",
         table_store::schema::Relation{
             {types::TIME64NS, types::STRING, types::STRING, types::INT64},
             {"time_", "service", "young", "young_gc_time"},
             {types::ST_NONE, types::ST_SERVICE_NAME, types::ST_NONE, types::ST_DURATION_NS},
         },
         R"pb(
op_type: OTEL_EXPORT_SINK_OPERATOR
otel_sink_op {
  endpoint_config {
    url: "0.0.0.0:55690"
    timeout: 5
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
    name: "runtime.jvm.gc.collection"
    time_column_index: 0
    unit: "special_unit"
    gauge {
      int_column_index: 3
    }
  }
})pb"},
        {"manually_specify_summary_unit",
         R"pxl(
otel.Data(
  endpoint=otel.Endpoint(
    url='0.0.0.0:55690',
  ),
  resource={
      'service.name' : df.service,
  },
  data=[
    otelmetric.Summary(
      name='http.resp.latency',
      count=df.count,
      sum=df.sum,
      unit ='special_unit'
      quantile_values={
        0.5: df.p50,
      },
    )
  ]
))pxl",
         table_store::schema::Relation{
             {types::TIME64NS, types::STRING, types::STRING, types::INT64, types::FLOAT64,
              types::FLOAT64, types::FLOAT64},
             {"time_", "service", "status", "count", "sum", "p50", "p99"},
             {types::ST_NONE, types::ST_SERVICE_NAME, types::ST_NONE, types::ST_NONE,
              types::ST_NONE, types::ST_DURATION_NS, types::ST_DURATION_NS}},
         R"pb(
op_type: OTEL_EXPORT_SINK_OPERATOR
otel_sink_op {
  endpoint_config {
    url: "0.0.0.0:55690"
    timeout: 5
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
    time_column_index: 0
    unit: "special_unit"
    summary {
      count_column_index: 3
      sum_column_index: 4
      quantile_values {
        quantile: 0.5
        value_column_index: 5
      }
    }
  }
})pb"},
    }),
    [](const ::testing::TestParamInfo<SuccessTestCase>& info) { return info.param.name; });

struct ErrorTestCase {
  std::string name;
  std::string otel_export_expression;
  std::string regex_error;
};

class OTelErrorTests : public OTelExportTest,
                       public ::testing::WithParamInterface<ErrorTestCase> {};

TEST_P(OTelErrorTests, parse_expression_for_error) {
  auto tc = GetParam();
  table_store::schema::Relation relation{
      {types::TIME64NS, types::STRING, types::STRING, types::STRING, types::INT64, types::INT64},
      {"time_", "service", "young", "full", "young_gc_time", "full_gc_time"},
      {types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_DURATION_NS,
       types::ST_DURATION_NS},
  };
  auto df2 = Dataframe::Create(compiler_state.get(), MakeMemSource("table2"), ast_visitor.get())
                 .ConsumeValueOrDie();
  var_table->Add("df2", df2);
  EXPECT_THAT(ParseOutOTelExportIR(tc.otel_export_expression, relation).status(),
              HasCompilerError(tc.regex_error));
}

INSTANTIATE_TEST_SUITE_P(
    OTelErrorSuite, OTelErrorTests,
    ::testing::ValuesIn(std::vector<ErrorTestCase>{
        {"endpoint_not_specified",
         R"pxl(
otel.Data(
  resource={
      'service.name' : df.service,
  },
  data=[
    otelmetric.Gauge(
      name='gc',
      value=df.young_gc_time,
    )
  ]
))pxl",
         "no default config found for endpoint, please specify one"},
        // TODO(philkuz) address the issue where column references can be
        // interchangably used despite the different parents they reference.
        //                              {"wrong_parent_column",
        //                               R"pxl(
        // otel.Data(
        //   endpoint=otel.Endpoint(url='0.0.0.0:55690'),
        //   resource={
        //       'service.name' : df.service,
        //   },
        //   data=[
        //     otelmetric.Gauge(
        //       name='gc',
        //       value=df2.young_gc_time,
        //     )
        //   ]
        // ))pxl",
        //                               "Wrong parent column"},
        {"test_missing_service_name_resource",
         R"pxl(
otel.Data(
  endpoint=otel.Endpoint(url='0.0.0.0:55690'),
  resource={ },
  data=[
    otelmetric.Gauge(
      name='gc',
      value=df2.young_gc_time,
    )
  ]
))pxl",
         "'service.name' must be specified in resource"},
        {"must_specify_1_data",
         R"pxl(
otel.Data(
  endpoint=otel.Endpoint(url='0.0.0.0:55690'),
  resource={
      'service.name' : df.service,
  },
  data=[]
))pxl",
         "Must specify at least 1 data configuration"},
        {"gauge_name_must_follow_format1", "otelmetric.Gauge(name='0gc', value=df.young_gc_time)",
         "Metric name is invalid"},
        {"gauge_name_must_follow_format2", "otelmetric.Gauge(name='g]/[c', value=df.young_gc_time)",
         "Metric name is invalid"},
        {"summary_name_must_follow_format",
         R"pxl(
otelmetric.Summary(
  name='0gc',
  count=df.count,
  sum=df.sum,
  quantile_values={
    0.99: df.p99,
  },
))pxl",
         "Metric name is invalid"},
        {"resource_attribute_key_is_empty",
         R"pxl(
otel.Data(
  endpoint=otel.Endpoint(url='0.0.0.0:55690'),
  resource={
    'service.name': df.service,
    '': df.young,
  },
  data=[otelmetric.Gauge(name='gc', value=df.young_gc_time)],
))pxl",
         "Attribute key must be a non-empty string"},
        {"metric_attribute_key_is_empty",
         R"pxl(
otel.Data(
  endpoint=otel.Endpoint(url='0.0.0.0:55690'),
  resource={
    'service.name': df.young,
  },
  data=[otelmetric.Gauge(
    name='gc',
    value=df.young_gc_time,
    attributes={'': df.young},
  )],
))pxl",
         "Attribute key must be a non-empty string"},
    }),
    [](const ::testing::TestParamInfo<ErrorTestCase>& info) { return info.param.name; });

TEST_F(OTelExportTest, endpoint_from_compiler_context) {
  std::string otel_export_expression = R"pxl(
otel.Data(
  resource={
      'service.name' : df.service,
  },
  data=[
    otelmetric.Gauge(
      name='runtime.jvm.gc.collection',
      value=df.young_gc_time,
    )
  ]
))pxl";
  std::string expected_proto = R"pb(
op_type: OTEL_EXPORT_SINK_OPERATOR
otel_sink_op {
  endpoint_config {
    url: "px.dev:55690"
    headers {
      key: "apikey"
      value: "12345"
    }
    timeout: 6
  }
  resource {
    attributes {
      name: "service.name"
      column {
        column_type: STRING
        column_index: 1
      }
    }
  }
  metrics {
    name: "runtime.jvm.gc.collection"
    time_column_index: 0
    unit: "ns"
    gauge {
      int_column_index: 3
    }
  }
})pb";
  table_store::schema::Relation relation{
      {types::TIME64NS, types::STRING, types::STRING, types::INT64},
      {"time_", "service", "young", "young_gc_time"},
      {types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_DURATION_NS},
  };
  auto endpoint_config = std::make_unique<planpb::OTelEndpointConfig>();
  endpoint_config->set_url(("px.dev:55690"));
  endpoint_config->set_timeout(6);
  (*endpoint_config->mutable_headers())["apikey"] = "12345";
  CompilerState compiler_state(std::make_unique<RelationMap>(),
                               /* sensitive_columns */ SensitiveColumnMap{}, info.get(),
                               /* time_now */ 0,
                               /* max_output_rows_per_table */ 0, "addrr", "result_ssl_targetname",
                               /* redaction_options */ RedactionOptions{},
                               std::move(endpoint_config), nullptr, planner::DebugInfo{});

  // Create the OTel Module with a compiler_state that has an endpoint config.
  ASSERT_OK_AND_ASSIGN(auto otel,
                       OTelModule::Create(&compiler_state, ast_visitor.get(), graph.get()));
  var_table->Add("otel", otel);
  ASSERT_OK_AND_ASSIGN(auto otel_export_sink,
                       ParseOutOTelExportIR(otel_export_expression, relation));
  planpb::Operator op;
  ASSERT_OK(otel_export_sink->ToProto(&op));
  EXPECT_THAT(op, testing::proto::EqualsProto(expected_proto));
}

TEST_F(OTelExportTest, resource_attributes_from_debug) {
  std::string otel_export_expression = R"pxl(
otel.Data(
  resource={
      'service.name' : df.service,
  },
  data=[
    otelmetric.Gauge(
      name='runtime.jvm.gc.collection',
      value=df.young_gc_time,
    ),
    oteltrace.Span(
      name='svc',
      start_time=df.start_time,
      end_time=df.time_,
      kind=oteltrace.SPAN_KIND_CLIENT,
    ),
  ]
))pxl";
  std::string expected_proto = R"pb(
op_type: OTEL_EXPORT_SINK_OPERATOR
otel_sink_op {
  endpoint_config {
    url: "px.dev:55690"
  }
  resource {
    attributes {
      name: "service.name"
      column {
        column_type: STRING
        column_index: 1
      }
    }
    attributes {
      name: "pixie_cloud_addr"
      string_value: "px.dev"
    }
  }
  metrics {
    name: "runtime.jvm.gc.collection"
    time_column_index: 0
    unit: "ns"
    gauge {
      int_column_index: 3
    }
  }
  spans {
    name_string: "svc"
    start_time_column_index: 4
    end_time_column_index: 0
    trace_id_column_index: -1
    span_id_column_index: -1
    parent_span_id_column_index: -1
    kind_value: 3
  }
})pb";
  table_store::schema::Relation relation{
      {types::TIME64NS, types::STRING, types::STRING, types::INT64, types::TIME64NS},
      {"time_", "service", "young", "young_gc_time", "start_time"},
      {types::ST_NONE, types::ST_NONE, types::ST_NONE, types::ST_DURATION_NS, types::ST_NONE},
  };
  auto endpoint_config = std::make_unique<planpb::OTelEndpointConfig>();
  endpoint_config->set_url(("px.dev:55690"));
  CompilerState compiler_state(
      std::make_unique<RelationMap>(), /* sensitive_columns */ SensitiveColumnMap{}, info.get(),
      /* time_now */ 0,
      /* max_output_rows_per_table */ 0, "addrr", "result_ssl_targetname",
      /* redaction_options */ RedactionOptions{}, std::move(endpoint_config), nullptr,
      planner::DebugInfo{{{"pixie_cloud_addr", "px.dev"}}});

  // Create the OTel Module with a compiler_state that has an endpoint config.
  ASSERT_OK_AND_ASSIGN(auto otel,
                       OTelModule::Create(&compiler_state, ast_visitor.get(), graph.get()));
  var_table->Add("otel", otel);
  ASSERT_OK_AND_ASSIGN(auto otel_export_sink,
                       ParseOutOTelExportIR(otel_export_expression, relation));
  planpb::Operator op;
  ASSERT_OK(otel_export_sink->ToProto(&op));
  EXPECT_THAT(op, testing::proto::EqualsProto(expected_proto));
}

TEST_F(OTelExportTest, valid_metric_names) {
  auto df = Dataframe::Create(compiler_state.get(), MakeMemSource(), ast_visitor.get())
                .ConsumeValueOrDie();
  var_table->Add("df", df);
  ASSERT_OK(
      ParseExpression(
          R"pxl(otelmetric.Gauge(name='abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_.-0123456789', value=df.gc))pxl")
          .status());
  ASSERT_OK(ParseExpression(R"pxl(otelmetric.Gauge(name='one.two-3_', value=df.gc))pxl").status());
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
