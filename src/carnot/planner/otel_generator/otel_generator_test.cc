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

#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <memory>
#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/api/proto/uuidpb/uuid.pb.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/logical_planner.h"
#include "src/carnot/planner/otel_generator/otel_generator.h"
#include "src/carnot/planner/rules/rules.h"
#include "src/carnot/planner/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/testing/protobuf.h"
#include "src/common/testing/status.h"

namespace px {
namespace carnot {
namespace planner {

using testutils::EqualsProto;

class OTelGeneratorTest : public ::testing::Test {
 protected:
  void SetUp() {
    info_ = udfexporter::ExportUDFInfo().ConsumeValueOrDie()->info_pb();
    registry_info_ = std::make_unique<planner::RegistryInfo>();
    PX_CHECK_OK(registry_info_->Init(info_));
  }
  udfspb::UDFInfo info_;
  std::unique_ptr<planner::RegistryInfo> registry_info_;
};

struct GetUnusedVarNameTestCase {
  std::string name;
  std::string base_name;
  std::string expected_name;
};
class GetUnusedVarNameTest : public OTelGeneratorTest,
                             public ::testing::WithParamInterface<GetUnusedVarNameTestCase> {};

TEST_P(GetUnusedVarNameTest, GetUnusedVarName) {
  auto state = testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CompilerState> compiler_state,
                       CreateCompilerState(state, registry_info_.get(), /* max_output_rows */ 0));
  ASSERT_OK_AND_ASSIGN(auto unique_name,
                       OTelGenerator::GetUnusedVarName(compiler_state.get(), R"pxl(
import px
df = px.DataFrame('http_events', start_time='-5m')
px.display(df)
)pxl",
                                                       GetParam().base_name));
  EXPECT_EQ(unique_name, GetParam().expected_name);
}
INSTANTIATE_TEST_SUITE_P(GetUnusedVarNameTestSuite, GetUnusedVarNameTest,
                         ::testing::ValuesIn(std::vector<GetUnusedVarNameTestCase>{
                             {"match_user_defined", "df", "df_0"},
                             {"match_imported", "px", "px_0"},
                             {"no_match", "aaa", "aaa"},
                         }),
                         [](const ::testing::TestParamInfo<GetUnusedVarNameTestCase>& info) {
                           return info.param.name;
                         });

struct PxDisplayParserTestCase {
  std::string name;
  std::string pxl;
  std::string error;
  std::vector<DisplayLine> expected;
};
class PxDisplayParserTest : public OTelGeneratorTest,
                            public ::testing::WithParamInterface<PxDisplayParserTestCase> {};

TEST_P(PxDisplayParserTest, GetPxDisplayLines) {
  if (GetParam().error != "") {
    EXPECT_COMPILER_ERROR(OTelGenerator::GetPxDisplayLines(GetParam().pxl).status(),
                          GetParam().error);
    return;
  }
  ASSERT_OK_AND_ASSIGN(auto display_lines, OTelGenerator::GetPxDisplayLines(GetParam().pxl));
  ASSERT_EQ(display_lines.size(), GetParam().expected.size());

  for (size_t i = 0; i < display_lines.size(); ++i) {
    // Compare line, table_name, table_argument, line_number_start, line_number_end.
    EXPECT_EQ(display_lines[i].line, GetParam().expected[i].line);
    EXPECT_EQ(display_lines[i].table_name, GetParam().expected[i].table_name);
    EXPECT_EQ(display_lines[i].table_argument, GetParam().expected[i].table_argument);
    EXPECT_EQ(display_lines[i].line_number_start, GetParam().expected[i].line_number_start);
    EXPECT_EQ(display_lines[i].line_number_end, GetParam().expected[i].line_number_end);
  }
}
INSTANTIATE_TEST_SUITE_P(
    PxDisplayParserTestSuite, PxDisplayParserTest,
    ::testing::ValuesIn(std::vector<PxDisplayParserTestCase>{
        {"simple",
         R"pxl(
import px
ndf = px.DataFrame('http_events', start_time='-5m')
px.display(ndf[['time_', 'service', 'latency_ns']], "http_graph"))pxl",
         "",
         {
             {
                 R"pxl(px.display(ndf[['time_', 'service', 'latency_ns']], "http_graph"))pxl",
                 "http_graph",
                 "ndf[['time_', 'service', 'latency_ns']]",
                 3,
                 3,
             },
         }},
        {"multi_line_call",
         R"pxl(
import px
ndf = px.DataFrame('http_events', start_time='-5m')
px.display(df.groupby(['time_', 'service', 'latency_ns']).agg(
    {'latency_ns': 'mean'},
), "http_graph"))pxl",
         "",
         {
             {
                 R"pxl(px.display(df.groupby(['time_', 'service', 'latency_ns']).agg(
    {'latency_ns': 'mean'},
), "http_graph"))pxl",
                 "http_graph",
                 R"pxl(df.groupby(['time_', 'service', 'latency_ns']).agg({'latency_ns': 'mean'}))pxl",
                 3,
                 5,
             },
         }},
        {"many_display_call",
         R"pxl(
import px
df = px.DataFrame('http_events', start_time='-5m')
px.display(df[['time_', 'service', 'latency_ns']], "http_latency")
px.display(df[['time_', 'service', 'num_errors']], "http_num_errors")
px.display(df.head(1), "limited"))pxl",
         "",
         {
             {
                 R"pxl(px.display(df[['time_', 'service', 'latency_ns']], "http_latency"))pxl",
                 "http_latency",
                 "df[['time_', 'service', 'latency_ns']]",
                 3,
                 3,
             },
             {
                 R"pxl(px.display(df[['time_', 'service', 'num_errors']], "http_num_errors"))pxl",
                 "http_num_errors",
                 "df[['time_', 'service', 'num_errors']]",
                 4,
                 4,
             },
             {
                 R"pxl(px.display(df.head(1), "limited"))pxl",
                 "limited",
                 "df.head(1)",
                 5,
                 5,
             },
         }},
        {"wrong_table_type",
         R"pxl(
import px
ndf = px.DataFrame('http_events', start_time='-5m')
px.display(ndf[['time_', 'service', 'latency_ns']], "a" + "b"))pxl",
         "expected second argument to px.display to be a string, received a",
         {}},
        {"missing_table_name",
         R"pxl(
import px
ndf = px.DataFrame('http_events', start_time='-5m')
px.display(ndf[['time_', 'service', 'latency_ns']]))pxl",
         "expected two arguments to px.display",
         {}},
    }),
    [](const ::testing::TestParamInfo<PxDisplayParserTestCase>& info) { return info.param.name; });

struct ComputeOutputSchemasTestCase {
  std::string name;
  std::string pxl;
  absl::flat_hash_map<std::string, std::string> expected_name_to_output_schema;
};
class ComputeOutputSchemasTest
    : public OTelGeneratorTest,
      public ::testing::WithParamInterface<ComputeOutputSchemasTestCase> {};

TEST_P(ComputeOutputSchemasTest, test_schemas) {
  compiler::Compiler compiler_;
  auto state = testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema);
  ASSERT_OK_AND_ASSIGN(auto compiler_state, CreateCompilerState(state, registry_info_.get(),
                                                                /* max_output_rows_per_table*/ 0));

  ASSERT_OK_AND_ASSIGN(auto resp, OTelGenerator::CalculateOutputSchemas(
                                      &compiler_, compiler_state.get(), GetParam().pxl));
  for (const auto& [name, expected_schema] : GetParam().expected_name_to_output_schema) {
    EXPECT_THAT(resp, ::testing::Contains(::testing::Pair(name, EqualsProto(expected_schema))));
  }
  EXPECT_EQ(resp.size(), GetParam().expected_name_to_output_schema.size());
}
INSTANTIATE_TEST_SUITE_P(ComputeOutputSchemasTestSuite, ComputeOutputSchemasTest,
                         ::testing::ValuesIn(std::vector<ComputeOutputSchemasTestCase>{
                             {
                                 "two_tables",
                                 R"pxl(import px
df = px.DataFrame(table='http_events', start_time='-6m')
df.service = df.ctx['service']
px.display(df[['service', 'resp_latency_ns']], 'latencies')
px.display(df[['service', 'req_body']], 'req_body'))pxl",
                                 {{"latencies", R"proto(
      columns {
        column_name: "service"
        column_type: STRING
        column_semantic_type: ST_SERVICE_NAME
      }
      columns {
        column_name: "resp_latency_ns"
        column_type: INT64
        column_semantic_type: ST_DURATION_NS
      })proto"},
                                  {"req_body", R"proto(
      columns {
        column_name: "service"
        column_type: STRING
        column_semantic_type: ST_SERVICE_NAME
      }
      columns {
        column_name: "req_body"
        column_type: STRING
        column_semantic_type: ST_NONE
      })proto"}},
                             },
                             {
                                 "default_output_name",
                                 R"pxl(import px
df = px.DataFrame(table='http_events', start_time='-6m')
df.service = df.ctx['service']
px.display(df[['service', 'req_body']]))pxl",
                                 {{"output", R"proto(
      columns {
        column_name: "service"
        column_type: STRING
        column_semantic_type: ST_SERVICE_NAME
      }
      columns {
        column_name: "req_body"
        column_type: STRING
        column_semantic_type: ST_NONE
      })proto"}},
                             },
                         }),
                         [](const ::testing::TestParamInfo<ComputeOutputSchemasTestCase>& info) {
                           return info.param.name;
                         });

struct GenerateOTelScriptTestCase {
  std::string name;
  std::string input_script;
  std::string expected_script;
  std::string error;
};
class GenerateOTelScriptTest : public OTelGeneratorTest,
                               public ::testing::WithParamInterface<GenerateOTelScriptTestCase> {};
TEST_P(GenerateOTelScriptTest, GenerateOTelScript) {
  compiler::Compiler compiler_;
  auto state = testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema);
  ASSERT_OK_AND_ASSIGN(auto compiler_state, CreateCompilerState(state, registry_info_.get(),
                                                                /* max_output_rows_per_table*/ 0));

  if (GetParam().error != "") {
    EXPECT_THAT(
        OTelGenerator::GenerateOTelScript(&compiler_, compiler_state.get(), GetParam().input_script)
            .status()
            .ToString(),
        ::testing::MatchesRegex(".*?" + GetParam().error + ".*?"));
  } else {
    ASSERT_OK_AND_ASSIGN(std::string resp,
                         OTelGenerator::GenerateOTelScript(&compiler_, compiler_state.get(),
                                                           GetParam().input_script));
    EXPECT_EQ(resp, GetParam().expected_script);
  }
}

// The test suite for GenerateOTelScriptTest.
INSTANTIATE_TEST_SUITE_P(
    GenerateOTelScriptTestSuite, GenerateOTelScriptTest,
    ::testing::ValuesIn(std::vector<GenerateOTelScriptTestCase>{
        {"multi_metric",
         R"pxl(import px
df = px.DataFrame('http_events', start_time='-5m')
df.service = df.ctx['service']
df.is_error = df.resp_status >= 400
df = df.groupby(['time_', 'service']).agg(
  resp_latency_ns=('resp_latency_ns', px.mean),
  num_errors=('is_error', px.sum),
)
px.display(df[['time_', 'service', 'resp_latency_ns', 'num_errors']], 'http_graph'))pxl",
         R"otel(import px
df = px.DataFrame('http_events', start_time=px.plugin.start_time, end_time=px.plugin.end_time)
df.service = df.ctx['service']
df.is_error = df.resp_status >= 400
df = df.groupby(['time_', 'service']).agg(
  resp_latency_ns=('resp_latency_ns', px.mean),
  num_errors=('is_error', px.sum),
)
px.display(df[['time_', 'service', 'resp_latency_ns', 'num_errors']], 'http_graph')

otel_df = df[['time_', 'service', 'resp_latency_ns', 'num_errors']]
px.export(otel_df, px.otel.Data(
  resource={
    'http_graph.service': otel_df.service,
    'service.name': otel_df.service
  },
  data=[
    px.otel.metric.Gauge(
      name='http_graph.resp_latency_ns',
      description='',
      value=otel_df.resp_latency_ns,
    ),
    px.otel.metric.Gauge(
      name='http_graph.num_errors',
      description='',
      value=otel_df.num_errors,
    )
  ]
)))otel",
         ""},
        {"assigns_to_unique_varname",
         R"pxl(import px
otel_df = 'placeholder'
df = px.DataFrame('http_events', start_time='-5m')
df.service = df.ctx['service']
px.display(df[['time_', 'service', 'resp_latency_ns']], 'http_graph'))pxl",
         R"otel(import px
otel_df = 'placeholder'
df = px.DataFrame('http_events', start_time=px.plugin.start_time, end_time=px.plugin.end_time)
df.service = df.ctx['service']
px.display(df[['time_', 'service', 'resp_latency_ns']], 'http_graph')

otel_df_0 = df[['time_', 'service', 'resp_latency_ns']]
px.export(otel_df_0, px.otel.Data(
  resource={
    'http_graph.service': otel_df_0.service,
    'service.name': otel_df_0.service
  },
  data=[
    px.otel.metric.Gauge(
      name='http_graph.resp_latency_ns',
      description='',
      value=otel_df_0.resp_latency_ns,
    )
  ]
)))otel",
         ""},
        {"multiple_display_calls",
         R"pxl(import px
df = px.DataFrame('http_events', start_time='-5m')
df.service = df.ctx['service']
df.is_error = df.resp_status >= 400
df = df.groupby(['time_', 'service']).agg(
  resp_latency_ns=('resp_latency_ns', px.mean),
  num_errors=('is_error', px.sum),
)
px.display(df[['time_', 'service', 'resp_latency_ns']], "http_latency")
px.display(df[['time_', 'service', 'num_errors']], "http_num_errors"))pxl",
         R"otel(import px
df = px.DataFrame('http_events', start_time=px.plugin.start_time, end_time=px.plugin.end_time)
df.service = df.ctx['service']
df.is_error = df.resp_status >= 400
df = df.groupby(['time_', 'service']).agg(
  resp_latency_ns=('resp_latency_ns', px.mean),
  num_errors=('is_error', px.sum),
)
px.display(df[['time_', 'service', 'resp_latency_ns']], "http_latency")

otel_df = df[['time_', 'service', 'resp_latency_ns']]
px.export(otel_df, px.otel.Data(
  resource={
    'http_latency.service': otel_df.service,
    'service.name': otel_df.service
  },
  data=[
    px.otel.metric.Gauge(
      name='http_latency.resp_latency_ns',
      description='',
      value=otel_df.resp_latency_ns,
    )
  ]
))

px.display(df[['time_', 'service', 'num_errors']], "http_num_errors")

otel_df = df[['time_', 'service', 'num_errors']]
px.export(otel_df, px.otel.Data(
  resource={
    'http_num_errors.service': otel_df.service,
    'service.name': otel_df.service
  },
  data=[
    px.otel.metric.Gauge(
      name='http_num_errors.num_errors',
      description='',
      value=otel_df.num_errors,
    )
  ]
)))otel",
         ""},
        {"always_creates_alias_to_otel_df",
         R"pxl(import px
df = px.DataFrame('http_events', start_time='-5m')
df.service = df.ctx['service']
df = df[['time_', 'service', 'resp_latency_ns']]
px.display(df, 'http_graph'))pxl",
         R"otel(import px
df = px.DataFrame('http_events', start_time=px.plugin.start_time, end_time=px.plugin.end_time)
df.service = df.ctx['service']
df = df[['time_', 'service', 'resp_latency_ns']]
px.display(df, 'http_graph')

otel_df = df
px.export(otel_df, px.otel.Data(
  resource={
    'http_graph.service': otel_df.service,
    'service.name': otel_df.service
  },
  data=[
    px.otel.metric.Gauge(
      name='http_graph.resp_latency_ns',
      description='',
      value=otel_df.resp_latency_ns,
    )
  ]
)))otel",
         ""},

        {"preserve_the_remaining_lines",
         R"pxl(import px
df = px.DataFrame('http_events', start_time='-5m')
df.service = df.ctx['service']
df = df[['time_', 'service', 'resp_latency_ns']]
px.display(df, 'http_graph')
px.export(df, px.otel.Data(
  endpoint=px.otel.Endpoint(
    url='http://otel-collector:4317',
  ),
  resource={
    'service.name': df.service
  },
  data=[
    px.otel.metric.Gauge(
      name='my_other_export_is_also_preserved',
      description='',
      value=df.resp_latency_ns,
    )
  ]
)))pxl",
         R"otel(import px
df = px.DataFrame('http_events', start_time=px.plugin.start_time, end_time=px.plugin.end_time)
df.service = df.ctx['service']
df = df[['time_', 'service', 'resp_latency_ns']]
px.display(df, 'http_graph')

otel_df = df
px.export(otel_df, px.otel.Data(
  resource={
    'http_graph.service': otel_df.service,
    'service.name': otel_df.service
  },
  data=[
    px.otel.metric.Gauge(
      name='http_graph.resp_latency_ns',
      description='',
      value=otel_df.resp_latency_ns,
    )
  ]
))

px.export(df, px.otel.Data(
  endpoint=px.otel.Endpoint(
    url='http://otel-collector:4317',
  ),
  resource={
    'service.name': df.service
  },
  data=[
    px.otel.metric.Gauge(
      name='my_other_export_is_also_preserved',
      description='',
      value=df.resp_latency_ns,
    )
  ]
)))otel",
         ""},
        {"multi_line_df_argument_is_handled",
         R"pxl(import px
df = px.DataFrame('http_events', start_time='-5m')
df.service = df.ctx['service']
px.display(df.groupby(['time_', 'service']).agg(
  resp_latency_ns=('resp_latency_ns', px.mean),
), 'http_graph'))pxl",
         R"otel(import px
df = px.DataFrame('http_events', start_time=px.plugin.start_time, end_time=px.plugin.end_time)
df.service = df.ctx['service']
px.display(df.groupby(['time_', 'service']).agg(
  resp_latency_ns=('resp_latency_ns', px.mean),
), 'http_graph')

otel_df = df.groupby(['time_', 'service']).agg(resp_latency_ns=('resp_latency_ns', px.mean))
px.export(otel_df, px.otel.Data(
  resource={
    'http_graph.service': otel_df.service,
    'service.name': otel_df.service
  },
  data=[
    px.otel.metric.Gauge(
      name='http_graph.resp_latency_ns',
      description='',
      value=otel_df.resp_latency_ns,
    )
  ]
)))otel",
         ""},
        {
            "missing_time_column",
            R"pxl(import px
df = px.DataFrame('http_events', start_time='-5m')
df.service = df.ctx['service']
px.display(df[['service', 'resp_latency_ns']], 'http_graph'))pxl",
            "",
            "does not have a time_ column",
        },
        {
            "missing_data_column",
            R"pxl(import px
df = px.DataFrame('http_events', start_time='-5m')
df.service = df.ctx['service']
px.display(df[['time_', 'service']], 'http_graph'))pxl",
            "",
            "does not have any INT64 or FLOAT64 that can be converted to OTel metrics",
        },
        {
            "missing_service_column",
            R"pxl(import px
ndf = px.DataFrame('http_events', start_time='-5m')
px.display(ndf[['time_', 'resp_latency_ns']], "http_graph"))pxl",
            "",
            "does not have a service column",
        },
        {
            "duration_quantiles_not_supported",
            R"pxl(import px
df = px.DataFrame('http_events', start_time='-5m')
df.service = df.ctx['service']
df = df.groupby(['time_', 'service']).agg(latency=('resp_latency_ns', px.quantiles))
px.display(df, "http_graph"))pxl",
            "",
            "quantiles are not supported yet for generation of OTel export scripts",
        },
        {
            "normal_quantiles_not_supported",
            R"pxl(import px
df = px.DataFrame('http_events', start_time='-5m')
df.service = df.ctx['service']
df = df.groupby(['time_', 'service']).agg(resp_status=('resp_status', px.quantiles))
px.display(df, "http_graph"))pxl",
            "",
            "quantiles are not supported yet for generation of OTel export scripts",
        },
        {
            "no_returned_tables",
            R"pxl(import px
df = px.DataFrame('http_events', start_time='-5m')
df.service = df.ctx['service']
otel_df = df[['time_', 'service', 'resp_latency_ns']]
px.export(otel_df, px.otel.Data(
  endpoint=px.otel.Endpoint(
    url='http://otel-collector:4317',
  ),
  resource={
    'http_graph.service': otel_df.service,
    'service.name': otel_df.service
  },
  data=[
    px.otel.metric.Gauge(
      name='http_graph.resp_latency_ns',
      description='',
      value=otel_df.resp_latency_ns,
    )
  ]
)))pxl",
            "",
            "script does not have any output tables",
        },
        {
            "duplicate_table_name",
            R"pxl(
import px
df = px.DataFrame('http_events', start_time='-5m')
df.service = df.ctx['service']
px.display(df[['time_', 'service', 'resp_latency_ns']], "table")
px.display(df[['time_', 'service', 'resp_status']], "table"))pxl",
            "",
            "duplicate table name. 'table' already in use",
        },
    }),
    [](const ::testing::TestParamInfo<GenerateOTelScriptTestCase>& info) {
      return info.param.name;
    });

struct ReplacePluginTimeTestCase {
  std::string name;
  std::string pxl;
  std::vector<DataFrameCall> expected;
};
class ReplacePluginTimeTest : public OTelGeneratorTest,
                              public ::testing::WithParamInterface<ReplacePluginTimeTestCase> {};

TEST_P(ReplacePluginTimeTest, replace_plugin_time) {
  ASSERT_OK_AND_ASSIGN(auto dataframe_calls, OTelGenerator::ReplaceDataFrameTimes(GetParam().pxl));
  ASSERT_EQ(dataframe_calls.size(), GetParam().expected.size());

  for (size_t i = 0; i < dataframe_calls.size(); ++i) {
    EXPECT_EQ(dataframe_calls[i].original_call, GetParam().expected[i].original_call);
    EXPECT_EQ(dataframe_calls[i].updated_line, GetParam().expected[i].updated_line);
    EXPECT_EQ(dataframe_calls[i].line_number_start, GetParam().expected[i].line_number_start);
    EXPECT_EQ(dataframe_calls[i].line_number_end, GetParam().expected[i].line_number_end);
  }
}
INSTANTIATE_TEST_SUITE_P(
    ReplacePluginTimeTestSuite, ReplacePluginTimeTest,
    ::testing::ValuesIn(std::vector<ReplacePluginTimeTestCase>{
        {"simple",
         R"pxl(
import px
df = px.DataFrame('http_events', start_time='-5m', end_time='-1m')
px.display(df, 'http_graph'))pxl",
         {
             {
                 "df = px.DataFrame('http_events', start_time='-5m', end_time='-1m')",
                 R"pxl(df = px.DataFrame('http_events', start_time=px.plugin.start_time, end_time=px.plugin.end_time))pxl",
                 2,
                 2,
             },
         }},
        {"multi_line_call",
         R"pxl(
import px
df = px.DataFrame('http_events',
  start_time='-5m',
  end_time='-1m')
px.display(df, 'http_graph'))pxl",
         {
             {
                 R"pxl(df = px.DataFrame('http_events',
  start_time='-5m',
  end_time='-1m'))pxl",
                 R"pxl(df = px.DataFrame('http_events', start_time=px.plugin.start_time, end_time=px.plugin.end_time))pxl",
                 2,
                 4,
             },
         }},
        {"in_function_body",
         R"pxl(
import px
def foo(start_time):
  df = px.DataFrame('http_events', start_time=start_time, end_time='-1m')
  return df
px.display(foo('-5m'), 'http_graph'))pxl",
         {
             {
                 R"pxl(  df = px.DataFrame('http_events', start_time=start_time, end_time='-1m'))pxl",
                 R"pxl(  df = px.DataFrame('http_events', start_time=px.plugin.start_time, end_time=px.plugin.end_time))pxl",
                 3,
                 3,
             },
         }},
        {"in_function_return",
         R"pxl(
import px
def foo(start_time):
  return px.DataFrame('http_events', start_time=start_time, end_time='-1m')
px.display(foo('-5m'), 'http_graph'))pxl",
         {
             {
                 R"pxl(  return px.DataFrame('http_events', start_time=start_time, end_time='-1m'))pxl",
                 R"pxl(  return px.DataFrame('http_events', start_time=px.plugin.start_time, end_time=px.plugin.end_time))pxl",
                 3,
                 3,
             },
         }},
        {"multi_function",
         R"pxl(
import px
def foo(start_time):
  df = px.DataFrame('http_events', start_time=start_time, end_time='-1m')
  return px.DataFrame('http_events', start_time=start_time, end_time='-1m')
px.display(foo('-5m'), 'http_graph')
df)pxl",
         {
             {
                 R"pxl(  df = px.DataFrame('http_events', start_time=start_time, end_time='-1m'))pxl",
                 R"pxl(  df = px.DataFrame('http_events', start_time=px.plugin.start_time, end_time=px.plugin.end_time))pxl",
                 3,
                 3,
             },
             {
                 R"pxl(  return px.DataFrame('http_events', start_time=start_time, end_time='-1m'))pxl",
                 R"pxl(  return px.DataFrame('http_events', start_time=px.plugin.start_time, end_time=px.plugin.end_time))pxl",
                 4,
                 4,
             },
         }},
        {"multiple_calls",
         R"pxl(
import px
df = px.DataFrame('http_events', start_time='-5m')
px.display(df, 'http_graph')
df = px.DataFrame('process_stats', start_time='-5m')
px.display(df, 'http_graph'))pxl",
         {
             {
                 "df = px.DataFrame('http_events', start_time='-5m')",
                 R"pxl(df = px.DataFrame('http_events', start_time=px.plugin.start_time, end_time=px.plugin.end_time))pxl",
                 2,
                 2,
             },
             {
                 "df = px.DataFrame('process_stats', start_time='-5m')",
                 R"pxl(df = px.DataFrame('process_stats', start_time=px.plugin.start_time, end_time=px.plugin.end_time))pxl",
                 4,
                 4,
             },
         }},
        {"default_end_time_argument",
         R"pxl(
import px
df = px.DataFrame('http_events', start_time='-5m')
px.display(df, 'http_graph'))pxl",
         {
             {
                 "df = px.DataFrame('http_events', start_time='-5m')",
                 R"pxl(df = px.DataFrame('http_events', start_time=px.plugin.start_time, end_time=px.plugin.end_time))pxl",
                 2,
                 2,
             },
         }},
        {"all_optional_arguments_not_specified",
         R"pxl(
import px
df = px.DataFrame('http_events')
px.display(df, 'http_graph'))pxl",
         {
             {
                 "df = px.DataFrame('http_events')",
                 R"pxl(df = px.DataFrame('http_events', start_time=px.plugin.start_time, end_time=px.plugin.end_time))pxl",
                 2,
                 2,
             },
         }},
        {"all_positional_arguments",
         R"pxl(
import px
df = px.DataFrame('http_events', ['time_', 'upid', 'resp_body'], '-5m', '-1m')
px.display(df, 'http_graph'))pxl",
         {
             {
                 "df = px.DataFrame('http_events', ['time_', 'upid', 'resp_body'], '-5m', '-1m')",
                 R"pxl(df = px.DataFrame('http_events', select=['time_', 'upid', 'resp_body'], start_time=px.plugin.start_time, end_time=px.plugin.end_time))pxl",
                 2,
                 2,
             },
         }},
        {"order_kwargs_differently",
         R"pxl(
import px
df = px.DataFrame('http_events', end_time='-1m', start_time='-5m', select=['time_', 'upid', 'resp_body'])
px.display(df, 'http_graph'))pxl",
         {
             {
                 R"pxl(df = px.DataFrame('http_events', end_time='-1m', start_time='-5m', select=['time_', 'upid', 'resp_body']))pxl",
                 R"pxl(df = px.DataFrame('http_events', select=['time_', 'upid', 'resp_body'], start_time=px.plugin.start_time, end_time=px.plugin.end_time))pxl",
                 2,
                 2,
             },
         }},

        {"attribute_call_on_df",
         R"pxl(
import px
df = px.DataFrame('http_events', start_time='-5m', end_time='-1m').groupby(['upid']).agg(
  latency=('latency_ns', px.mean),
)
px.display(df, 'http_graph'))pxl",
         {
             {
                 R"pxl(df = px.DataFrame('http_events', start_time='-5m', end_time='-1m').groupby(['upid']).agg(
  latency=('latency_ns', px.mean),
))pxl",
                 R"pxl(df = px.DataFrame('http_events', start_time=px.plugin.start_time, end_time=px.plugin.end_time).groupby(['upid']).agg(latency=('latency_ns', px.mean)))pxl",
                 2,
                 4,
             },
         }},
        {"subscript_call_on_df",
         R"pxl(
import px
df = px.DataFrame('http_events', start_time='-5m', end_time='-1m')[['time_', 'upid', 'resp_body']]
px.display(df, 'http_graph'))pxl",
         {
             {
                 R"pxl(df = px.DataFrame('http_events', start_time='-5m', end_time='-1m')[['time_', 'upid', 'resp_body']])pxl",
                 R"pxl(df = px.DataFrame('http_events', start_time=px.plugin.start_time, end_time=px.plugin.end_time)[['time_', 'upid', 'resp_body']])pxl",
                 2,
                 2,
             },
         }},

        {"dataframe_as_function_argument",
         R"pxl(
import px
px.display(px.DataFrame('http_events', start_time='-5m', end_time='-1m'), 'http_graph'))pxl",
         {
             {
                 R"pxl(px.display(px.DataFrame('http_events', start_time='-5m', end_time='-1m'), 'http_graph'))pxl",
                 R"pxl(px.display(px.DataFrame('http_events', start_time=px.plugin.start_time, end_time=px.plugin.end_time), 'http_graph'))pxl",
                 2,
                 2,
             },
         }},
    }),
    [](const ::testing::TestParamInfo<ReplacePluginTimeTestCase>& info) {
      return info.param.name;
    });
}  // namespace planner
}  // namespace carnot
}  // namespace px
