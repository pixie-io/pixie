#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <unordered_map>
#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/analyzer.h"
#include "src/carnot/planner/compiler/optimizer/filter_push_down.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"

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

class FilterPushDownTest : public OperatorTests {
 protected:
  void SetUpRegistryInfo() { info_ = udfexporter::ExportUDFInfo().ConsumeValueOrDie(); }
  void SetUpImpl() override {
    SetUpRegistryInfo();

    auto rel_map = std::make_unique<RelationMap>();
    cpu_relation = table_store::schema::Relation(
        std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64,
                                      types::DataType::FLOAT64, types::DataType::FLOAT64}),
        std::vector<std::string>({"count", "cpu0", "cpu1", "cpu2"}));
    rel_map->emplace("cpu", cpu_relation);

    compiler_state_ =
        std::make_unique<CompilerState>(std::move(rel_map), info_.get(), time_now, "result_addr");
    md_handler = MetadataHandler::Create();
  }
  FilterIR* MakeFilter(OperatorIR* parent) {
    auto constant1 = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
    auto column = MakeColumn("column", 0);

    auto filter_func = graph
                           ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::eq, "==", "equals"},
                                                std::vector<ExpressionIR*>{constant1, column})
                           .ValueOrDie();
    filter_func->SetOutputDataType(types::DataType::BOOLEAN);

    return graph->CreateNode<FilterIR>(ast, parent, filter_func).ValueOrDie();
  }
  FilterIR* MakeFilter(OperatorIR* parent, ColumnIR* filter_value) {
    auto constant1 = graph->CreateNode<StringIR>(ast, "value").ValueOrDie();
    FuncIR* filter_func =
        graph
            ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::eq, "==", "equals"},
                                 std::vector<ExpressionIR*>{constant1, filter_value})
            .ValueOrDie();
    return graph->CreateNode<FilterIR>(ast, parent, filter_func).ValueOrDie();
  }
  FilterIR* MakeFilter(OperatorIR* parent, FuncIR* filter_expr) {
    return graph->CreateNode<FilterIR>(ast, parent, filter_expr).ValueOrDie();
  }
  using OperatorTests::MakeBlockingAgg;
  BlockingAggIR* MakeBlockingAgg(OperatorIR* parent, ColumnIR* by_column, ColumnIR* fn_column) {
    auto agg_func = graph
                        ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "mean"},
                                             std::vector<ExpressionIR*>{fn_column})
                        .ValueOrDie();
    return graph
        ->CreateNode<BlockingAggIR>(ast, parent, std::vector<ColumnIR*>{by_column},
                                    ColExpressionVector{{"agg_fn", agg_func}})
        .ValueOrDie();
  }

  void TearDown() override {
    if (skip_check_stray_nodes_) {
      return;
    }
    CleanUpStrayIRNodesRule cleanup;
    auto before = graph->DebugString();
    auto result = cleanup.Execute(graph.get());
    ASSERT_OK(result);
    ASSERT_FALSE(result.ConsumeValueOrDie())
        << "Rule left stray non-Operator IRNodes in graph: " << before;
  }

  // skip_check_stray_nodes_ should only be set to 'true' for tests of rules when they return an
  // error.
  bool skip_check_stray_nodes_ = false;
  std::unique_ptr<CompilerState> compiler_state_;
  std::unique_ptr<RegistryInfo> info_;
  int64_t time_now = 1552607213931245000;
  table_store::schema::Relation cpu_relation;
  std::unique_ptr<MetadataHandler> md_handler;
};

TEST_F(FilterPushDownTest, simple_no_op) {
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource(relation);
  FilterIR* filter = MakeFilter(src);
  MakeMemSink(filter, "foo", {});

  FilterPushdownRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
}

TEST_F(FilterPushDownTest, simple) {
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource(relation);
  MapIR* map =
      MakeMap(src, {{"abc_1", MakeColumn("abc", 0)}, {"abc", MakeColumn("abc", 0)}}, false);
  FilterIR* filter = MakeFilter(map, MakeEqualsFunc(MakeColumn("abc", 0), MakeInt(2)));
  MemorySinkIR* sink = MakeMemSink(filter, "foo", {});

  FilterPushdownRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  EXPECT_THAT(sink->parents(), ElementsAre(map));
  EXPECT_THAT(map->parents(), ElementsAre(filter));
  EXPECT_THAT(filter->parents(), ElementsAre(src));
  EXPECT_MATCH(filter->filter_expr(), Equals(ColumnNode("abc"), Int(2)));
}

TEST_F(FilterPushDownTest, two_col_filter) {
  Relation relation({types::DataType::INT64}, {"abc"});
  MemorySourceIR* src = MakeMemSource(relation);
  MapIR* map1 = MakeMap(src, {{"abc", MakeColumn("abc", 0)}}, false);
  MapIR* map2 = MakeMap(map1, {{"xyz", MakeInt(3)}, {"abc", MakeColumn("abc", 0)}}, false);
  MapIR* map3 =
      MakeMap(map2, {{"xyz", MakeColumn("xyz", 0)}, {"abc", MakeColumn("abc", 0)}}, false);
  MapIR* map4 =
      MakeMap(map3, {{"xyz", MakeColumn("xyz", 0)}, {"abc", MakeColumn("abc", 0)}}, false);
  FilterIR* filter = MakeFilter(map4, MakeEqualsFunc(MakeColumn("abc", 0), MakeColumn("xyz", 0)));
  MemorySinkIR* sink = MakeMemSink(filter, "foo", {});

  FilterPushdownRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  EXPECT_THAT(sink->parents(), ElementsAre(map4));
  EXPECT_THAT(map4->parents(), ElementsAre(map3));
  EXPECT_THAT(map3->parents(), ElementsAre(filter));
  EXPECT_THAT(filter->parents(), ElementsAre(map2));
  EXPECT_THAT(map2->parents(), ElementsAre(map1));
  EXPECT_THAT(map1->parents(), ElementsAre(src));
  EXPECT_MATCH(filter->filter_expr(), Equals(ColumnNode("abc"), ColumnNode("xyz")));
}

TEST_F(FilterPushDownTest, multi_condition_filter) {
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource(relation);
  MapIR* map1 = MakeMap(src, {{"abc", MakeColumn("abc", 0)}}, false);
  MapIR* map2 = MakeMap(map1, {{"xyz", MakeInt(3)}, {"abc", MakeColumn("abc", 0)}}, false);
  MapIR* map3 =
      MakeMap(map2, {{"xyz", MakeColumn("xyz", 0)}, {"abc", MakeColumn("abc", 0)}}, false);
  FilterIR* filter =
      MakeFilter(map3, MakeAndFunc(MakeEqualsFunc(MakeColumn("abc", 0), MakeInt(2)),
                                   MakeEqualsFunc(MakeColumn("xyz", 0), MakeInt(3))));
  MemorySinkIR* sink = MakeMemSink(filter, "foo", {});

  FilterPushdownRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  EXPECT_THAT(sink->parents(), ElementsAre(map3));
  EXPECT_THAT(map3->parents(), ElementsAre(filter));
  EXPECT_THAT(filter->parents(), ElementsAre(map2));
  EXPECT_THAT(map2->parents(), ElementsAre(map1));
  EXPECT_THAT(map1->parents(), ElementsAre(src));
  EXPECT_MATCH(filter->filter_expr(),
               LogicalAnd(Equals(ColumnNode("abc"), Int(2)), Equals(ColumnNode("xyz"), Int(3))));
}

TEST_F(FilterPushDownTest, column_rename) {
  // abc -> def
  // filter on def
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource(relation);
  MapIR* map1 = MakeMap(src, {{"def", MakeColumn("abc", 0)}}, false);
  MapIR* map2 = MakeMap(map1, {{"xyz", MakeInt(3)}, {"def", MakeColumn("def", 0)}}, false);
  FilterIR* filter = MakeFilter(map2, MakeEqualsFunc(MakeColumn("def", 0), MakeInt(2)));
  MemorySinkIR* sink = MakeMemSink(filter, "foo", {});

  FilterPushdownRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  EXPECT_THAT(sink->parents(), ElementsAre(map2));
  EXPECT_THAT(map2->parents(), ElementsAre(map1));
  EXPECT_THAT(map1->parents(), ElementsAre(filter));
  EXPECT_THAT(filter->parents(), ElementsAre(src));
  EXPECT_MATCH(filter->filter_expr(), Equals(ColumnNode("abc"), Int(2)));
}

TEST_F(FilterPushDownTest, two_filters_different_cols) {
  // create abc
  // create def
  // create ghi
  // filter on def
  // filter on abc
  Relation relation({types::DataType::INT64}, {"abc"});
  MemorySourceIR* src = MakeMemSource(relation);
  MapIR* map1 = MakeMap(src, {{"abc", MakeColumn("abc", 0)}, {"def", MakeInt(2)}}, false);
  MapIR* map2 = MakeMap(
      map1, {{"abc", MakeColumn("abc", 0)}, {"def", MakeColumn("def", 0)}, {"ghi", MakeInt(2)}},
      false);
  FilterIR* filter1 = MakeFilter(map2, MakeEqualsFunc(MakeColumn("def", 0), MakeInt(2)));
  FilterIR* filter2 = MakeFilter(filter1, MakeEqualsFunc(MakeColumn("abc", 0), MakeInt(3)));
  MemorySinkIR* sink = MakeMemSink(filter2, "foo", {});

  FilterPushdownRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  EXPECT_THAT(sink->parents(), ElementsAre(map2));
  EXPECT_THAT(map2->parents(), ElementsAre(filter1));
  EXPECT_THAT(filter1->parents(), ElementsAre(map1));
  EXPECT_THAT(map1->parents(), ElementsAre(filter2));
  EXPECT_THAT(filter2->parents(), ElementsAre(src));

  EXPECT_MATCH(filter1->filter_expr(), Equals(ColumnNode("def"), Int(2)));
  EXPECT_MATCH(filter2->filter_expr(), Equals(ColumnNode("abc"), Int(3)));
}

TEST_F(FilterPushDownTest, two_filters_same_cols) {
  // create def
  // filter on def
  // create ghi
  // filter on def again
  Relation relation({types::DataType::INT64}, {"abc"});
  MemorySourceIR* src = MakeMemSource(relation);
  MapIR* map1 = MakeMap(src, {{"abc", MakeColumn("abc", 0)}, {"def", MakeInt(2)}}, false);
  MapIR* map2 =
      MakeMap(map1, {{"abc", MakeColumn("abc", 0)}, {"def", MakeColumn("def", 0)}}, false);
  FilterIR* filter1 = MakeFilter(map2, MakeEqualsFunc(MakeColumn("def", 0), MakeInt(2)));
  MapIR* map3 = MakeMap(
      filter1, {{"abc", MakeColumn("abc", 0)}, {"def", MakeColumn("def", 0)}, {"ghi", MakeInt(2)}},
      false);
  FilterIR* filter2 = MakeFilter(map3, MakeEqualsFunc(MakeInt(3), MakeColumn("def", 0)));
  MemorySinkIR* sink = MakeMemSink(filter2, "foo", {});

  FilterPushdownRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  EXPECT_THAT(sink->parents(), ElementsAre(map3));
  EXPECT_THAT(map3->parents(), ElementsAre(map2));
  EXPECT_THAT(map2->parents(), ElementsAre(filter1));
  EXPECT_THAT(filter1->parents(), ElementsAre(filter2));
  EXPECT_THAT(filter2->parents(), ElementsAre(map1));
  EXPECT_THAT(map1->parents(), ElementsAre(src));

  EXPECT_MATCH(filter1->filter_expr(), Equals(ColumnNode("def"), Int(2)));
  EXPECT_MATCH(filter2->filter_expr(), Equals(ColumnNode("def"), Int(3)));
}

TEST_F(FilterPushDownTest, single_col_rename_collision) {
  // 0: abc, def
  // 1: abc->def, drop first def, xyz->2
  // 2: abc=xyz, def=def
  // 3: filter on def (bool col) becomes filter on abc at position 1.
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "def"});
  MemorySourceIR* src = MakeMemSource(relation);
  MapIR* map1 = MakeMap(src, {{"def", MakeColumn("abc", 0)}, {"xyz", MakeInt(2)}}, false);
  MapIR* map2 =
      MakeMap(map1, {{"def", MakeColumn("def", 0)}, {"abc", MakeColumn("xyz", 0)}}, false);
  FilterIR* filter = graph->CreateNode<FilterIR>(ast, map2, MakeColumn("def", 0)).ValueOrDie();
  MemorySinkIR* sink = MakeMemSink(filter, "foo", {});

  FilterPushdownRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  EXPECT_THAT(sink->parents(), ElementsAre(map2));
  EXPECT_THAT(map2->parents(), ElementsAre(map1));
  EXPECT_THAT(map1->parents(), ElementsAre(filter));
  EXPECT_THAT(filter->parents(), ElementsAre(src));

  // Make sure the former name of the filter column gets used.
  EXPECT_MATCH(filter->filter_expr(), ColumnNode("abc"));
}

TEST_F(FilterPushDownTest, single_col_rename_collision_swap) {
  // abc -> xyz, xyz -> abc
  // filter on abc
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource(relation);
  MapIR* map = MakeMap(src, {{"xyz", MakeColumn("abc", 0)}, {"abc", MakeColumn("xyz", 0)}}, false);
  FilterIR* filter = MakeFilter(map, MakeEqualsFunc(MakeColumn("abc", 0), MakeInt(2)));
  MemorySinkIR* sink = MakeMemSink(filter, "foo", {});

  FilterPushdownRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  EXPECT_THAT(sink->parents(), ElementsAre(map));
  EXPECT_THAT(map->parents(), ElementsAre(filter));
  EXPECT_THAT(filter->parents(), ElementsAre(src));
  EXPECT_MATCH(filter->filter_expr(), Equals(ColumnNode("xyz"), Int(2)));
}

TEST_F(FilterPushDownTest, multicol_rename_collision) {
  // abc -> def, def -> abc
  // abc -> def, def -> abc
  // filter on abc
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource(relation);
  MapIR* map1 = MakeMap(src, {{"xyz", MakeColumn("abc", 0)}, {"abc", MakeColumn("xyz", 0)}}, false);
  MapIR* map2 =
      MakeMap(map1, {{"xyz", MakeColumn("abc", 0)}, {"abc", MakeColumn("xyz", 0)}}, false);
  FilterIR* filter = MakeFilter(map2, MakeEqualsFunc(MakeColumn("abc", 0), MakeInt(2)));
  MemorySinkIR* sink = MakeMemSink(filter, "foo", {});

  FilterPushdownRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_THAT(sink->parents(), ElementsAre(map2));
  EXPECT_THAT(map2->parents(), ElementsAre(map1));
  EXPECT_THAT(map1->parents(), ElementsAre(filter));
  EXPECT_THAT(filter->parents(), ElementsAre(src));

  EXPECT_MATCH(filter->filter_expr(), Equals(ColumnNode("abc"), Int(2)));
}

TEST_F(FilterPushDownTest, agg_group) {
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource(relation);
  BlockingAggIR* agg =
      MakeBlockingAgg(src, {MakeColumn("abc", 0)}, {{"out", MakeMeanFunc(MakeColumn("xyz", 0))}});
  FilterIR* filter = MakeFilter(agg, MakeEqualsFunc(MakeColumn("abc", 0), MakeInt(2)));
  MemorySinkIR* sink = MakeMemSink(filter, "");

  FilterPushdownRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_THAT(sink->parents(), ElementsAre(agg));
  EXPECT_THAT(agg->parents(), ElementsAre(filter));
  EXPECT_THAT(filter->parents(), ElementsAre(src));
  EXPECT_MATCH(filter->filter_expr(), Equals(ColumnNode("abc"), Int(2)));
}

TEST_F(FilterPushDownTest, agg_expr_no_push) {
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource(relation);
  BlockingAggIR* agg =
      MakeBlockingAgg(src, {MakeColumn("abc", 0)}, {{"xyz", MakeMeanFunc(MakeColumn("xyz", 0))}});
  FilterIR* filter = MakeFilter(agg, MakeEqualsFunc(MakeColumn("xyz", 0), MakeColumn("abc", 0)));
  MakeMemSink(filter, "");

  FilterPushdownRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
}

TEST_F(FilterPushDownTest, multiple_children_dont_push) {
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource(relation);
  BlockingAggIR* agg =
      MakeBlockingAgg(src, {MakeColumn("abc", 0)}, {{"out", MakeMeanFunc(MakeColumn("xyz", 0))}});
  FilterIR* filter = MakeFilter(agg, MakeEqualsFunc(MakeColumn("abc", 0), MakeInt(2)));
  MakeMemSink(filter, "");
  MakeMemSink(agg, "2");

  FilterPushdownRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());

  // Don't push it anywhere.
  EXPECT_MATCH(filter->Children()[0], MemorySink());
  EXPECT_MATCH(filter->parents()[0], BlockingAgg());
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
