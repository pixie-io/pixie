#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>

#include <unordered_map>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/compiler.h"
#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/proto/plan.pb.h"

namespace pl {
namespace carnot {
namespace compiler {

using testing::_;

const char *kExpectedUDFInfo = R"(
scalar_udfs {
  name: "pl.divide"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type: INT64
}
scalar_udfs {
  name: "pl.add"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type: FLOAT64
}
scalar_udfs {
  name: "pl.greaterThan"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type: BOOLEAN
}
scalar_udfs {
  name: "pl.lessThan"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type: BOOLEAN
}
scalar_udfs {
  name: "pl.greaterThanEqual"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type: BOOLEAN
}
scalar_udfs {
  name: "pl.lessThanEqual"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type: BOOLEAN
}
scalar_udfs {
  name: "pl.equal"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type: BOOLEAN
}
scalar_udfs {
  name: "pl.modulo"
  exec_arg_types: INT64
  exec_arg_types: INT64
  return_type: INT64
}
scalar_udfs {
  name: "pl.subtract"
  exec_arg_types: INT64
  exec_arg_types: INT64
  return_type: INT64
}
udas {
  name: "pl.mean"
  update_arg_types: FLOAT64
  finalize_type:  FLOAT64
}
udas {
  name: "pl.count"
  update_arg_types: FLOAT64
  finalize_type:  INT64
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
  EXPECT_EQ(7200000000000, src->time_stop_ns() - src->time_start_ns());
}

TEST(CompilerTest, remove_range_start_end) {
  // Construct example IR Graph.
  auto graph = std::make_shared<IR>();

  // Create nodes.
  auto src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  auto range = graph->MakeNode<RangeIR>().ValueOrDie();
  auto time = graph->MakeNode<StringIR>().ValueOrDie();
  auto sink = graph->MakeNode<MemorySinkIR>().ValueOrDie();

  EXPECT_OK(time->Init("20,50"));
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
  EXPECT_EQ(50, src->time_stop_ns());
  EXPECT_EQ(20, src->time_start_ns());
}

// Test for select order that is different than the schema.
const char *kSelectOrderLogicalPlan = R"(
dag {
  nodes { id: 1 }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 1
      sorted_deps: 0
    }
    nodes {
      id: 0
    }
  }
  nodes {
    id: 1
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "cpu"
        column_idxs: 2
        column_idxs: 0
        column_idxs: 1
        column_names: "cpu2"
        column_names: "count"
        column_names: "cpu1"
        column_types: FLOAT64
        column_types: INT64
        column_types: FLOAT64
      }
    }
  }
  nodes {
    id: 0
    op {
      op_type: MEMORY_SINK_OPERATOR mem_sink_op {
        name: "cpu_out"
        column_types: FLOAT64
        column_types: INT64
        column_types: FLOAT64
        column_names: "cpu2"
        column_names: "count"
        column_names: "cpu1"
      }
    }
  }
}
)";
TEST(CompilerTest, select_order_test) {
  auto info = std::make_shared<RegistryInfo>();
  carnotpb::UDFInfo info_pb;
  google::protobuf::TextFormat::MergeFromString(kExpectedUDFInfo, &info_pb);
  EXPECT_OK(info->Init(info_pb));

  auto rel_map = std::make_shared<std::unordered_map<std::string, plan::Relation>>();
  rel_map->emplace("cpu", plan::Relation(std::vector<types::DataType>({types::DataType::INT64,
                                                                       types::DataType::FLOAT64,
                                                                       types::DataType::FLOAT64}),
                                         std::vector<std::string>({"count", "cpu1", "cpu2"})));

  auto compiler_state = std::make_unique<CompilerState>(rel_map, info.get());
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu2', 'count', 'cpu1']).Result(name='cpu_out')",
      },
      "\n");
  auto compiler = Compiler();
  auto plan = compiler.Compile(query, compiler_state.get());
  EXPECT_OK(plan);
  carnotpb::Plan plan_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kSelectOrderLogicalPlan, &plan_pb));
  VLOG(1) << plan.ValueOrDie().DebugString();
  EXPECT_TRUE(
      google::protobuf::util::MessageDifferencer::Equals(plan_pb, plan.ConsumeValueOrDie()));
}

const char *kGroupByAllPlan = R"(
dag {
  nodes { id: 1 }
}
nodes {
  id: 1
  dag {
    nodes { id: 0 sorted_deps: 6 }
    nodes { id: 6 sorted_deps: 5 }
    nodes { id: 5 }
  }
  nodes {
    id: 0
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "cpu"
        column_idxs: 1
        column_idxs: 0
        column_names: "cpu1"
        column_names: "cpu0"
        column_types: FLOAT64
        column_types: FLOAT64
      }
    }
  }
  nodes {
    id: 6
    op {
      op_type: BLOCKING_AGGREGATE_OPERATOR
      blocking_agg_op {
        values {
          name: "pl.mean"
          args {
            column {
              index: 1
            }
          }
        }
        value_names: "mean"
      }
    }
  }
  nodes {
    id: 5
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "cpu_out"
        column_types: FLOAT64
        column_names: "mean"
      }
    }
  }
}
)";
TEST(CompilerTest, group_by_all) {
  auto info = std::make_shared<RegistryInfo>();
  carnotpb::UDFInfo info_pb;
  google::protobuf::TextFormat::MergeFromString(kExpectedUDFInfo, &info_pb);
  EXPECT_OK(info->Init(info_pb));
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu1', 'cpu0'])",
          "aggDF = queryDF.Agg(by=None, fn=lambda r : {'mean' : "
          "pl.mean(r.cpu0)}).Result(name='cpu_out')",
      },
      "\n");
  auto rel_map = std::make_shared<std::unordered_map<std::string, plan::Relation>>();
  rel_map->emplace("cpu", plan::Relation(std::vector<types::DataType>(
                                             {types::DataType::FLOAT64, types::DataType::FLOAT64}),
                                         std::vector<std::string>({"cpu0", "cpu1"})));
  auto compiler_state = std::make_unique<CompilerState>(rel_map, info.get());
  auto compiler = Compiler();
  auto plan_status = compiler.Compile(query, compiler_state.get());
  VLOG(1) << plan_status.ToString();
  // EXPECT_OK(plan_status);
  ASSERT_TRUE(plan_status.ok());
  auto logical_plan = plan_status.ConsumeValueOrDie();
  VLOG(1) << logical_plan.DebugString();
  carnotpb::Plan expected_logical_plan;
  ASSERT_TRUE(
      google::protobuf::TextFormat::MergeFromString(kGroupByAllPlan, &expected_logical_plan));
  EXPECT_TRUE(
      google::protobuf::util::MessageDifferencer::Equals(expected_logical_plan, logical_plan));
}

const char *kRangeAggPlan = R"(
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 7
      sorted_deps: 13
    }
    nodes {
      id: 13
      sorted_deps: 18
    }
    nodes {
      id: 18
      sorted_deps: 0
    }
    nodes {
    }
  }
  nodes {
    id: 7
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "cpu"
        column_idxs: 2
        column_idxs: 0
        column_idxs: 1
        column_names: "cpu2"
        column_names: "count"
        column_names: "cpu1"
        column_types: FLOAT64
        column_types: INT64
        column_types: FLOAT64
      }
    }
  }
  nodes {
    id: 13
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          func {
            name: "pl.subtract"
            args {
              column {
                node: 7
                index: 1
              }
            }
            args {
              func {
                name: "pl.modulo"
                args {
                  column {
                    node: 7
                    index: 1
                  }
                }
                args {
                  constant {
                    data_type: INT64
                    int64_value: 2
                  }
                }
              }
            }
          }
        }
        expressions {
          column {
            node: 7
            index: 2
          }
        }
        column_names: "group"
        column_names: "cpu1"
      }
    }
  }
  nodes {
    id: 18
    op {
      op_type: BLOCKING_AGGREGATE_OPERATOR
      blocking_agg_op {
        values {
          name: "pl.mean"
          args {
            column {
              node: 13
              index: 1
            }
          }
        }
        groups {
          node: 13
        }
        group_names: "group"
        value_names: "mean"
      }
    }
  }
  nodes {
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "cpu_out"
        column_types: INT64
        column_types: FLOAT64
        column_names: "group"
        column_names: "mean"
      }
    }
  }
}
)";
TEST(CompilerTest, range_agg_test) {
  auto info = std::make_shared<RegistryInfo>();
  carnotpb::UDFInfo info_pb;
  google::protobuf::TextFormat::MergeFromString(kExpectedUDFInfo, &info_pb);
  EXPECT_OK(info->Init(info_pb));

  auto rel_map = std::make_shared<std::unordered_map<std::string, plan::Relation>>();
  rel_map->emplace("cpu", plan::Relation(std::vector<types::DataType>({types::DataType::INT64,
                                                                       types::DataType::FLOAT64,
                                                                       types::DataType::FLOAT64}),
                                         std::vector<std::string>({"count", "cpu1", "cpu2"})));

  auto compiler_state = std::make_unique<CompilerState>(rel_map, info.get());
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu2', 'count', 'cpu1']).RangeAgg(by=lambda r: "
          "r.count, size=2, fn= lambda r: { 'mean': pl.mean(r.cpu1)}).Result(name='cpu_out')",
      },
      "\n");
  auto compiler = Compiler();
  auto plan = compiler.Compile(query, compiler_state.get());
  EXPECT_OK(plan);
  carnotpb::Plan plan_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kRangeAggPlan, &plan_pb));
  VLOG(1) << plan.ValueOrDie().DebugString();
  EXPECT_TRUE(
      google::protobuf::util::MessageDifferencer::Equals(plan_pb, plan.ConsumeValueOrDie()));
}

TEST(CompilerTest, multiple_group_by_agg_test) {
  auto info = std::make_shared<RegistryInfo>();
  carnotpb::UDFInfo info_pb;
  google::protobuf::TextFormat::MergeFromString(kExpectedUDFInfo, &info_pb);
  EXPECT_OK(info->Init(info_pb));

  auto rel_map = std::make_shared<std::unordered_map<std::string, plan::Relation>>();
  rel_map->emplace(
      "cpu", plan::Relation(
                 std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64,
                                               types::DataType::FLOAT64, types::DataType::FLOAT64}),
                 std::vector<std::string>({"count", "cpu0", "cpu1", "cpu2"})));
  auto compiler_state = std::make_unique<CompilerState>(rel_map, info.get());
  std::string query = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(time='-2m')",
       "aggDF = queryDF.Agg(by=lambda r : [r.cpu0, r.cpu2], fn=lambda r : {'cpu_count' : "
       "pl.count(r.cpu1), 'cpu_mean' : pl.mean(r.cpu1)}).Result(name='cpu_out')"},
      "\n");
  auto compiler = Compiler();
  auto plan = compiler.Compile(query, compiler_state.get());
  VLOG(1) << plan.ToString();
  EXPECT_OK(plan);
}

TEST(CompilerTest, multiple_group_by_map_then_agg) {
  auto info = std::make_shared<RegistryInfo>();
  carnotpb::UDFInfo info_pb;
  google::protobuf::TextFormat::MergeFromString(kExpectedUDFInfo, &info_pb);
  EXPECT_OK(info->Init(info_pb));

  auto rel_map = std::make_shared<std::unordered_map<std::string, plan::Relation>>();
  rel_map->emplace(
      "cpu", plan::Relation(
                 std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64,
                                               types::DataType::FLOAT64, types::DataType::FLOAT64}),
                 std::vector<std::string>({"count", "cpu0", "cpu1", "cpu2"})));
  auto compiler_state = std::make_unique<CompilerState>(rel_map, info.get());
  std::string query = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(time='-2m')",
       "mapDF =  queryDF.Map(fn = lambda r : {'cpu0' : r.cpu0, 'cpu1' : r.cpu1, 'cpu2' : r.cpu2, "
       "'cpu_sum' : r.cpu0+r.cpu1+r.cpu2})",
       "aggDF = mapDF.Agg(by=lambda r : [r.cpu0, r.cpu2], fn=lambda r : {'cpu_count' : "
       "pl.count(r.cpu1), 'cpu_mean' : pl.mean(r.cpu1)}).Result(name='cpu_out')"},
      "\n");
  auto compiler = Compiler();
  auto plan = compiler.Compile(query, compiler_state.get());
  VLOG(1) << plan.ToString();
  EXPECT_OK(plan);
}
TEST(CompilerTest, rename_then_group_by_test) {
  auto info = std::make_shared<RegistryInfo>();
  carnotpb::UDFInfo info_pb;
  google::protobuf::TextFormat::MergeFromString(kExpectedUDFInfo, &info_pb);
  EXPECT_OK(info->Init(info_pb));

  auto rel_map = std::make_shared<std::unordered_map<std::string, plan::Relation>>();
  rel_map->emplace("sequences",
                   plan::Relation(std::vector<types::DataType>({
                                      types::DataType::TIME64NS,
                                      types::DataType::FLOAT64,
                                      types::DataType::FLOAT64,
                                  }),
                                  std::vector<std::string>({"_time", "xmod10", "PIx"})));
  auto compiler_state = std::make_unique<CompilerState>(rel_map, info.get());
  auto query = absl::StrJoin(
      {"queryDF = From(table='sequences', select=['_time', 'xmod10', 'PIx'])",
       "map_out = queryDF.Map(fn=lambda r : {'res': r.PIx, 'c1': r.xmod10})",
       "agg_out = map_out.Agg(by=lambda r: [r.res, r.c1], fn=lambda r: {'count': pl.count(r.c1)})",
       "agg_out.Result(name='t15')"},
      "\n");
  auto compiler = Compiler();
  auto plan = compiler.Compile(query, compiler_state.get());
  VLOG(1) << plan.ToString();
  EXPECT_OK(plan);
}

// Test to see whether comparisons work.
TEST(CompilerTest, comparison_test) {
  auto info = std::make_shared<RegistryInfo>();
  carnotpb::UDFInfo info_pb;
  google::protobuf::TextFormat::MergeFromString(kExpectedUDFInfo, &info_pb);
  EXPECT_OK(info->Init(info_pb));

  auto rel_map = std::make_shared<std::unordered_map<std::string, plan::Relation>>();
  rel_map->emplace("sequences",
                   plan::Relation(std::vector<types::DataType>({
                                      types::DataType::TIME64NS,
                                      types::DataType::FLOAT64,
                                      types::DataType::FLOAT64,
                                  }),
                                  std::vector<std::string>({"_time", "xmod10", "PIx"})));

  auto compiler_state = std::make_unique<CompilerState>(rel_map, info.get());
  auto query =
      absl::StrJoin({"queryDF = From(table='sequences', select=['_time', 'xmod10', 'PIx'])",
                     "map_out = queryDF.Map(fn=lambda r : {'res': r.PIx, "
                     "'c1': r.xmod10, 'gt' : r.xmod10 > 10.0,'lt' : r.xmod10 < 10.0,",
                     "'gte' : r.PIx >= 1.0, 'lte' : r.PIx <= 1.0,", "'eq' : r.PIx == 1.0})",
                     "map_out.Result(name='t15')"},
                    "\n");
  auto compiler = Compiler();
  auto plan = compiler.Compile(query, compiler_state.get());
  VLOG(1) << plan.ToString();
  EXPECT_OK(plan);
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
