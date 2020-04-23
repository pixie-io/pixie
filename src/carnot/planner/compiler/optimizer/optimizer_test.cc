#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <unordered_map>
#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/analyzer.h"
#include "src/carnot/planner/compiler/optimizer/optimizer.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/parser/parser.h"

namespace pl {
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
    PL_ASSIGN_OR_RETURN(std::unique_ptr<Analyzer> analyzer,
                        Analyzer::Create(compiler_state_.get()));
    return analyzer->Execute(ir_graph.get());
  }
  Status Optimize(std::shared_ptr<IR> ir_graph) {
    PL_ASSIGN_OR_RETURN(std::unique_ptr<Optimizer> optimizer,
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

  EXPECT_OK(Optimize(graph));

  auto memory_sources = graph->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(memory_sources.size(), 1);
  auto mem_src = static_cast<MemorySourceIR*>(memory_sources[0]);
  ASSERT_EQ(mem_src->Children().size(), 5);
  for (auto ch : mem_src->Children()) {
    ASSERT_MATCH(ch, MemorySink());
  }
}

TEST_F(OptimizerTest, mem_src_different_columns_test) {
  {
    auto mem_src = MakeMemSource("cpu", cpu_relation, {"upid", "cpu0"});
    MakeMemSink(mem_src, "");
  }

  {
    auto mem_src = MakeMemSource("cpu", cpu_relation, {"upid", "cpu1"});
    MakeMemSink(mem_src, "");
  }

  EXPECT_OK(Optimize(graph));

  auto memory_sources = graph->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(memory_sources.size(), 1);
  auto mem_src = static_cast<MemorySourceIR*>(memory_sources[0]);
  EXPECT_THAT(mem_src->column_names(), UnorderedElementsAre("upid", "cpu0", "cpu1"));
  ASSERT_EQ(mem_src->Children().size(), 2);
  for (auto ch : mem_src->Children()) {
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
    parent_fn->SetOutputDataType(types::FLOAT64);
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
    agg_fn->SetOutputDataType(types::FLOAT64);
    auto column = MakeColumn("upid", 0);
    column->ResolveColumnType(types::UINT128);
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
    auto mem_src = MakeMemSource("cpu", cpu_relation);
    auto filter_expr = MakeEqualsFunc(MakeColumn("cpu0", 0), MakeInt(23));
    filter_expr->SetOutputDataType(types::BOOLEAN);
    auto filter = MakeFilter(mem_src, filter_expr);
    MakeMemSink(filter, "");
  }
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
  EXPECT_MATCH(filter->Children()[0], MemorySink());
}

TEST_F(OptimizerTest, mem_src_different_filter_test) {
  // Test to make sure we can remove duplicated graphs.
  {
    // Graph 0.
    auto mem_src = MakeMemSource("cpu", cpu_relation);
    auto filter_expr = MakeEqualsFunc(MakeColumn("cpu0", 0), MakeInt(23));
    filter_expr->SetOutputDataType(types::BOOLEAN);
    auto filter = MakeFilter(mem_src, filter_expr);
    MakeMemSink(filter, "");
  }
  {
    // Graph 1.
    auto mem_src = MakeMemSource("cpu", cpu_relation);
    auto equals_fn1 = MakeEqualsFunc(MakeColumn("cpu0", 0), MakeInt(23));
    auto equals_fn2 = MakeEqualsFunc(MakeColumn("cpu0", 0), MakeInt(46));
    auto filter_expr = MakeOrFunc(equals_fn1, equals_fn2);
    filter_expr->SetOutputDataType(types::BOOLEAN);
    auto filter = MakeFilter(mem_src, filter_expr);
    MakeMemSink(filter, "");
  }
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
  EXPECT_MATCH(filter0->Children()[0], MemorySink());
  EXPECT_EQ(filter1->Children().size(), 1);
  EXPECT_MATCH(filter1->Children()[0], MemorySink());
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
  EXPECT_EQ(mem_src->Children()[1]->relation(), mem_src->relation());
  EXPECT_EQ(mem_src->Children()[2]->relation(), mem_src->relation());

  EXPECT_EQ(mem_src->Children()[0], mem_src->Children()[1]->Children()[0]);
  EXPECT_EQ(mem_src->Children()[0], mem_src->Children()[2]->Children()[0]);
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
