#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <pypa/parser/parser.hh>

#include <unordered_map>
#include <vector>

#include "src/carnot/compiler/compiler.h"
#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/proto/plan.pb.h"

namespace pl {
namespace carnot {
namespace compiler {

using testing::_;

const char *kExpectedUDFInfo = R"(
scalar_udfs {
  name: "divide"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type: INT64
}
)";

TEST(CompilerTest, basic) {
  auto info = std::make_shared<RegistryInfo>();
  carnotpb::UDFInfo info_pb;
  google::protobuf::TextFormat::MergeFromString(kExpectedUDFInfo, &info_pb);
  EXPECT_OK(info->Init(info_pb));

  auto lookup_map =
      std::unordered_map<std::string, std::unordered_map<std::string, types::DataType>>();
  auto cpu_lookup = std::unordered_map<std::string, types::DataType>();
  cpu_lookup.emplace("cpu0", types::DataType::FLOAT64);
  cpu_lookup.emplace("cpu1", types::DataType::FLOAT64);
  lookup_map.emplace("cpu", cpu_lookup);

  auto compiler_state = std::make_unique<CompilerState>(lookup_map, info.get());

  auto compiler = Compiler();
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "mapDF = queryDF.Map(fn=lambda r : {'quotient' : r.cpu0 / r.cpu1}).Result()",
      },
      "\n");
  EXPECT_OK(compiler.Compile(query, compiler_state.get()));
}

TEST(CompilerTest, remove_range) {
  // Construct example IR Graph.
  auto graph = std::make_shared<IR>();

  // Create nodes.
  auto src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  auto range = graph->MakeNode<RangeIR>().ValueOrDie();
  auto time = graph->MakeNode<StringIR>().ValueOrDie();
  auto sink = graph->MakeNode<MemorySinkIR>().ValueOrDie();

  EXPECT_OK(time->Init("-2h"));
  EXPECT_OK(range->Init(src, time));
  EXPECT_OK(sink->Init(range));
  EXPECT_FALSE(src->IsTimeSet());

  EXPECT_EQ(std::vector<int64_t>({0, 1, 2, 3}), graph->dag().TopologicalSort());

  // Add dependencies.
  EXPECT_OK(graph->AddEdge(src, range));
  EXPECT_OK(graph->AddEdge(range, sink));
  EXPECT_OK(graph->AddEdge(range, time));

  EXPECT_OK(Compiler::CollapseRange(graph.get()));

  EXPECT_EQ(std::vector<int64_t>({0, 3}), graph->dag().TopologicalSort());
  EXPECT_TRUE(src->IsTimeSet());
}

const char *kExpectedLogicalPlan = R"(
dag {
  nodes { id: 1 }
}
nodes {
  id: 1
  dag {
    nodes { sorted_deps: 3 }
    nodes { id: 3 sorted_deps: 5 }
    nodes { id: 5 sorted_deps: 8 }
    nodes { id: 8 }
  }
  nodes {
    id: 0
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op { name: "test_table" }
    }
  }
  nodes {
    id: 3
    op {
      op_type: MAP_OPERATOR map_op { }
    }
  }
  nodes {
    id: 5
    op {
      op_type: BLOCKING_AGGREGATE_OPERATOR blocking_agg_op { }
    }
  }
  nodes {
    id: 8
    op {
      op_type: MEMORY_SINK_OPERATOR mem_sink_op { }
    }
  }
}
)";

TEST(CompilerTest, to_logical_plan) {
  // Construct example IR Graph.
  auto graph = std::make_shared<IR>();

  // Create nodes.
  auto src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  auto table_node = graph->MakeNode<StringIR>().ValueOrDie();
  auto select = graph->MakeNode<ListIR>().ValueOrDie();
  EXPECT_OK(table_node->Init("test_table"));
  EXPECT_OK(src->Init(table_node, select));
  auto map = graph->MakeNode<MapIR>().ValueOrDie();
  auto map_lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
  EXPECT_OK(map->Init(src, map_lambda));
  auto agg = graph->MakeNode<AggIR>().ValueOrDie();
  auto agg_by = graph->MakeNode<LambdaIR>().ValueOrDie();
  auto agg_func = graph->MakeNode<LambdaIR>().ValueOrDie();
  EXPECT_OK(agg->Init(map, agg_by, agg_func));
  auto sink = graph->MakeNode<MemorySinkIR>().ValueOrDie();
  EXPECT_OK(sink->Init(agg));

  auto compiler = Compiler();
  carnotpb::Plan logical_plan = compiler.IRToLogicalPlan(*graph).ValueOrDie();

  carnotpb::Plan expected_logical_plan;
  ASSERT_TRUE(
      google::protobuf::TextFormat::MergeFromString(kExpectedLogicalPlan, &expected_logical_plan));
  EXPECT_TRUE(
      google::protobuf::util::MessageDifferencer::Equals(expected_logical_plan, logical_plan));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
