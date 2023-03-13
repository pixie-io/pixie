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
#include <vector>

#include "src/carnot/planner/compiler/analyzer/analyzer.h"
#include "src/carnot/planner/compiler/optimizer/merge_nodes_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::UnorderedElementsAre;

class MergeNodesTest : public ASTVisitorTest {
 public:
  Status Analyze(std::shared_ptr<IR> ir_graph) {
    PX_ASSIGN_OR_RETURN(std::unique_ptr<Analyzer> analyzer,
                        Analyzer::Create(compiler_state_.get()));
    return analyzer->Execute(ir_graph.get());
  }
};

TEST_F(MergeNodesTest, can_merge_test) {
  auto mem_src_cpu1 = MakeMemSource("cpu", cpu_relation);
  auto mem_src_cpu2 = MakeMemSource("cpu");
  auto mem_src_network1 = MakeMemSource("network");

  MergeNodesRule rule(compiler_state_.get());
  EXPECT_TRUE(rule.CanMerge(mem_src_cpu1, mem_src_cpu2));
  EXPECT_FALSE(rule.CanMerge(mem_src_cpu1, mem_src_network1));
}

TEST_F(MergeNodesTest, merge_sets_mem_src) {
  auto mem_src_cpu1 = MakeMemSource("cpu", cpu_relation);
  auto mem_src_cpu2 = MakeMemSource("cpu", cpu_relation);
  auto mem_src_network1 = MakeMemSource("network", network_relation);

  MergeNodesRule rule(compiler_state_.get());
  auto matching_sets = rule.FindMatchingSets({mem_src_cpu1, mem_src_cpu2, mem_src_network1});
  EXPECT_EQ(matching_sets.size(), 2);

  EXPECT_THAT(matching_sets[0].operators, ElementsAre(mem_src_cpu1, mem_src_cpu2));
  EXPECT_THAT(matching_sets[1].operators, ElementsAre(mem_src_network1));
}

TEST_F(MergeNodesTest, merge_sets_map) {
  int64_t num_runs = 5;
  std::vector<OperatorIR*> maps;
  for (int64_t i = 0; i < num_runs; ++i) {
    auto mem_src_cpu = MakeMemSource("cpu");
    auto map = MakeMap(mem_src_cpu, {{"cpu0_mo", MakeAddFunc(MakeColumn("cpu0", 0), MakeInt(2))}});
    maps.push_back(map);
  }

  MergeNodesRule rule(compiler_state_.get());
  auto matching_sets = rule.FindMatchingSets(maps);
  ASSERT_EQ(matching_sets.size(), 1);

  EXPECT_THAT(matching_sets[0].operators, ElementsAreArray(maps));
}

TEST_F(MergeNodesTest, merge_maps) {
  // Test to make sure weird map merging works as expected.
  auto og_mem_src = MakeMemSource("cpu", cpu_relation);
  std::vector<OperatorIR*> maps;
  {
    auto child_fn = MakeAddFunc(MakeColumn("cpu0", 0), MakeInt(2));
    auto parent_fn = MakeAddFunc(child_fn, MakeInt(23));
    auto map = MakeMap(og_mem_src, {{"fn0", parent_fn}});
    maps.push_back(map);
    MakeMemSink(map, "");
  }

  {
    auto child_fn = MakeAddFunc(MakeColumn("cpu0", 0), MakeInt(2));
    auto parent_fn = MakeAddFunc(child_fn, MakeInt(23));
    auto map = MakeMap(og_mem_src, {{"fn1", parent_fn}});
    maps.push_back(map);
    MakeMemSink(map, "");
  }

  EXPECT_OK(Analyze(graph));

  MergeNodesRule rule(compiler_state_.get());
  auto merged_op_or_s = rule.MergeOps(graph.get(), maps);
  EXPECT_OK(merged_op_or_s);

  auto merged_op = merged_op_or_s.ConsumeValueOrDie();

  ASSERT_MATCH(merged_op, Map());
  MapIR* map = static_cast<MapIR*>(merged_op);
  EXPECT_EQ(map->col_exprs().size(), 2);
  EXPECT_EQ(map->col_exprs()[0].name, "fn0");
  auto expr_fn0 = static_cast<FuncIR*>(map->col_exprs()[0].node);
  ASSERT_TRUE(Match(expr_fn0, Add(Add(ColumnNode("cpu0"), Int(2)), Int(23))))
      << expr_fn0->DebugString();

  EXPECT_EQ(map->col_exprs()[1].name, "fn1");
  auto expr_fn1 = static_cast<FuncIR*>(map->col_exprs()[1].node);
  ASSERT_TRUE(Match(expr_fn1, Add(Add(ColumnNode("cpu0"), Int(2)), Int(23))))
      << expr_fn1->DebugString();
}

TEST_F(MergeNodesTest, merge_memory_sources_different_columns) {
  // Test to make sure weird map merging works as expected.
  std::vector<OperatorIR*> srcs;
  {
    auto mem_src = MakeMemSource("cpu", {"upid", "cpu0"});
    MakeMemSink(mem_src, "");
    srcs.push_back(mem_src);
  }

  {
    auto mem_src = MakeMemSource("cpu", {"upid", "agent_id"});
    MakeMemSink(mem_src, "");
    srcs.push_back(mem_src);
  }

  EXPECT_OK(Analyze(graph));

  MergeNodesRule rule(compiler_state_.get());
  auto merged_op_or_s = rule.MergeOps(graph.get(), srcs);
  EXPECT_OK(merged_op_or_s);

  auto merged_op = merged_op_or_s.ConsumeValueOrDie();

  ASSERT_MATCH(merged_op, MemorySource());
  MemorySourceIR* mem_src = static_cast<MemorySourceIR*>(merged_op);
  EXPECT_EQ(mem_src->table_name(), "cpu");
  EXPECT_THAT(mem_src->column_names(), UnorderedElementsAre("upid", "cpu0", "agent_id"));
}

TEST_F(MergeNodesTest, memory_sources_with_different_ranges_fails) {
  // TODO(philkuz) (PP-1812) remove this test as it will be incorrect.
  // Make sure that memory sources don't match if they have different time ranges.
  std::vector<OperatorIR*> srcs;
  {
    auto mem_src = MakeMemSource("cpu", {"upid", "cpu0"});
    mem_src->SetTimeStartNS(10);
    mem_src->SetTimeStopNS(100);
    MakeMemSink(mem_src, "");
    srcs.push_back(mem_src);
  }

  {
    auto mem_src = MakeMemSource("cpu", {"upid", "agent_id"});
    mem_src->SetTimeStartNS(50);
    mem_src->SetTimeStopNS(150);
    MakeMemSink(mem_src, "");
    srcs.push_back(mem_src);
  }

  EXPECT_OK(Analyze(graph));

  MergeNodesRule rule(compiler_state_.get());
  EXPECT_FALSE(rule.CanMerge(srcs[0], srcs[1]));
}

TEST_F(MergeNodesTest, DISABLED_merge_memory_sources_intersecting_time_ranges) {
  // TODO(philkuz) (PP-1812) enable this when you can merge memory sources together with
  // intersecting time ranges.
  std::vector<OperatorIR*> srcs;
  {
    auto mem_src = MakeMemSource("cpu", {"upid", "cpu0"});
    mem_src->SetTimeStartNS(10);
    mem_src->SetTimeStopNS(100);
    MakeMemSink(mem_src, "");
    srcs.push_back(mem_src);
  }

  {
    auto mem_src = MakeMemSource("cpu", {"upid", "agent_id"});
    mem_src->SetTimeStartNS(50);
    mem_src->SetTimeStopNS(150);
    MakeMemSink(mem_src, "");
    srcs.push_back(mem_src);
  }

  EXPECT_OK(Analyze(graph));

  MergeNodesRule rule(compiler_state_.get());
  EXPECT_TRUE(rule.CanMerge(srcs[0], srcs[1]));

  auto merged_op_or_s = rule.MergeOps(graph.get(), srcs);
  EXPECT_OK(merged_op_or_s);

  auto merged_op = merged_op_or_s.ConsumeValueOrDie();

  ASSERT_MATCH(merged_op, MemorySource());
  MemorySourceIR* mem_src = static_cast<MemorySourceIR*>(merged_op);
  EXPECT_EQ(mem_src->table_name(), "cpu");
  EXPECT_THAT(mem_src->column_names(), UnorderedElementsAre("upid", "cpu0", "agent_id"));

  EXPECT_EQ(mem_src->time_start_ns(), 10);
  EXPECT_EQ(mem_src->time_stop_ns(), 150);
}

TEST_F(MergeNodesTest, memory_sources_with_non_intersecting_time_ranges) {
  std::vector<OperatorIR*> srcs;
  {
    auto mem_src = MakeMemSource("cpu", {"upid", "cpu0"});
    mem_src->SetTimeStartNS(10);
    mem_src->SetTimeStopNS(50);
    MakeMemSink(mem_src, "");
    srcs.push_back(mem_src);
  }

  {
    auto mem_src = MakeMemSource("cpu", {"upid", "agent_id"});
    mem_src->SetTimeStartNS(100);
    mem_src->SetTimeStopNS(150);
    MakeMemSink(mem_src, "");
    srcs.push_back(mem_src);
  }

  EXPECT_OK(Analyze(graph));
  MergeNodesRule rule(compiler_state_.get());
  EXPECT_FALSE(rule.CanMerge(srcs[0], srcs[1]));
}

TEST_F(MergeNodesTest, limit_merge) {
  auto mem_src = MakeMemSource("cpu", {"upid", "cpu0"});
  mem_src->SetTimeStartNS(10);
  mem_src->SetTimeStopNS(100);
  auto map = MakeMap(mem_src, {{"cpu0_mo", MakeAddFunc(MakeColumn("cpu0", 0), MakeInt(2))}});
  auto limit1 = MakeLimit(map, 100);
  auto limit2 = MakeLimit(map, 100);
  auto limit3 = MakeLimit(map, 101);
  MakeMemSink(limit1, "1");
  MakeMemSink(limit2, "2");
  MakeMemSink(limit3, "2");

  EXPECT_OK(Analyze(graph));

  MergeNodesRule rule(compiler_state_.get());
  EXPECT_TRUE(rule.CanMerge(limit1, limit2));
  EXPECT_FALSE(rule.CanMerge(limit1, limit3));
}

TEST_F(MergeNodesTest, limit_merge_different_relations_shouldnt_merge) {
  auto mem_src = MakeMemSource("cpu", {"upid", "cpu0", "cpu1"});
  mem_src->SetTimeStartNS(10);
  mem_src->SetTimeStopNS(100);
  auto map1 = MakeMap(mem_src, {{"cpu0_mo", MakeAddFunc(MakeColumn("cpu0", 0), MakeInt(2))}});
  auto map2 = MakeMap(mem_src, {{"cpu1_mo", MakeAddFunc(MakeColumn("cpu1", 0), MakeInt(2))}});
  auto limit1 = MakeLimit(map1, 100);
  auto limit2 = MakeLimit(map2, 100);
  MakeMemSink(limit1, "1");
  MakeMemSink(limit2, "2");

  EXPECT_OK(Analyze(graph));

  MergeNodesRule rule(compiler_state_.get());
  EXPECT_FALSE(rule.CanMerge(limit1, limit2));
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
