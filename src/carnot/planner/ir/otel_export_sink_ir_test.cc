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

TEST_F(OTelExportSinkTest, correct_init) {
  std::string otel_metric = R"pb(
    metric {
      name: "spans"
      attributes {
        name: "http.method"
        value_column: "req_method"
      }
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
      start_time_unix_nano_column: "time_"
      time_unix_nano_column: "end_time"
    })pb";

  planpb::OTelExportSinkOperator otelpb;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(otel_metric, &otelpb));

  std::vector<ExpectedColumn> expected_columns{
      {MakeString("req_method"), "attribute", "req_method", {types::STRING}},
      {MakeString("count"), "count", "count", {types::FLOAT64}},
      {MakeString("sum"), "sum", "sum", {types::FLOAT64}},
      {MakeString("p50"), "0.50", "p50", {types::FLOAT64}},
      {MakeString("p99"), "0.99", "p99", {types::FLOAT64}},
      {MakeString("time_"), "start_time_unix_nano", "time_", {types::TIME64NS}},
      {MakeString("end_time"), "time_unix_nano", "end_time", {types::TIME64NS}},
  };

  auto src = MakeMemSource("table");
  ASSERT_OK_AND_ASSIGN(OTelExportSinkIR * otel_sink,
                       graph->CreateNode<OTelExportSinkIR>(ast, src, otelpb, expected_columns));
  EXPECT_THAT(otel_sink->RequiredInputColumns().ConsumeValueOrDie()[0],
              ::testing::UnorderedElementsAre("req_method", "count", "sum", "p50", "p99", "time_",
                                              "end_time"));

  table_store::schema::Relation relation;
  for (const auto& c : expected_columns) {
    relation.AddColumn(*(c.coltypes.begin()), c.colname);
  }

  (*compiler_state_->relation_map())["table"] = std::move(relation);
  EXPECT_OK(src->ResolveType(compiler_state_.get()));
  otel_sink->PullParentTypes();
  EXPECT_OK(otel_sink->UpdateOpAfterParentTypesResolved());
}

struct TestCase {
  std::string name;
  absl::flat_hash_set<types::DataType> col_types;
  table_store::schema::Relation relation;
  std::string error_regex;
};
class OTelExportWrongArgsTest : public OTelExportSinkTest,
                                public ::testing::WithParamInterface<TestCase> {};
TEST_P(OTelExportWrongArgsTest, wrong_args) {
  auto tc = GetParam();
  std::vector<ExpectedColumn> expected_columns{
      {MakeString("count"), "count", "count", tc.col_types},
  };
  (*compiler_state_->relation_map())["table"] = tc.relation;

  auto src = MakeMemSource("table");
  OTelExportSinkIR* otel_sink =
      graph
          ->CreateNode<OTelExportSinkIR>(ast, src, planpb::OTelExportSinkOperator{},
                                         expected_columns)
          .ConsumeValueOrDie();
  EXPECT_OK(src->ResolveType(compiler_state_.get()));
  otel_sink->PullParentTypes();

  EXPECT_COMPILER_ERROR(otel_sink->UpdateOpAfterParentTypesResolved(), tc.error_regex);
}
INSTANTIATE_TEST_SUITE_P(
    ErrorTests, OTelExportWrongArgsTest,
    ::testing::ValuesIn(std::vector<TestCase>{
        {
            "missing_type_in_relation",
            {types::FLOAT64},
            table_store::schema::Relation{{types::STRING}, {"not_count"}},
            "Column 'count' not found.*",
        },
        {
            "wrong_type_single_option",
            {types::FLOAT64},
            table_store::schema::Relation{{types::STRING}, {"count"}},
            "Expected 'count' column to be a FLOAT64, 'count' is of type STRING",
        },
        {
            "wrong_type_multi_option",
            {types::FLOAT64, types::INT64},
            table_store::schema::Relation{{types::STRING}, {"count"}},
            "Expected 'count' column to be one of .*, 'count' is of type STRING",
        }}),
    [](const ::testing::TestParamInfo<TestCase>& info) { return info.param.name; });
}  // namespace planner
}  // namespace carnot
}  // namespace px
