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

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/splitter/partial_op_mgr/partial_op_mgr.h"
#include "src/carnot/planner/rules/rules.h"
#include "src/carnot/udf_exporter/udf_exporter.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

class PartialOpMgrTest : public OperatorTests {};

TEST_F(PartialOpMgrTest, limit_test) {
  auto mem_src = MakeMemSource(MakeRelation());
  auto limit = MakeLimit(mem_src, 10);
  MakeMemSink(limit, "out");

  LimitOperatorMgr mgr;
  EXPECT_TRUE(mgr.Matches(limit));
  auto prepare_limit_or_s = mgr.CreatePrepareOperator(graph.get(), limit);
  ASSERT_OK(prepare_limit_or_s);
  OperatorIR* prepare_limit_uncasted = prepare_limit_or_s.ConsumeValueOrDie();
  ASSERT_MATCH(prepare_limit_uncasted, Limit());
  LimitIR* prepare_limit = static_cast<LimitIR*>(prepare_limit_uncasted);
  EXPECT_EQ(prepare_limit->limit_value(), limit->limit_value());
  EXPECT_EQ(prepare_limit->parents(), limit->parents());
  EXPECT_NE(prepare_limit, limit);

  auto mem_src2 = MakeMemSource(MakeRelation());
  auto merge_limit_or_s = mgr.CreateMergeOperator(graph.get(), mem_src2, limit);
  ASSERT_OK(merge_limit_or_s);
  OperatorIR* merge_limit_uncasted = merge_limit_or_s.ConsumeValueOrDie();
  ASSERT_MATCH(merge_limit_uncasted, Limit());
  LimitIR* merge_limit = static_cast<LimitIR*>(merge_limit_uncasted);
  EXPECT_EQ(merge_limit->limit_value(), limit->limit_value());
  EXPECT_EQ(merge_limit->parents()[0], mem_src2);
  EXPECT_NE(merge_limit, limit);
}

TEST_F(PartialOpMgrTest, agg_test) {
  auto relation = MakeRelation();
  relation.AddColumn(types::STRING, "service");
  auto mem_src = MakeMemSource(relation);
  auto count_col = MakeColumn("count", 0);
  count_col->ResolveColumnType(types::INT64);
  auto service_col = MakeColumn("service", 0);
  service_col->ResolveColumnType(types::STRING);
  auto mean_func = MakeMeanFunc(MakeColumn("count", 0));
  mean_func->SetSupportsPartial(true);
  auto agg = MakeBlockingAgg(mem_src, {count_col, service_col}, {{"mean", mean_func}});
  Relation agg_relation({types::INT64, types::STRING, types::FLOAT64},
                        {"count", "service", "mean"});
  ASSERT_OK(agg->SetRelation(agg_relation));
  MakeMemSink(agg, "out");

  // Pre-checks to make sure things work.
  EXPECT_MATCH(agg, FullAgg());
  EXPECT_NOT_MATCH(agg, FinalizeAgg());
  EXPECT_NOT_MATCH(agg, PartialAgg());

  AggOperatorMgr mgr;
  EXPECT_TRUE(mgr.Matches(agg));
  auto prepare_agg_or_s = mgr.CreatePrepareOperator(graph.get(), agg);
  ASSERT_OK(prepare_agg_or_s);

  OperatorIR* prepare_agg_uncasted = prepare_agg_or_s.ConsumeValueOrDie();
  ASSERT_MATCH(prepare_agg_uncasted, PartialAgg());
  BlockingAggIR* prepare_agg = static_cast<BlockingAggIR*>(prepare_agg_uncasted);

  auto mem_src2 = MakeMemSource(MakeRelation());
  auto merge_agg_or_s = mgr.CreateMergeOperator(graph.get(), mem_src2, agg);
  ASSERT_OK(merge_agg_or_s);
  OperatorIR* merge_agg_uncasted = merge_agg_or_s.ConsumeValueOrDie();
  ASSERT_MATCH(merge_agg_uncasted, FinalizeAgg());
  BlockingAggIR* merge_agg = static_cast<BlockingAggIR*>(merge_agg_uncasted);

  ASSERT_EQ(prepare_agg->aggregate_expressions().size(), merge_agg->aggregate_expressions().size());
  for (int64_t i = 0; i < static_cast<int64_t>(prepare_agg->aggregate_expressions().size()); ++i) {
    auto prep_expr = prepare_agg->aggregate_expressions()[i];
    auto merge_expr = merge_agg->aggregate_expressions()[i];
    EXPECT_EQ(prep_expr.name, merge_expr.name);
    EXPECT_TRUE(prep_expr.node->Equals(merge_expr.node))
        << absl::Substitute("prep expr $0 merge expr $1", prep_expr.node->DebugString(),
                            merge_expr.node->DebugString());
  }
  // Confirm that the relations are good.
  EXPECT_EQ(prepare_agg->relation(), Relation({types::INT64, types::STRING, types::STRING},
                                              {"count", "service", "serialized_expressions"}));

  EXPECT_EQ(merge_agg->relation(), agg_relation);
}

// This tests aggs with functions that can't partial. We don't partial the agg if that's the case.
TEST_F(PartialOpMgrTest, agg_where_fn_cant_partial) {
  auto mem_src = MakeMemSource(MakeRelation());
  auto count_col = MakeColumn("count", 0);
  count_col->ResolveColumnType(types::INT64);
  auto service_col = MakeColumn("service", 0);
  service_col->ResolveColumnType(types::STRING);
  // One function is partial
  auto mean_func = MakeMeanFunc(MakeColumn("count", 0));
  mean_func->SetSupportsPartial(false);
  // Even though one is partial, the entire agg cannot be converted.
  auto mean_func2 = MakeMeanFunc(MakeColumn("count", 0));
  mean_func2->SetSupportsPartial(true);
  auto agg = MakeBlockingAgg(mem_src, {count_col, service_col},
                             {{"mean", mean_func}, {"mean2", mean_func2}});
  Relation agg_relation({types::INT64, types::STRING, types::FLOAT64, types::FLOAT64},
                        {"count", "service", "mean", "mean2"});
  ASSERT_OK(agg->SetRelation(agg_relation));
  MakeMemSink(agg, "out");

  // Pre-checks to make sure things work.
  EXPECT_MATCH(agg, FullAgg());
  EXPECT_NOT_MATCH(agg, FinalizeAgg());
  EXPECT_NOT_MATCH(agg, PartialAgg());

  AggOperatorMgr mgr;
  EXPECT_FALSE(mgr.Matches(agg));
}
}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
