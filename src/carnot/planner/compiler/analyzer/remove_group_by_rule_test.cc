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
#include "src/carnot/planner/compiler/analyzer/remove_group_by_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using ::testing::ElementsAre;
using ::testing::Not;
using ::testing::UnorderedElementsAreArray;

TEST_F(RulesTest, RemoveGroupByRule) {
  MemorySourceIR* mem_source = MakeMemSource();
  GroupByIR* group_by = MakeGroupBy(mem_source, {MakeColumn("col1", 0), MakeColumn("col2", 0)});
  int64_t group_by_node_id = group_by->id();
  // Note that the parent is mem_source not group by.
  BlockingAggIR* agg = MakeBlockingAgg(mem_source, {MakeColumn("col1", 0), MakeColumn("col2", 0)},
                                       {{"outcount", MakeMeanFunc(MakeColumn("count", 0))}});
  MakeMemSink(agg, "");
  // Do match groupbys() that no longer have children
  RemoveGroupByRule rule;
  auto result = rule.Execute(graph.get());
  EXPECT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  EXPECT_FALSE(graph->HasNode(group_by_node_id));
  // Make sure no groupby sticks around either
  for (int64_t i : graph->dag().TopologicalSort()) {
    EXPECT_FALSE(Match(graph->Get(i), GroupBy())) << absl::Substitute("Node $0 is a groupby()", i);
  }
}

TEST_F(RulesTest, RemoveGroupByRule_FailOnBadGroupBy) {
  // Error on groupbys() that have sinks or follow up nodes.
  MemorySourceIR* mem_source = MakeMemSource();
  GroupByIR* group_by = MakeGroupBy(mem_source, {MakeColumn("col1", 0), MakeColumn("col2", 0)});
  // Note that mem sink is conect to a groupby. Anything that has a group by as a parent should
  // fail at this point.
  MakeMemSink(group_by, "");
  RemoveGroupByRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_NOT_OK(result);
  EXPECT_THAT(result.status(), HasCompilerError("'groupby.*' should be followed by an 'agg.*'"));
}

TEST_F(RulesTest, MergeAndRemove) {
  MemorySourceIR* mem_source = MakeMemSource();
  GroupByIR* group_by = MakeGroupBy(mem_source, {MakeColumn("col1", 0), MakeColumn("col2", 0)});
  BlockingAggIR* agg =
      MakeBlockingAgg(group_by, {}, {{"outcount", MakeMeanFunc(MakeColumn("count", 0))}});
  MakeMemSink(agg, "");

  int64_t group_by_node_id = group_by->id();
  std::vector<int64_t> groupby_ids;
  for (ColumnIR* g : group_by->groups()) {
    groupby_ids.push_back(g->id());
  }

  // Do remove groupby after running both
  MergeGroupByIntoGroupAcceptorRule rule1(IRNodeType::kBlockingAgg);
  RemoveGroupByRule rule2;
  auto result1 = rule1.Execute(graph.get());
  ASSERT_OK(result1);
  ASSERT_TRUE(result1.ConsumeValueOrDie());
  auto result2 = rule2.Execute(graph.get());
  ASSERT_OK(result1);
  ASSERT_TRUE(result2.ConsumeValueOrDie());

  EXPECT_FALSE(graph->HasNode(group_by_node_id));
  // Make sure no groupby sticks around either
  for (int64_t i : graph->dag().TopologicalSort()) {
    EXPECT_FALSE(Match(graph->Get(i), GroupBy())) << absl::Substitute("Node $0 is a groupby()", i);
  }

  EXPECT_THAT(agg->parents(), ElementsAre(mem_source));

  std::vector<std::string> actual_group_names;
  std::vector<int64_t> actual_group_ids;
  for (ColumnIR* g : agg->groups()) {
    actual_group_names.push_back(g->col_name());
    actual_group_ids.push_back(g->id());
  }

  EXPECT_THAT(actual_group_names, ElementsAre("col1", "col2"));
  EXPECT_THAT(actual_group_ids, Not(UnorderedElementsAreArray(groupby_ids)));
}

TEST_F(RulesTest, MergeAndRemove_MultipleAggs) {
  MemorySourceIR* mem_source = MakeMemSource();
  GroupByIR* group_by = MakeGroupBy(mem_source, {MakeColumn("col1", 0), MakeColumn("col2", 0)});
  BlockingAggIR* agg1 =
      MakeBlockingAgg(group_by, {}, {{"outcount", MakeMeanFunc(MakeColumn("count", 0))}});
  MakeMemSink(agg1, "");
  BlockingAggIR* agg2 =
      MakeBlockingAgg(group_by, {}, {{"latency_mean", MakeMeanFunc(MakeColumn("latency", 0))}});
  MakeMemSink(agg2, "");

  int64_t group_by_node_id = group_by->id();

  // Verification that everything is constructed correctly.
  EXPECT_THAT(agg1->parents(), ElementsAre(group_by));
  EXPECT_EQ(agg1->groups().size(), 0);
  EXPECT_THAT(agg2->parents(), ElementsAre(group_by));
  EXPECT_EQ(agg2->groups().size(), 0);

  // Do remove groupby after running both
  MergeGroupByIntoGroupAcceptorRule rule1(IRNodeType::kBlockingAgg);
  RemoveGroupByRule rule2;
  auto result1 = rule1.Execute(graph.get());
  ASSERT_OK(result1);
  ASSERT_TRUE(result1.ConsumeValueOrDie());
  auto result2 = rule2.Execute(graph.get());
  ASSERT_OK(result1);
  ASSERT_TRUE(result2.ConsumeValueOrDie());

  EXPECT_FALSE(graph->HasNode(group_by_node_id));
  // Make sure no groupby sticks around either
  for (int64_t i : graph->dag().TopologicalSort()) {
    EXPECT_FALSE(Match(graph->Get(i), GroupBy())) << absl::Substitute("Node $0 is a groupby()", i);
  }

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

TEST_F(RulesTest, MergeAndRemove_GroupByOnMetadataColumns) {
  MemorySourceIR* mem_source = MakeMemSource();
  GroupByIR* group_by =
      MakeGroupBy(mem_source, {MakeMetadataIR("service", 0), MakeColumn("col2", 0)});
  BlockingAggIR* agg =
      MakeBlockingAgg(group_by, {}, {{"outcount", MakeMeanFunc(MakeColumn("count", 0))}});
  MakeMemSink(agg, "");

  int64_t group_by_node_id = group_by->id();
  std::vector<int64_t> groupby_ids;
  for (ColumnIR* g : group_by->groups()) {
    groupby_ids.push_back(g->id());
  }

  // Do remove groupby after running both
  MergeGroupByIntoGroupAcceptorRule rule1(IRNodeType::kBlockingAgg);
  RemoveGroupByRule rule2;
  auto result1 = rule1.Execute(graph.get());
  ASSERT_OK(result1);
  ASSERT_TRUE(result1.ConsumeValueOrDie());
  auto result2 = rule2.Execute(graph.get());
  ASSERT_OK(result1);
  ASSERT_TRUE(result2.ConsumeValueOrDie());

  EXPECT_FALSE(graph->HasNode(group_by_node_id));
  // Make sure no groupby sticks around either
  for (int64_t i : graph->dag().TopologicalSort()) {
    EXPECT_FALSE(Match(graph->Get(i), GroupBy())) << absl::Substitute("Node $0 is a groupby()", i);
  }

  EXPECT_THAT(agg->parents(), ElementsAre(mem_source));

  std::vector<std::string> actual_group_names;
  std::vector<int64_t> actual_group_ids;
  for (ColumnIR* g : agg->groups()) {
    actual_group_names.push_back(g->col_name());
    actual_group_ids.push_back(g->id());
  }
  EXPECT_THAT(actual_group_ids, Not(UnorderedElementsAreArray(groupby_ids)));

  EXPECT_MATCH(agg->groups()[0], Metadata());
  EXPECT_TRUE(!Match(agg->groups()[1], Metadata()));
  EXPECT_MATCH(agg->groups()[1], ColumnNode());
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
