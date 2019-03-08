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
  auto time = graph->MakeNode<StringIR>().ValueOrDie();
  auto sink = graph->MakeNode<MemorySinkIR>().ValueOrDie();
  int64_t num_hours = 2;
  EXPECT_OK(time->Init(absl::StrFormat("-%dh", num_hours)));
  EXPECT_OK(range->Init(src, time));
  EXPECT_OK(sink->Init(range, "sink"));
  EXPECT_FALSE(src->IsTimeSet());

  EXPECT_EQ(std::vector<int64_t>({0, 1, 2, 3}), graph->dag().TopologicalSort());

  // Add dependencies.
  EXPECT_OK(graph->AddEdge(src, range));
  EXPECT_OK(graph->AddEdge(range, sink));
  EXPECT_OK(graph->AddEdge(range, time));

  EXPECT_OK(IROptimizer().Optimize(graph.get()));

  EXPECT_EQ(std::vector<int64_t>({0, 3}), graph->dag().TopologicalSort());
  EXPECT_TRUE(src->IsTimeSet());
  std::chrono::nanoseconds ns = std::chrono::hours(num_hours);
  EXPECT_EQ(ns.count(), src->time_stop_ns() - src->time_start_ns());
}

TEST(CompilerTest, remove_range_start_end) {
  // Construct example IR Graph.
  auto graph = std::make_shared<IR>();

  // Create nodes.
  auto src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  auto range = graph->MakeNode<RangeIR>().ValueOrDie();
  auto time = graph->MakeNode<StringIR>().ValueOrDie();
  auto sink = graph->MakeNode<MemorySinkIR>().ValueOrDie();
  int64_t stop_time = 50;
  int64_t start_time = 20;
  EXPECT_OK(time->Init(absl::Substitute("$0,$1", start_time, stop_time)));
  EXPECT_OK(range->Init(src, time));
  EXPECT_OK(sink->Init(range, "sink"));
  EXPECT_FALSE(src->IsTimeSet());

  EXPECT_EQ(std::vector<int64_t>({0, 1, 2, 3}), graph->dag().TopologicalSort());

  // Add dependencies.
  EXPECT_OK(graph->AddEdge(src, range));
  EXPECT_OK(graph->AddEdge(range, sink));
  EXPECT_OK(graph->AddEdge(range, time));

  EXPECT_OK(IROptimizer().Optimize(graph.get()));

  EXPECT_EQ(std::vector<int64_t>({0, 3}), graph->dag().TopologicalSort());
  EXPECT_TRUE(src->IsTimeSet());
  EXPECT_EQ(stop_time, src->time_stop_ns());
  EXPECT_EQ(start_time, src->time_start_ns());
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
