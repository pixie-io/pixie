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

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "src/carnot/planner/compiler/analyzer/merge_group_by_into_group_acceptor_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using table_store::schema::Relation;
using ::testing::ElementsAre;

TEST_F(RulesTest, MergeGroupByAggRule) {
  MemorySourceIR* mem_source = MakeMemSource();
  GroupByIR* group_by = MakeGroupBy(mem_source, {MakeColumn("col1", 0), MakeColumn("col2", 0)});
  BlockingAggIR* agg =
      MakeBlockingAgg(group_by, {}, {{"outcount", MakeMeanFunc(MakeColumn("count", 0))}});
  MakeMemSink(agg, "");

  EXPECT_THAT(agg->parents(), ElementsAre(group_by));
  EXPECT_EQ(agg->groups().size(), 0);
  std::vector<int64_t> groupby_ids;
  for (ColumnIR* g : group_by->groups()) {
    groupby_ids.push_back(g->id());
  }

  // Do match and merge Groupby with agg
  // make sure agg parent changes from groupby to the parent of the groupby
  MergeGroupByIntoGroupAcceptorRule rule(IRNodeType::kBlockingAgg);
  auto result = rule.Execute(graph.get());
  EXPECT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  EXPECT_THAT(agg->parents(), ElementsAre(mem_source));

  std::vector<std::string> actual_group_names;
  std::vector<int64_t> actual_group_ids;
  for (ColumnIR* g : agg->groups()) {
    actual_group_names.push_back(g->col_name());
    actual_group_ids.push_back(g->id());
  }

  EXPECT_THAT(actual_group_names, ElementsAre("col1", "col2"));
  EXPECT_NE(actual_group_ids, groupby_ids);
}

TEST_F(RulesTest, MergeGroupByRollingRule) {
  MemorySourceIR* mem_source = MakeMemSource();
  GroupByIR* group_by = MakeGroupBy(mem_source, {MakeColumn("col1", 0), MakeColumn("col2", 0)});
  RollingIR* rolling = MakeRolling(group_by, MakeColumn("time_", 0), 1);
  MakeMemSink(rolling, "");

  EXPECT_THAT(rolling->parents(), ElementsAre(group_by));
  EXPECT_EQ(rolling->groups().size(), 0);
  std::vector<int64_t> groupby_ids;
  for (ColumnIR* g : group_by->groups()) {
    groupby_ids.push_back(g->id());
  }

  // Do match and merge Groupby with rolling
  // make sure rolling parent changes from groupby to the parent of the groupby
  MergeGroupByIntoGroupAcceptorRule rule(IRNodeType::kRolling);
  auto result = rule.Execute(graph.get());
  EXPECT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  EXPECT_THAT(rolling->parents(), ElementsAre(mem_source));

  std::vector<std::string> actual_group_names;
  std::vector<int64_t> actual_group_ids;
  for (ColumnIR* g : rolling->groups()) {
    actual_group_names.push_back(g->col_name());
    actual_group_ids.push_back(g->id());
  }

  EXPECT_THAT(actual_group_names, ElementsAre("col1", "col2"));
  EXPECT_NE(actual_group_ids, groupby_ids);
}

TEST_F(RulesTest, MergeGroupByAggRule_MultipleAggsOneGroupBy) {
  MemorySourceIR* mem_source = MakeMemSource();
  GroupByIR* group_by = MakeGroupBy(mem_source, {MakeColumn("col1", 0), MakeColumn("col2", 0)});
  BlockingAggIR* agg1 =
      MakeBlockingAgg(group_by, {}, {{"outcount", MakeMeanFunc(MakeColumn("count", 0))}});
  MakeMemSink(agg1, "");
  BlockingAggIR* agg2 =
      MakeBlockingAgg(group_by, {}, {{"latency_mean", MakeMeanFunc(MakeColumn("latency", 0))}});
  MakeMemSink(agg2, "");

  EXPECT_EQ(graph->FindNodesThatMatch(BlockingAgg()).size(), 2);

  EXPECT_THAT(agg1->parents(), ElementsAre(group_by));
  EXPECT_EQ(agg1->groups().size(), 0);
  EXPECT_THAT(agg2->parents(), ElementsAre(group_by));
  EXPECT_EQ(agg2->groups().size(), 0);

  MergeGroupByIntoGroupAcceptorRule rule(IRNodeType::kBlockingAgg);
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  EXPECT_THAT(agg1->parents(), ElementsAre(mem_source));
  EXPECT_THAT(agg2->parents(), ElementsAre(mem_source));
  std::vector<std::string> group_names1;
  std::vector<std::string> group_names2;
  std::vector<int64_t> group_ids1;
  std::vector<int64_t> group_ids2;
  for (ColumnIR* g : agg1->groups()) {
    group_names1.push_back(g->col_name());
    group_ids1.push_back(g->id());
  }
  for (ColumnIR* g : agg2->groups()) {
    group_names2.push_back(g->col_name());
    group_ids2.push_back(g->id());
  }

  EXPECT_THAT(group_names1, ElementsAre("col1", "col2"));
  EXPECT_THAT(group_names2, ElementsAre("col1", "col2"));

  // Ids must be different -> must be a deep copy not a pointer copy.
  EXPECT_NE(group_ids1, group_ids2);
}

TEST_F(RulesTest, MergeGroupByAggRule_MissesSoleAgg) {
  MemorySourceIR* mem_source = MakeMemSource();
  BlockingAggIR* agg =
      MakeBlockingAgg(mem_source, {}, {{"outcount", MakeMeanFunc(MakeColumn("count", 0))}});
  MakeMemSink(agg, "");

  EXPECT_THAT(agg->parents(), ElementsAre(mem_source));
  EXPECT_EQ(agg->groups().size(), 0);

  // Don't match Agg by itself
  MergeGroupByIntoGroupAcceptorRule rule(IRNodeType::kBlockingAgg);
  auto result = rule.Execute(graph.get());
  EXPECT_OK(result);
  EXPECT_FALSE(result.ConsumeValueOrDie());

  // Agg parents don't change
  EXPECT_THAT(agg->parents(), ElementsAre(mem_source));
  // Agg groups should not change.
  EXPECT_EQ(agg->groups().size(), 0);
}

TEST_F(RulesTest, MergeGroupByAggRule_DoesNotTouchSoleGroupby) {
  MemorySourceIR* mem_source = MakeMemSource();
  GroupByIR* group_by = MakeGroupBy(mem_source, {MakeColumn("col1", 0), MakeColumn("col2", 0)});
  MakeMemSink(group_by, "");
  // Don't match groupby by itself
  MergeGroupByIntoGroupAcceptorRule rule(IRNodeType::kBlockingAgg);
  auto result = rule.Execute(graph.get());
  EXPECT_OK(result);
  // Should not do anything to the graph.
  EXPECT_FALSE(result.ConsumeValueOrDie());
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
