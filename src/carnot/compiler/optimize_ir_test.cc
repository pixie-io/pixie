#include <gtest/gtest.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "src/carnot/compiler/optimize_ir.h"

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
  EXPECT_OK(start_time->Init(start_time_ns));
  EXPECT_OK(stop_time->Init(stop_time_ns));
  EXPECT_OK(range->Init(src, start_time, stop_time));
  EXPECT_OK(sink->Init(range, "sink"));
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

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
