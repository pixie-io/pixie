#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/ir_test_utils.h"

namespace pl {
namespace carnot {
namespace compiler {
/**
 * Creates IR Graph that is the following query compiled
 *
 * `From(table="tableName", select=["testCol"]).Range("-2m")`
 */

TEST(IRTest, check_connection) {
  auto ig = std::make_shared<IR>();
  auto src = ig->MakeNode<MemorySourceIR>().ValueOrDie();
  auto range = ig->MakeNode<RangeIR>().ValueOrDie();
  auto rng_str = ig->MakeNode<StringIR>().ValueOrDie();
  auto table_str = ig->MakeNode<StringIR>().ValueOrDie();
  auto select_col = ig->MakeNode<StringIR>().ValueOrDie();
  auto select_list = ig->MakeNode<ListIR>().ValueOrDie();
  EXPECT_TRUE(rng_str->Init("-2m").ok());
  EXPECT_TRUE(table_str->Init("tableName").ok());
  EXPECT_TRUE(select_col->Init("testCol").ok());
  EXPECT_TRUE(select_list->AddListItem(select_col).ok());
  EXPECT_TRUE(src->Init(table_str, select_list).ok());
  EXPECT_TRUE(range->Init(src, rng_str).ok());
  EXPECT_EQ(range->parent(), src);
  EXPECT_EQ(range->time_repr(), rng_str);
  EXPECT_EQ(src->table_node(), table_str);
  EXPECT_EQ(src->select(), select_list);
  EXPECT_EQ(select_list->children()[0], select_col);
  EXPECT_EQ(select_col->str(), "testCol");
  VerifyGraphConnections(ig.get());
}

TEST(IRWalker, basic_tests) {
  // Construct example IR Graph.
  auto graph = std::make_shared<IR>();

  // Create nodes.
  auto src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  auto select_list = graph->MakeNode<ListIR>().ValueOrDie();
  auto map = graph->MakeNode<MapIR>().ValueOrDie();
  auto agg = graph->MakeNode<AggIR>().ValueOrDie();
  auto sink = graph->MakeNode<MemorySinkIR>().ValueOrDie();

  // Add dependencies.
  EXPECT_OK(graph->AddEdge(src, select_list));
  EXPECT_OK(graph->AddEdge(src, map));
  EXPECT_OK(graph->AddEdge(map, agg));
  EXPECT_OK(graph->AddEdge(agg, sink));

  std::vector<int64_t> call_order;
  IRWalker()
      .OnMemorySink([&](auto* mem_sink) { call_order.push_back(mem_sink->id()); })
      .OnMemorySource([&](auto* mem_src) { call_order.push_back(mem_src->id()); })
      .OnMap([&](auto* map) { call_order.push_back(map->id()); })
      .OnAgg([&](auto* agg) { call_order.push_back(agg->id()); })
      .Walk(graph.get());
  EXPECT_EQ(std::vector<int64_t>({0, 2, 3, 4}), call_order);
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
