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

const char* kExpectedUDFInfo = R"(
scalar_udfs {
  name: "pl.divide"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type: FLOAT64
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
udas {
  name: "pl.count"
  finalize_type:  INT64
}
)";

class CompilerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    info_ = std::make_shared<RegistryInfo>();
    carnotpb::UDFInfo info_pb;
    google::protobuf::TextFormat::MergeFromString(kExpectedUDFInfo, &info_pb);
    EXPECT_OK(info_->Init(info_pb));

    auto rel_map = std::make_shared<std::unordered_map<std::string, plan::Relation>>();
    rel_map->emplace("sequences",
                     plan::Relation(std::vector<types::DataType>({
                                        types::DataType::TIME64NS,
                                        types::DataType::FLOAT64,
                                        types::DataType::FLOAT64,
                                    }),
                                    std::vector<std::string>({"_time", "xmod10", "PIx"})));

    rel_map->emplace("cpu",
                     plan::Relation(std::vector<types::DataType>(
                                        {types::DataType::INT64, types::DataType::FLOAT64,
                                         types::DataType::FLOAT64, types::DataType::FLOAT64}),
                                    std::vector<std::string>({"count", "cpu0", "cpu1", "cpu2"})));
    compiler_state_ = std::make_unique<CompilerState>(rel_map, info_.get());
  }
  std::unique_ptr<CompilerState> compiler_state_;
  std::shared_ptr<RegistryInfo> info_;
};

const char* kExpectedLogicalPlan = R"(
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      sorted_deps: 5
    }
    nodes {
      id: 5
      sorted_deps: 13
    }
    nodes {
      id: 13
      sorted_deps: 12
    }
    nodes {
      id: 12
    }
  }
  nodes {
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "cpu"
        column_idxs: 1
        column_idxs: 2
        column_names: "cpu0"
        column_names: "cpu1"
        column_types: FLOAT64
        column_types: FLOAT64
      }
    }
  }
  nodes {
    id: 5
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
            node: 0
            index: 0
          }
        }
        expressions {
          column {
            node: 0
            index: 1
          }
        }
        expressions {
          func {
            name: "pl.divide"
            args {
              column {
                node: 0
                index: 1
              }
            }
            args {
              column {
                node: 0
                index: 0
              }
            }
          }
        }
        column_names: "cpu0"
        column_names: "cpu1"
        column_names: "quotient"
      }
    }
  }
  nodes {
    id: 13
    op {
      op_type: BLOCKING_AGGREGATE_OPERATOR
      blocking_agg_op {
        values {
          name: "pl.mean"
          args {
            column {
              node: 5
              index: 2
            }
          }
        }
        values {
          name: "pl.mean"
          args {
            column {
              node: 5
              index: 1
            }
          }
        }
        groups {
          node: 5
        }
        group_names: "cpu0"
        value_names: "quotient_mean"
        value_names: "cpu1_mean"
      }
    }
  }
  nodes {
    id: 12
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "cpu2"
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_names: "cpu0"
        column_names: "quotient_mean"
        column_names: "cpu1_mean"
      }
    }
  }
}
)";

TEST_F(CompilerTest, test_general_compilation) {
  auto compiler = Compiler();
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1'])",
          "mapDF = queryDF.Map(fn=lambda r : {'cpu0' : r.cpu0, 'cpu1' : r.cpu1, 'quotient' : "
          "r.cpu1 / r.cpu0})",
          "aggDF = mapDF.Agg(by=lambda r: r.cpu0, fn=lambda r : {'quotient_mean' : "
          "pl.mean(r.quotient), 'cpu1_mean' : pl.mean(r.cpu1)}"
          ").Result(name='cpu2')",
      },
      "\n");
  auto plan_status = compiler.Compile(query, compiler_state_.get());
  VLOG(1) << plan_status.ToString();
  ASSERT_OK(plan_status);

  carnotpb::Plan logical_plan = plan_status.ValueOrDie();
  VLOG(1) << logical_plan.DebugString();

  carnotpb::Plan expected_logical_plan;

  ASSERT_TRUE(
      google::protobuf::TextFormat::MergeFromString(kExpectedLogicalPlan, &expected_logical_plan));
  EXPECT_TRUE(
      google::protobuf::util::MessageDifferencer::Equals(expected_logical_plan, logical_plan));
}

// Test for select order that is different than the schema.
const char* kSelectOrderLogicalPlan = R"(
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
        column_idxs: 3
        column_idxs: 0
        column_idxs: 2
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

TEST_F(CompilerTest, select_order_test) {
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu2', 'count', 'cpu1']).Result(name='cpu_out')",
      },
      "\n");
  auto compiler = Compiler();
  auto plan = compiler.Compile(query, compiler_state_.get());
  EXPECT_OK(plan);
  carnotpb::Plan plan_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kSelectOrderLogicalPlan, &plan_pb));
  VLOG(1) << plan.ValueOrDie().DebugString();
  EXPECT_TRUE(
      google::protobuf::util::MessageDifferencer::Equals(plan_pb, plan.ConsumeValueOrDie()));
}

const char* kGroupByAllPlan = R"(
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
        column_idxs: 2
        column_idxs: 1
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

TEST_F(CompilerTest, group_by_all) {
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu1', 'cpu0'])",
          "aggDF = queryDF.Agg(by=None, fn=lambda r : {'mean' : "
          "pl.mean(r.cpu0)}).Result(name='cpu_out')",
      },
      "\n");
  auto compiler = Compiler();
  auto plan_status = compiler.Compile(query, compiler_state_.get());
  VLOG(1) << plan_status.ToString();
  // EXPECT_OK(plan_status);
  ASSERT_OK(plan_status);
  auto logical_plan = plan_status.ConsumeValueOrDie();
  VLOG(1) << logical_plan.DebugString();
  carnotpb::Plan expected_logical_plan;
  // google::protobuf::util::MessageDifferencer differ();
  ASSERT_TRUE(
      google::protobuf::TextFormat::MergeFromString(kGroupByAllPlan, &expected_logical_plan));
  EXPECT_TRUE(
      google::protobuf::util::MessageDifferencer::Equals(expected_logical_plan, logical_plan));
}

const char* kRangeAggPlan = R"(
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
        column_idxs: 3
        column_idxs: 0
        column_idxs: 2
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
TEST_F(CompilerTest, range_agg_test) {
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu2', 'count', 'cpu1']).RangeAgg(by=lambda r: "
          "r.count, size=2, fn= lambda r: { 'mean': pl.mean(r.cpu1)}).Result(name='cpu_out')",
      },
      "\n");
  auto compiler = Compiler();
  auto plan = compiler.Compile(query, compiler_state_.get());
  EXPECT_OK(plan);
  carnotpb::Plan plan_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kRangeAggPlan, &plan_pb));
  VLOG(1) << plan.ValueOrDie().DebugString();
  EXPECT_TRUE(
      google::protobuf::util::MessageDifferencer::Equals(plan_pb, plan.ConsumeValueOrDie()));
}

TEST_F(CompilerTest, multiple_group_by_agg_test) {
  std::string query = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(start=0,stop=10)",
       "aggDF = queryDF.Agg(by=lambda r : [r.cpu0, r.cpu2], fn=lambda r : {'cpu_count' : "
       "pl.count(r.cpu1), 'cpu_mean' : pl.mean(r.cpu1)}).Result(name='cpu_out')"},
      "\n");
  auto compiler = Compiler();
  auto plan = compiler.Compile(query, compiler_state_.get());
  VLOG(1) << plan.ToString();
  EXPECT_OK(plan);
}

TEST_F(CompilerTest, multiple_group_by_map_then_agg) {
  std::string query = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(start=0,stop=10)",
       "mapDF =  queryDF.Map(fn = lambda r : {'cpu0' : r.cpu0, 'cpu1' : r.cpu1, 'cpu2' : r.cpu2, "
       "'cpu_sum' : r.cpu0+r.cpu1+r.cpu2})",
       "aggDF = mapDF.Agg(by=lambda r : [r.cpu0, r.cpu2], fn=lambda r : {'cpu_count' : "
       "pl.count(r.cpu1), 'cpu_mean' : pl.mean(r.cpu1)}).Result(name='cpu_out')"},
      "\n");
  auto compiler = Compiler();
  auto plan = compiler.Compile(query, compiler_state_.get());
  VLOG(1) << plan.ToString();
  EXPECT_OK(plan);
}
TEST_F(CompilerTest, rename_then_group_by_test) {
  auto query = absl::StrJoin(
      {"queryDF = From(table='sequences', select=['_time', 'xmod10', 'PIx'])",
       "map_out = queryDF.Map(fn=lambda r : {'res': r.PIx, 'c1': r.xmod10})",
       "agg_out = map_out.Agg(by=lambda r: [r.res, r.c1], fn=lambda r: {'count': pl.count(r.c1)})",
       "agg_out.Result(name='t15')"},
      "\n");
  auto compiler = Compiler();
  auto plan = compiler.Compile(query, compiler_state_.get());
  VLOG(1) << plan.ToString();
  EXPECT_OK(plan);
}

// Test to see whether comparisons work.
TEST_F(CompilerTest, comparison_test) {
  auto query =
      absl::StrJoin({"queryDF = From(table='sequences', select=['_time', 'xmod10', 'PIx'])",
                     "map_out = queryDF.Map(fn=lambda r : {'res': r.PIx, "
                     "'c1': r.xmod10, 'gt' : r.xmod10 > 10.0,'lt' : r.xmod10 < 10.0,",
                     "'gte' : r.PIx >= 1.0, 'lte' : r.PIx <= 1.0,", "'eq' : r.PIx == 1.0})",
                     "map_out.Result(name='t15')"},
                    "\n");
  auto compiler = Compiler();
  auto plan = compiler.Compile(query, compiler_state_.get());
  VLOG(1) << plan.ToString();
  EXPECT_OK(plan);
}

// Test to make sure that we can have no args to pl count.
TEST_F(CompilerTest, no_arg_pl_count_test) {
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
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(start=0, stop=10)",
       "aggDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : "
       "pl.count}).Result(name='cpu_out')"},
      "\n");
  auto compiler = Compiler();
  auto plan = compiler.Compile(query, compiler_state.get());
  VLOG(1) << plan.ToString();
  ASSERT_OK(plan);
  VLOG(1) << plan.ValueOrDie().DebugString();
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
