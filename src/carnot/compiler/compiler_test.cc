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
  name: "pl.div"
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

  auto rel_map = std::make_shared<std::unordered_map<std::string, plan::Relation>>();
  rel_map->emplace("cpu", plan::Relation(std::vector<types::DataType>(
                                             {types::DataType::FLOAT64, types::DataType::FLOAT64}),
                                         std::vector<std::string>({"cpu0", "cpu1"})));

  auto compiler_state = std::make_unique<CompilerState>(rel_map, info.get());

  auto compiler = Compiler();
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
          "mapDF = queryDF.Map(fn=lambda r : {'quotient' : r.cpu0 / r.cpu1}).Result(name='cpu2')",
      },
      "\n");
  EXPECT_OK(compiler.Compile(query, compiler_state.get()));
}

const char *kExpectedLogicalPlan = R"(
dag {
  nodes { id: 1 }
}
nodes {
  id: 1
  dag {
    nodes { sorted_deps: 4 }
    nodes { id: 4 sorted_deps: 6 }
    nodes { id: 6 sorted_deps: 9 }
    nodes { id: 9 }
  }
  nodes {
    id: 0
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "test_table"
        column_idxs: 0
        column_names: "test_col"
        column_types: FLOAT64
      }
    }
  }
  nodes {
    id: 4
    op {
      op_type: MAP_OPERATOR map_op { }
    }
  }
  nodes {
    id: 6
    op {
      op_type: BLOCKING_AGGREGATE_OPERATOR blocking_agg_op { }
    }
  }
  nodes {
    id: 9
    op {
      op_type: MEMORY_SINK_OPERATOR mem_sink_op { name: "sink" }
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
  auto col = graph->MakeNode<ColumnIR>().ValueOrDie();
  EXPECT_OK(col->Init("test_col"));
  col->SetColumnIdx(0);
  col->SetColumnType(types::DataType::FLOAT64);
  src->SetColumns(std::vector<ColumnIR *>({col}));
  auto map = graph->MakeNode<MapIR>().ValueOrDie();
  auto map_lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
  EXPECT_OK(map->Init(src, map_lambda));
  auto agg = graph->MakeNode<AggIR>().ValueOrDie();
  auto agg_by = graph->MakeNode<LambdaIR>().ValueOrDie();
  auto agg_func = graph->MakeNode<LambdaIR>().ValueOrDie();
  EXPECT_OK(agg->Init(map, agg_by, agg_func));
  auto sink = graph->MakeNode<MemorySinkIR>().ValueOrDie();
  EXPECT_OK(sink->Init(agg, "sink"));

  auto compiler = Compiler();
  carnotpb::Plan logical_plan = compiler.IRToLogicalPlan(*graph).ValueOrDie();

  carnotpb::Plan expected_logical_plan;
  ASSERT_TRUE(
      google::protobuf::TextFormat::MergeFromString(kExpectedLogicalPlan, &expected_logical_plan));
  EXPECT_TRUE(
      google::protobuf::util::MessageDifferencer::Equals(expected_logical_plan, logical_plan));
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
  EXPECT_OK(sink->Init(range, "sink"));
  EXPECT_FALSE(src->IsTimeSet());

  EXPECT_EQ(std::vector<int64_t>({0, 1, 2, 3}), graph->dag().TopologicalSort());

  // Add dependencies.
  EXPECT_OK(graph->AddEdge(src, range));
  EXPECT_OK(graph->AddEdge(range, sink));
  EXPECT_OK(graph->AddEdge(range, time));

  EXPECT_OK(Compiler::CollapseRange(graph.get()));

  EXPECT_EQ(std::vector<int64_t>({0, 3}), graph->dag().TopologicalSort());
  EXPECT_TRUE(src->IsTimeSet());
  EXPECT_EQ(7200000, src->time_stop_ms() - src->time_start_ms());
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
