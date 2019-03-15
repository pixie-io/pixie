#include <gtest/gtest.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "src/carnot/compiler/optimize_ir.h"
#include "src/carnot/compiler/test_utils.h"

namespace pl {
namespace carnot {
namespace compiler {

TEST(CompilerTest, remove_range) {
  // Construct example IR Graph.
  auto graph = std::make_shared<IR>();

  // Create nodes.
  auto src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  auto range = graph->MakeNode<RangeIR>().ValueOrDie();
  auto start_time = graph->MakeNode<IntIR>().ValueOrDie();
  auto stop_time = graph->MakeNode<IntIR>().ValueOrDie();
  auto sink = graph->MakeNode<MemorySinkIR>().ValueOrDie();
  int64_t start_time_ns = 2;
  int64_t stop_time_ns = 4;
  auto ast = MakeTestAstPtr();
  EXPECT_OK(start_time->Init(start_time_ns, ast));
  EXPECT_OK(stop_time->Init(stop_time_ns, ast));
  EXPECT_OK(range->Init(src, start_time, stop_time, ast));
  EXPECT_OK(sink->Init(range, "sink", ast));
  EXPECT_FALSE(src->IsTimeSet());

  EXPECT_EQ(std::vector<int64_t>({0, 1, 2, 3, 4}), graph->dag().TopologicalSort());

  // Add dependencies.
  EXPECT_OK(graph->AddEdge(src, range));
  EXPECT_OK(graph->AddEdge(range, sink));
  EXPECT_OK(graph->AddEdge(range, start_time));
  EXPECT_OK(graph->AddEdge(range, stop_time));

  EXPECT_OK(IROptimizer().Optimize(graph.get()));
  // checks to make sure that all the edges related to range are removed.
  EXPECT_EQ(std::vector<int64_t>({0, 4}), graph->dag().TopologicalSort());
  EXPECT_TRUE(src->IsTimeSet());
  EXPECT_EQ(stop_time_ns - start_time_ns, src->time_stop_ns() - src->time_start_ns());
  EXPECT_EQ(stop_time_ns, src->time_stop_ns());
  EXPECT_EQ(start_time_ns, src->time_start_ns());
}

TEST(CompileThenOptimize, query_test_start_stop) {
  int64_t start_hours = 2;
  int64_t stop_hours = 10;
  std::string query =
      "From(table='test_table', select=['time_','col1']).Range(start=$0, "
      "stop=$1).Result(name='out')";
  auto ir_graph_status = ParseQuery(absl::Substitute(query, start_hours, stop_hours));
  ASSERT_TRUE(ir_graph_status.ok());
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  EXPECT_OK(IROptimizer().Optimize(ir_graph.get()));
  IRNode* node;
  for (const auto idx : ir_graph->dag().TopologicalSort()) {
    node = ir_graph->Get(idx);
    if (node->type() == IRNodeType::MemorySourceType) {
      break;
    }
  }

  ASSERT_NE(nullptr, node);
  MemorySourceIR* src = static_cast<MemorySourceIR*>(node);

  EXPECT_EQ(stop_hours - start_hours, src->time_stop_ns() - src->time_start_ns());
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
