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

#include <unordered_map>
#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/analyzer/analyzer.h"
#include "src/carnot/planner/compiler/optimizer/optimizer.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/parser/parser.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using table_store::schema::Relation;
using ::testing::_;

using ::testing::Contains;
using ::testing::ContainsRegex;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Not;
using ::testing::UnorderedElementsAre;

class OptimizerTest : public ASTVisitorTest {
 protected:
  Status Analyze(std::shared_ptr<IR> ir_graph) {
    PX_ASSIGN_OR_RETURN(std::unique_ptr<Analyzer> analyzer,
                        Analyzer::Create(compiler_state_.get()));
    return analyzer->Execute(ir_graph.get());
  }
  Status Optimize(std::shared_ptr<IR> ir_graph) {
    PX_ASSIGN_OR_RETURN(std::unique_ptr<Optimizer> optimizer,
                        Optimizer::Create(compiler_state_.get()));
    return optimizer->Execute(ir_graph.get());
  }
};

TEST_F(OptimizerTest, mem_src_and_sink_test) {
  int64_t num_runs = 5;
  for (int64_t i = 0; i < num_runs; i++) {
    auto mem_src = MakeMemSource("cpu", cpu_relation);
    MakeMemSink(mem_src, "");
  }

  EXPECT_OK(Analyze(graph));
  EXPECT_OK(Optimize(graph));

  auto memory_sources = graph->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(memory_sources.size(), 1);

  auto mem_src = static_cast<MemorySourceIR*>(memory_sources[0]);
  ASSERT_EQ(mem_src->Children().size(), 1);

  auto limit = mem_src->Children()[0];
  ASSERT_MATCH(limit, Limit());
  ASSERT_EQ(limit->Children().size(), 5);

  for (auto ch : limit->Children()) {
    ASSERT_MATCH(ch, MemorySink());
  }
}

TEST_F(OptimizerTest, mem_src_different_columns_test) {
  {
    auto mem_src = MakeMemSource("cpu", {"upid", "cpu0"});
    MakeMemSink(mem_src, "");
  }

  {
    auto mem_src = MakeMemSource("cpu", {"upid", "cpu1"});
    MakeMemSink(mem_src, "");
  }

  EXPECT_OK(Analyze(graph));
  EXPECT_OK(Optimize(graph));

  auto memory_sources = graph->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(memory_sources.size(), 1);
  auto mem_src = static_cast<MemorySourceIR*>(memory_sources[0]);
  EXPECT_THAT(mem_src->column_names(), UnorderedElementsAre("upid", "cpu0", "cpu1"));
  ASSERT_EQ(mem_src->Children().size(), 1);

  auto limit = mem_src->Children()[0];
  ASSERT_MATCH(limit, Limit());
  ASSERT_EQ(limit->Children().size(), 2);

  for (auto ch : limit->Children()) {
    ASSERT_MATCH(ch, MemorySink());
  }
}

TEST_F(OptimizerTest, mem_src_map_sink_test) {
  // Test to make sure we can remove duplicated graphs.
  int64_t num_runs = 5;
  for (int64_t i = 0; i < num_runs; i++) {
    auto mem_src = MakeMemSource("cpu", cpu_relation);
    auto child_fn = MakeAddFunc(MakeColumn("cpu0", 0), MakeInt(2));
    auto parent_fn = MakeAddFunc(child_fn, MakeInt(23));
    auto map = MakeMap(mem_src, {{"fn", parent_fn}});
    MakeMemSink(map, "");
  }

  EXPECT_OK(Analyze(graph));
  EXPECT_OK(Optimize(graph));

  auto memory_sources = graph->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(memory_sources.size(), 1);
  auto mem_src = static_cast<MemorySourceIR*>(memory_sources[0]);
  ASSERT_EQ(mem_src->Children().size(), 1);
  ASSERT_MATCH(mem_src->Children()[0], Map());

  MapIR* map = static_cast<MapIR*>(mem_src->Children()[0]);
  EXPECT_EQ(map->col_exprs().size(), 1);
  EXPECT_EQ(map->col_exprs()[0].name, "fn");
  auto expr_fn = static_cast<FuncIR*>(map->col_exprs()[0].node);
  ASSERT_EQ(expr_fn->args().size(), 2);
  ASSERT_TRUE(Match(expr_fn, Add(Add(ColumnNode("cpu0"), Int(2)), Int(23))))
      << expr_fn->DebugString();

  // Child is a Limit because of the analyzer.
  EXPECT_MATCH(map->Children()[0], Limit());
  EXPECT_MATCH(map->Children()[0]->Children()[0], MemorySink());
}

TEST_F(OptimizerTest, DISABLED_mem_src_different_map_exprs_sink_test) {
  // TODO(philkuz) (PP-1812) this doesn't merge yet.
  // Test to make sure we can combine maps that have different functions.
  {
    auto mem_src = MakeMemSource("cpu", cpu_relation);
    auto child_fn = MakeAddFunc(MakeColumn("cpu0", 0), MakeInt(2));
    auto parent_fn = MakeAddFunc(child_fn, MakeInt(23));
    EXPECT_OK(parent_fn->SetResolvedType(ValueType::Create(types::FLOAT64, types::ST_NONE)));
    auto map = MakeMap(mem_src, {{"fn0", parent_fn}});
    MakeMemSink(map, "");
  }

  {
    auto mem_src = MakeMemSource("cpu", cpu_relation);
    auto child_fn = MakeAddFunc(MakeColumn("cpu0", 0), MakeInt(2));
    auto map = MakeMap(mem_src, {{"fn1", child_fn}});
    MakeMemSink(map, "");
  }

  EXPECT_OK(Analyze(graph));
  EXPECT_OK(Optimize(graph));

  auto memory_sources = graph->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(memory_sources.size(), 1);
  auto mem_src = static_cast<MemorySourceIR*>(memory_sources[0]);
  ASSERT_EQ(mem_src->Children().size(), 1);
  ASSERT_MATCH(mem_src->Children()[0], Map());

  MapIR* map = static_cast<MapIR*>(mem_src->Children()[0]);
  ASSERT_EQ(map->col_exprs().size(), 2);
  auto col_expr0 = map->col_exprs()[0];
  auto col_expr1 = map->col_exprs()[1];
  EXPECT_EQ(col_expr0.name, "fn0");
  EXPECT_EQ(col_expr1.name, "fn1");

  EXPECT_TRUE(Match(col_expr0.node, Add(Add(ColumnNode("cpu0"), Int(2)), Int(23))))
      << col_expr0.node->DebugString();
  EXPECT_TRUE(Match(col_expr1.node, Add(ColumnNode("cpu0"), Int(2))))
      << col_expr0.node->DebugString();

  // Child is a Limit because of the analyzer.
  EXPECT_MATCH(map->Children()[0], Limit());
  EXPECT_MATCH(map->Children()[0]->Children()[0], MemorySink());
}

TEST_F(OptimizerTest, mem_src_agg_test) {
  int64_t num_runs = 5;
  for (int64_t i = 0; i < num_runs; i++) {
    auto mem_src = MakeMemSource("cpu", cpu_relation);
    auto child_fn = MakeMeanFunc(MakeColumn("cpu0", 0));
    auto agg = MakeBlockingAgg(mem_src, {}, {{"fn", child_fn}});
    MakeMemSink(agg, "");
  }

  EXPECT_OK(Analyze(graph));
  EXPECT_OK(Optimize(graph));

  auto memory_sources = graph->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(memory_sources.size(), 1);
  auto mem_src = static_cast<MemorySourceIR*>(memory_sources[0]);
  ASSERT_EQ(mem_src->Children().size(), 1);
  EXPECT_MATCH(mem_src->Children()[0], BlockingAgg());

  BlockingAggIR* blocking_agg = static_cast<BlockingAggIR*>(mem_src->Children()[0]);
  EXPECT_EQ(blocking_agg->groups().size(), 0);

  EXPECT_EQ(blocking_agg->aggregate_expressions().size(), 1);

  EXPECT_EQ(blocking_agg->aggregate_expressions()[0].name, "fn");
  auto aggregate_fn = static_cast<FuncIR*>(blocking_agg->aggregate_expressions()[0].node);
  EXPECT_MATCH(aggregate_fn, Func("mean", ColumnNode("cpu0", 0)));
  // Child is a Limit because of the analyzer.
  EXPECT_MATCH(blocking_agg->Children()[0], Limit());
  EXPECT_MATCH(blocking_agg->Children()[0]->Children()[0], MemorySink());
}

TEST_F(OptimizerTest, DISABLED_mem_src_agg_merge_exprs_test) {
  // TODO(philkuz) (PP-1812) enable when we can have arbitrary expression merges.
  // In this test we have matching group but mis-matching expressions.
  {
    auto mem_src = MakeMemSource("cpu", cpu_relation);
    auto agg_fn = MakeMeanFunc(MakeColumn("cpu0", 0));
    auto agg = MakeBlockingAgg(mem_src, {}, {{"fn0", agg_fn}});
    MakeMemSink(agg, "");
  }
  {
    auto mem_src = MakeMemSource("cpu", cpu_relation);
    auto agg_fn = MakeMeanFunc(MakeColumn("cpu1", 0));
    auto agg = MakeBlockingAgg(mem_src, {}, {{"fn1", agg_fn}});
    MakeMemSink(agg, "");
  }

  EXPECT_OK(Analyze(graph));
  EXPECT_OK(Optimize(graph));

  auto memory_sources = graph->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(memory_sources.size(), 1);
  auto mem_src = static_cast<MemorySourceIR*>(memory_sources[0]);
  ASSERT_EQ(mem_src->Children().size(), 1);
  EXPECT_MATCH(mem_src->Children()[0], BlockingAgg());

  BlockingAggIR* blocking_agg = static_cast<BlockingAggIR*>(mem_src->Children()[0]);
  EXPECT_EQ(blocking_agg->groups().size(), 0);

  ASSERT_EQ(blocking_agg->aggregate_expressions().size(), 2);
  auto agg_col_expr0 = blocking_agg->aggregate_expressions()[0];
  auto agg_col_expr1 = blocking_agg->aggregate_expressions()[1];
  if (agg_col_expr0.name == "fn1") {
    auto temp = agg_col_expr0;
    agg_col_expr0 = agg_col_expr1;
    agg_col_expr1 = temp;
  }
  EXPECT_EQ(agg_col_expr0.name, "fn0");
  EXPECT_EQ(agg_col_expr1.name, "fn1");

  EXPECT_MATCH(agg_col_expr0.node, Func("mean", ColumnNode("cpu0")));
  EXPECT_MATCH(agg_col_expr1.node, Func("mean", ColumnNode("cpu1")));

  // Make sure there is only one blocking agg.
  ASSERT_EQ(graph->FindNodesThatMatch(BlockingAgg()).size(), 1);

  // Op is a limit because of the analyzer.
  EXPECT_MATCH(blocking_agg->Children()[0], Limit());
  EXPECT_MATCH(blocking_agg->Children()[0]->Children()[0], MemorySink());
}

TEST_F(OptimizerTest, mem_src_join_test) {
  int64_t num_runs = 5;
  for (int64_t i = 0; i < num_runs; i++) {
    // The parents of the Join() are heterogenous, so shouldn't combine them together.
    auto mem_src1 = MakeMemSource("cpu", cpu_relation);
    auto mem_src2 = MakeMemSource("network", network_relation);
    auto agg_fn = MakeMeanFunc(MakeColumn("cpu0", 0));
    auto agg = MakeBlockingAgg(mem_src1, {MakeColumn("upid", 0)}, {{"cpu0_mean", agg_fn}});
    Relation agg_relation({types::UINT128, types::FLOAT64}, {"upid", "cpu0_mean"});
    auto join = MakeJoin({mem_src2, agg}, "inner", cpu_relation, agg_relation, {"upid"}, {"upid"},
                         {"", "_x"});
    MakeMemSink(join, "");
  }
  EXPECT_OK(Analyze(graph));
  EXPECT_OK(Optimize(graph));

  auto memory_sources = graph->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(memory_sources.size(), 2);
  MemorySourceIR* mem_src_cpu = static_cast<MemorySourceIR*>(memory_sources[0]);
  MemorySourceIR* mem_src_network = static_cast<MemorySourceIR*>(memory_sources[1]);
  // Mismatched, map them back.
  if (mem_src_cpu->table_name() == "network") {
    auto mem_src_network_temp = mem_src_network;
    mem_src_network = mem_src_cpu;
    mem_src_cpu = mem_src_network_temp;
  }

  ASSERT_EQ(mem_src_cpu->table_name(), "cpu");
  ASSERT_EQ(mem_src_network->table_name(), "network");
  ASSERT_EQ(mem_src_cpu->Children().size(), 1);
  EXPECT_MATCH(mem_src_cpu->Children()[0], BlockingAgg());

  BlockingAggIR* blocking_agg = static_cast<BlockingAggIR*>(mem_src_cpu->Children()[0]);
  EXPECT_EQ(blocking_agg->groups().size(), 1);
  EXPECT_MATCH(blocking_agg->groups()[0], ColumnNode("upid"));

  EXPECT_EQ(blocking_agg->aggregate_expressions().size(), 1);

  EXPECT_EQ(blocking_agg->aggregate_expressions()[0].name, "cpu0_mean");
  auto aggregate_fn = static_cast<FuncIR*>(blocking_agg->aggregate_expressions()[0].node);
  ASSERT_EQ(aggregate_fn->args().size(), 1);
  EXPECT_MATCH(aggregate_fn->args()[0], ColumnNode("cpu0", 0));
  ASSERT_MATCH(blocking_agg->Children()[0], Join());
  JoinIR* join = static_cast<JoinIR*>(blocking_agg->Children()[0]);
  EXPECT_MATCH(join->left_on_columns()[0], ColumnNode("upid"));
  EXPECT_MATCH(join->right_on_columns()[0], ColumnNode("upid"));
  EXPECT_EQ(mem_src_network->Children()[0], join);

  EXPECT_EQ(graph->FindNodesThatMatch(Join()).size(), 1);

  // Op is a limit because of the analyzer.
  EXPECT_MATCH(join->Children()[0], Limit());
  EXPECT_MATCH(join->Children()[0]->Children()[0], MemorySink());
}

TEST_F(OptimizerTest, mem_src_join_same_src_test) {
  int64_t num_runs = 5;
  for (int64_t i = 0; i < num_runs; i++) {
    // The parents of the Join() have some shared properties and should be merged.
    Relation mem_src_relation({types::UINT128, types::FLOAT64}, {"upid", "cpu0"});
    auto mem_src0 = MakeMemSource("cpu", mem_src_relation);
    auto mem_src1 = MakeMemSource("cpu", mem_src_relation);
    auto agg_fn = MakeMeanFunc(MakeColumn("cpu0", 0));
    auto column = MakeColumn("upid", 0);
    ASSERT_OK(column->SetResolvedType(ValueType::Create(types::UINT128, types::ST_NONE)));
    auto agg = MakeBlockingAgg(mem_src0, {column}, {{"cpu0_mean", agg_fn}});
    Relation agg_relation({types::UINT128, types::FLOAT64}, {"upid", "cpu0_mean"});
    auto join = MakeJoin({mem_src1, agg}, "inner", mem_src_relation, agg_relation, {"upid"},
                         {"upid"}, {"", "_x"});
    MakeMemSink(join, "");
  }
  ASSERT_OK(Analyze(graph));
  EXPECT_OK(Optimize(graph));

  auto memory_sources = graph->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(memory_sources.size(), 1);
  MemorySourceIR* mem_src_cpu = static_cast<MemorySourceIR*>(memory_sources[0]);

  ASSERT_EQ(mem_src_cpu->table_name(), "cpu");
  ASSERT_EQ(mem_src_cpu->Children().size(), 2);
  EXPECT_MATCH(mem_src_cpu->Children()[0], BlockingAgg());

  BlockingAggIR* blocking_agg = static_cast<BlockingAggIR*>(mem_src_cpu->Children()[0]);
  EXPECT_EQ(blocking_agg->groups().size(), 1);
  EXPECT_MATCH(blocking_agg->groups()[0], ColumnNode("upid"));

  EXPECT_EQ(blocking_agg->aggregate_expressions().size(), 1);

  EXPECT_EQ(blocking_agg->aggregate_expressions()[0].name, "cpu0_mean");
  auto aggregate_fn = static_cast<FuncIR*>(blocking_agg->aggregate_expressions()[0].node);
  ASSERT_EQ(aggregate_fn->args().size(), 1);
  EXPECT_MATCH(aggregate_fn->args()[0], ColumnNode("cpu0", 0));
  ASSERT_MATCH(blocking_agg->Children()[0], Join());

  JoinIR* join = static_cast<JoinIR*>(blocking_agg->Children()[0]);
  EXPECT_MATCH(join->left_on_columns()[0], ColumnNode("upid"));
  EXPECT_MATCH(join->right_on_columns()[0], ColumnNode("upid"));
  // Make sure that the child of Mem src cpu is also Join.
  EXPECT_EQ(mem_src_cpu->Children()[1], join);

  EXPECT_MATCH(join->Children()[0], Limit());
  EXPECT_MATCH(join->Children()[0]->Children()[0], MemorySink());
}

TEST_F(OptimizerTest, mem_src_filter_test) {
  // Test to make sure we can remove duplicated graphs.
  int64_t num_runs = 5;
  for (int64_t i = 0; i < num_runs; i++) {
    auto mem_src = MakeMemSource("cpu");
    auto filter_expr = MakeEqualsFunc(MakeColumn("cpu0", 0), MakeInt(23));
    auto filter = MakeFilter(mem_src, filter_expr);
    MakeMemSink(filter, "");
  }
  EXPECT_OK(Analyze(graph));
  EXPECT_OK(Optimize(graph));

  auto memory_sources = graph->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(memory_sources.size(), 1);
  MemorySourceIR* mem_src_cpu = static_cast<MemorySourceIR*>(memory_sources[0]);

  ASSERT_EQ(mem_src_cpu->table_name(), "cpu");
  ASSERT_EQ(mem_src_cpu->Children().size(), 1)
      << absl::StrJoin(mem_src_cpu->Children(), ",", [](std::string* out, IRNode* in) {
           absl::StrAppend(out, in->DebugString());
         });
  EXPECT_MATCH(mem_src_cpu->Children()[0], Filter());
  auto filter = static_cast<FilterIR*>(mem_src_cpu->Children()[0]);

  EXPECT_MATCH(filter->filter_expr(), Equals(ColumnNode("cpu0"), Int(23)));
  auto limit = filter->Children()[0];
  ASSERT_MATCH(limit, Limit());
  ASSERT_MATCH(limit->Children()[0], MemorySink());
  EXPECT_EQ(limit->Children().size(), num_runs);
}

TEST_F(OptimizerTest, mem_src_different_filter_test) {
  // Test to make sure we can remove duplicated graphs.
  {
    // Graph 0.
    auto mem_src = MakeMemSource("cpu", cpu_relation);
    auto filter_expr = MakeEqualsFunc(MakeColumn("cpu0", 0), MakeInt(23));
    auto filter = MakeFilter(mem_src, filter_expr);
    MakeMemSink(filter, "");
  }
  {
    // Graph 1.
    auto mem_src = MakeMemSource("cpu", cpu_relation);
    auto equals_fn1 = MakeEqualsFunc(MakeColumn("cpu0", 0), MakeInt(23));
    auto equals_fn2 = MakeEqualsFunc(MakeColumn("cpu0", 0), MakeInt(46));
    auto filter_expr = MakeOrFunc(equals_fn1, equals_fn2);
    auto filter = MakeFilter(mem_src, filter_expr);
    MakeMemSink(filter, "");
  }
  EXPECT_OK(Analyze(graph));
  EXPECT_OK(Optimize(graph));

  auto memory_sources = graph->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(memory_sources.size(), 1);
  MemorySourceIR* mem_src_cpu = static_cast<MemorySourceIR*>(memory_sources[0]);

  ASSERT_EQ(mem_src_cpu->table_name(), "cpu");
  ASSERT_EQ(mem_src_cpu->Children().size(), 2);
  EXPECT_MATCH(mem_src_cpu->Children()[0], Filter());
  EXPECT_MATCH(mem_src_cpu->Children()[1], Filter());
  auto filter0 = static_cast<FilterIR*>(mem_src_cpu->Children()[0]);
  auto filter1 = static_cast<FilterIR*>(mem_src_cpu->Children()[1]);
  auto filter_expr0 = filter0->filter_expr();
  auto filter_expr1 = filter1->filter_expr();
  // Hack to make sure the expr matcher works, as the order is non-deterministic.
  if (!Match(filter_expr0, Equals(ColumnNode("cpu0"), Int(23)))) {
    auto temp = filter_expr0;
    filter_expr0 = filter_expr1;
    filter_expr1 = temp;
  }

  EXPECT_MATCH(filter_expr0, Equals(ColumnNode("cpu0"), Int(23)));
  EXPECT_MATCH(filter_expr1,
               LogicalOr(Equals(ColumnNode("cpu0"), Int(23)), Equals(ColumnNode("cpu0"), Int(46))));
  EXPECT_EQ(filter0->Children().size(), 1);
  auto limit0 = filter0->Children()[0];
  ASSERT_EQ(limit0->Children().size(), 1);
  EXPECT_MATCH(limit0->Children()[0], MemorySink());

  EXPECT_EQ(filter1->Children().size(), 1);
  auto limit1 = filter1->Children()[0];
  ASSERT_EQ(limit1->Children().size(), 1);
  EXPECT_MATCH(limit1->Children()[0], MemorySink());
}

TEST_F(OptimizerTest, dont_merge_joins_that_have_different_parents) {
  // This test poses a situation where two joins might look similar, but have different
  // parents and shouldn't be merged.

  int64_t num_runs = 5;
  for (int64_t i = 0; i < num_runs; ++i) {
    // The parents of the Join() are heterogenous, so shouldn't combine them together.
    auto mem_src1 = MakeMemSource("cpu", cpu_relation);
    auto mem_src2 = MakeMemSource("network", network_relation);
    auto agg_fn = MakeMeanFunc(MakeColumn("cpu0", 0));
    auto agg = MakeBlockingAgg(mem_src1, {MakeColumn("upid", 0)}, {{"cpu0_mean", agg_fn}});
    Relation agg_relation({types::UINT128, types::FLOAT64}, {"upid", "cpu0_mean"});
    auto join_agg_and_source = MakeJoin({mem_src2, agg}, "inner", cpu_relation, agg_relation,
                                        {"upid"}, {"upid"}, {"", "_x"});
    auto join_both_sources = MakeJoin({mem_src1, mem_src2}, "inner", cpu_relation, network_relation,
                                      {"upid"}, {"upid"}, {"", "_x"});
    MakeMemSink(join_agg_and_source, "");
    MakeMemSink(join_both_sources, "");
  }
  ASSERT_OK(Analyze(graph));
  EXPECT_OK(Optimize(graph));

  auto memory_sources = graph->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(memory_sources.size(), 2);
  MemorySourceIR* mem_src_cpu = static_cast<MemorySourceIR*>(memory_sources[0]);
  MemorySourceIR* mem_src_network = static_cast<MemorySourceIR*>(memory_sources[1]);

  // If Mismatched map them correctly.
  if (mem_src_cpu->table_name() == "network") {
    auto mem_src_network_temp = mem_src_network;
    mem_src_network = mem_src_cpu;
    mem_src_cpu = mem_src_network_temp;
  }

  ASSERT_EQ(mem_src_cpu->table_name(), "cpu");
  ASSERT_EQ(mem_src_network->table_name(), "network");
  ASSERT_EQ(mem_src_cpu->Children().size(), 2);
  EXPECT_MATCH(mem_src_cpu->Children()[0], BlockingAgg());

  BlockingAggIR* blocking_agg = static_cast<BlockingAggIR*>(mem_src_cpu->Children()[0]);
  EXPECT_EQ(blocking_agg->groups().size(), 1);
  EXPECT_MATCH(blocking_agg->groups()[0], ColumnNode("upid"));

  EXPECT_EQ(blocking_agg->aggregate_expressions().size(), 1);

  EXPECT_EQ(blocking_agg->aggregate_expressions()[0].name, "cpu0_mean");
  auto aggregate_fn = static_cast<FuncIR*>(blocking_agg->aggregate_expressions()[0].node);
  ASSERT_EQ(aggregate_fn->args().size(), 1);
  EXPECT_MATCH(aggregate_fn->args()[0], ColumnNode("cpu0", 0));
  ASSERT_MATCH(blocking_agg->Children()[0], Join());

  // Agg Join.
  JoinIR* agg_join = static_cast<JoinIR*>(blocking_agg->Children()[0]);
  EXPECT_MATCH(agg_join->left_on_columns()[0], ColumnNode("upid"));
  EXPECT_MATCH(agg_join->right_on_columns()[0], ColumnNode("upid"));

  ASSERT_MATCH(mem_src_cpu->Children()[1], Join());
  JoinIR* src_join = static_cast<JoinIR*>(mem_src_cpu->Children()[1]);
  EXPECT_THAT(src_join->parents(), ElementsAre(mem_src_cpu, mem_src_network));
  EXPECT_MATCH(src_join->left_on_columns()[0], ColumnNode("upid"));
  EXPECT_MATCH(src_join->right_on_columns()[0], ColumnNode("upid"));

  EXPECT_THAT(mem_src_network->Children(), UnorderedElementsAre(src_join, agg_join));
  EXPECT_THAT(mem_src_cpu->Children(), UnorderedElementsAre(src_join, blocking_agg));

  EXPECT_THAT(graph->FindNodesThatMatch(Join()), UnorderedElementsAre(agg_join, src_join));

  // Child is a Limit because of the analyzer.
  EXPECT_MATCH(src_join->Children()[0], Limit());
  EXPECT_MATCH(src_join->Children()[0]->Children()[0], MemorySink());
  EXPECT_MATCH(agg_join->Children()[0], Limit());
  EXPECT_MATCH(agg_join->Children()[0]->Children()[0], MemorySink());
}

TEST_F(OptimizerTest, shared_mem_src) {
  int64_t num_runs = 5;
  auto og_mem_src = MakeMemSource("cpu", cpu_relation);
  for (int64_t i = 0; i < num_runs; ++i) {
    auto child_fn = MakeAddFunc(MakeColumn("cpu0", 0), MakeInt(2));
    auto map = MakeMap(og_mem_src, {{"fn", child_fn}});
    MakeMemSink(map, "");
  }

  EXPECT_OK(Analyze(graph));
  EXPECT_OK(Optimize(graph));
  // There should only be 1 map.
  ASSERT_EQ(graph->FindNodesThatMatch(Map()).size(), 1);

  auto memory_sources = graph->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(memory_sources.size(), 1);
  auto mem_src = static_cast<MemorySourceIR*>(memory_sources[0]);
  ASSERT_EQ(mem_src->Children().size(), 1);
  ASSERT_MATCH(mem_src->Children()[0], Map());

  MapIR* map = static_cast<MapIR*>(mem_src->Children()[0]);
  EXPECT_EQ(map->col_exprs().size(), 1);
  EXPECT_EQ(map->col_exprs()[0].name, "fn");
  auto expr_fn = static_cast<FuncIR*>(map->col_exprs()[0].node);
  ASSERT_EQ(expr_fn->args().size(), 2);
  ASSERT_TRUE(Match(expr_fn, Add(ColumnNode("cpu0"), Int(2)))) << expr_fn->DebugString();

  EXPECT_EQ(map->Children().size(), 1);
  // Op is a limit because of the analyzer.
  EXPECT_MATCH(map->Children()[0], Limit());
  EXPECT_EQ(map->Children()[0]->Children().size(), 5);
  EXPECT_MATCH(map->Children()[0]->Children()[0], MemorySink());
}

TEST_F(OptimizerTest, join_with_identical_mem_src) {
  // This test poses a situation where two joins might look the same, but actually have different
  // parents.
  {
    // The parents of the Join() are merge-able so should combine them together.
    auto mem_src1 = MakeMemSource("cpu", cpu_relation);
    auto mem_src2 = MakeMemSource("cpu", cpu_relation);
    auto join_both_sources = MakeJoin({mem_src1, mem_src2}, "inner", cpu_relation, cpu_relation,
                                      {"upid"}, {"upid"}, {"", "_x"});
    MakeMemSink(join_both_sources, "");
  }
  EXPECT_OK(Analyze(graph));
  EXPECT_OK(Optimize(graph));

  auto memory_sources = graph->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(memory_sources.size(), 1);
  MemorySourceIR* mem_src = static_cast<MemorySourceIR*>(memory_sources[0]);

  ASSERT_EQ(mem_src->table_name(), "cpu");
  ASSERT_EQ(mem_src->Children().size(), 2);
  EXPECT_MATCH(mem_src->Children()[0], Join());
  // Make sure we have a no-op map.
  EXPECT_MATCH(mem_src->Children()[1], Map());

  EXPECT_EQ(mem_src->Children()[0], mem_src->Children()[1]->Children()[0]);
}

TEST_F(OptimizerTest, self_union) {
  {
    // The parents of the Union() are merg-able and should be combined.
    auto mem_src1 = MakeMemSource("cpu", cpu_relation);
    auto mem_src2 = MakeMemSource("cpu", cpu_relation);
    auto mem_src3 = MakeMemSource("cpu", cpu_relation);
    auto union_op = MakeUnion({mem_src1, mem_src2, mem_src3});
    MakeMemSink(union_op, "");
  }

  EXPECT_OK(Analyze(graph));
  EXPECT_OK(Optimize(graph));

  auto memory_sources = graph->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(memory_sources.size(), 1);
  MemorySourceIR* mem_src = static_cast<MemorySourceIR*>(memory_sources[0]);

  ASSERT_EQ(mem_src->table_name(), "cpu");
  ASSERT_EQ(mem_src->Children().size(), 3);
  EXPECT_MATCH(mem_src->Children()[0], Union());
  // Should be no-op maps.
  EXPECT_MATCH(mem_src->Children()[1], Map());
  EXPECT_MATCH(mem_src->Children()[2], Map());
  EXPECT_TRUE(
      mem_src->Children()[1]->resolved_table_type()->Equals(mem_src->resolved_table_type()));
  EXPECT_TRUE(
      mem_src->Children()[2]->resolved_table_type()->Equals(mem_src->resolved_table_type()));

  EXPECT_EQ(mem_src->Children()[0], mem_src->Children()[1]->Children()[0]);
  EXPECT_EQ(mem_src->Children()[0], mem_src->Children()[2]->Children()[0]);
}

constexpr char kInnerJoinFollowedByMapQuery[] = R"pxl(
import px
src1 = px.DataFrame(table='cpu', select=['upid', 'cpu0','cpu1'])
src2 = px.DataFrame(table='network', select=['upid', 'bytes_in', 'bytes_out'])
join = src1.merge(src2, how='inner', left_on=['upid'], right_on=['upid'], suffixes=['', '_x'])
join = join[['upid', 'bytes_in', 'bytes_out', 'cpu0', 'cpu1']]
join['mb_in'] = join['bytes_in'] / 1E6
df = join[['mb_in']]
px.display(df, 'joined')
)pxl";

TEST_F(OptimizerTest, prune_unused_columns) {
  auto ir_graph_status = CompileGraph(kInnerJoinFollowedByMapQuery);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  ASSERT_OK(Analyze(ir_graph));
  ASSERT_OK(Optimize(ir_graph));

  // Check source nodes.
  auto join_nodes = ir_graph->FindNodesOfType(IRNodeType::kJoin);
  ASSERT_EQ(1, join_nodes.size());
  auto join = static_cast<JoinIR*>(join_nodes[0]);
  EXPECT_THAT(join->column_names(), ElementsAre("bytes_in"));

  auto source_nodes = join->parents();

  auto right_src = static_cast<MemorySourceIR*>(source_nodes[1]);
  ASSERT_EQ("network", right_src->table_name());
  EXPECT_THAT(right_src->column_names(), ElementsAre("upid", "bytes_in"));

  auto left_src = static_cast<MemorySourceIR*>(source_nodes[0]);
  ASSERT_EQ("cpu", left_src->table_name());
  EXPECT_THAT(left_src->column_names(), ElementsAre("upid"));

  ASSERT_EQ(join->Children().size(), 1);
  ASSERT_MATCH(join->Children()[0], Map());
  // Maps 1 and 3 are no-ops after this column pruning, but once we add
  // a rule to remove no-op maps, these will go away.
  auto map1 = static_cast<MapIR*>(join->Children()[0]);
  EXPECT_THAT(map1->resolved_table_type()->ColumnNames(), ElementsAre("bytes_in"));

  ASSERT_EQ(map1->Children().size(), 1);
  ASSERT_MATCH(map1->Children()[0], Map());
  auto map2 = static_cast<MapIR*>(map1->Children()[0]);
  EXPECT_THAT(map2->resolved_table_type()->ColumnNames(), ElementsAre("mb_in"));

  ASSERT_EQ(map2->Children().size(), 1);
  ASSERT_MATCH(map2->Children()[0], Map());
  auto map3 = static_cast<MapIR*>(map2->Children()[0]);
  EXPECT_THAT(map3->resolved_table_type()->ColumnNames(), ElementsAre("mb_in"));

  ASSERT_EQ(map3->Children().size(), 1);
  ASSERT_MATCH(map3->Children()[0], Limit());
  auto limit = static_cast<LimitIR*>(map3->Children()[0]);
  EXPECT_THAT(limit->resolved_table_type()->ColumnNames(), ElementsAre("mb_in"));

  // Check sink node
  ASSERT_EQ(limit->Children().size(), 1);
  ASSERT_MATCH(limit->Children()[0], ExternalGRPCSink());
  auto sink = static_cast<GRPCSinkIR*>(limit->Children()[0]);
  EXPECT_THAT(sink->resolved_table_type()->ColumnNames(), ElementsAre("mb_in"));
}

constexpr char kAggAfterFilterQuery[] = R"pxl(
import px

df = px.DataFrame(table='http_events', start_time='-10m')
df.http_resp_latency_ms = df.resp_latency_ns / 1.0E6
df.service = df.ctx['service']
df = df[['service', 'resp_body']]

with_error_df = df[px.contains(df.resp_body, 'DOES NOT EXIST')]

px.display(df.groupby('service').agg(count=('resp_body', px.count)), "no errors")
px.display(with_error_df.groupby('service').agg(count=('resp_body', px.count)), "with errors")
)pxl";

TEST_F(OptimizerTest, aggs_dont_merge_if_different_filter) {
  auto ir_graph_status = CompileGraph(kAggAfterFilterQuery);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  ASSERT_OK(Analyze(ir_graph));
  ASSERT_OK(Optimize(ir_graph));

  // Check agg nodes.
  auto sink_nodes = ir_graph->FindNodesThatMatch(ExternalGRPCSink());
  ASSERT_EQ(2, sink_nodes.size());

  auto sink0 = static_cast<GRPCSinkIR*>(sink_nodes[0]);
  auto sink1 = static_cast<GRPCSinkIR*>(sink_nodes[1]);

  if (sink1->name() == "no errors") {
    auto temp = sink1;
    sink1 = sink0;
    sink0 = temp;
  }

  ASSERT_EQ(sink0->name(), "no errors");
  ASSERT_EQ(sink1->name(), "with errors");

  ASSERT_NE(sink0->parents()[0], sink1->parents()[0]);
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
