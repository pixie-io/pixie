#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>

#include <tuple>
#include <unordered_map>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/compiler.h"
#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/proto/plan.pb.h"
#include "src/carnot/proto/test_proto.h"

namespace pl {
namespace carnot {
namespace compiler {

using carnotpb::testutils::CompareLogicalPlans;
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

    auto rel_map = std::make_unique<RelationMap>();
    rel_map->emplace("sequences",
                     schema::Relation(std::vector<types::DataType>({
                                          types::DataType::TIME64NS,
                                          types::DataType::FLOAT64,
                                          types::DataType::FLOAT64,
                                      }),
                                      std::vector<std::string>({"_time", "xmod10", "PIx"})));

    rel_map->emplace("cpu",
                     schema::Relation(std::vector<types::DataType>(
                                          {types::DataType::INT64, types::DataType::FLOAT64,
                                           types::DataType::FLOAT64, types::DataType::FLOAT64}),
                                      std::vector<std::string>({"count", "cpu0", "cpu1", "cpu2"})));
    compiler_state_ = std::make_unique<CompilerState>(std::move(rel_map), info_.get(), time_now);
  }
  std::unique_ptr<CompilerState> compiler_state_;
  std::shared_ptr<RegistryInfo> info_;
  int64_t time_now = 1552607213931245000;
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
            id: 0
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
  VLOG(2) << plan_status.ToString();
  ASSERT_OK(plan_status);

  carnotpb::Plan logical_plan = plan_status.ValueOrDie();
  VLOG(2) << logical_plan.DebugString();

  carnotpb::Plan expected_logical_plan;

  ASSERT_TRUE(
      google::protobuf::TextFormat::MergeFromString(kExpectedLogicalPlan, &expected_logical_plan));
  EXPECT_TRUE(CompareLogicalPlans(expected_logical_plan, logical_plan, false /*ignore_ids*/));
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
  VLOG(2) << plan.ValueOrDie().DebugString();
  EXPECT_TRUE(CompareLogicalPlans(plan_pb, plan.ConsumeValueOrDie(), false /*ignore_ids*/));
}

const char* kRangeNowPlan = R"(
nodes {
  nodes {
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "sequences"
        column_idxs: 0
        column_idxs: 1
        column_names: "_time"
        column_names: "xmod10"
        column_types: TIME64NS
        column_types: FLOAT64
        start_time {
          value: $0
        }
        stop_time {
          value: $1
        }
      }
    }
  }
  nodes {
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "range_table"
        column_types: TIME64NS
        column_types: FLOAT64
        column_names: "_time"
        column_names: "xmod10"
      }
    }
  }
}

)";

TEST_F(CompilerTest, range_now_test) {
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='sequences', select=['_time', 'xmod10']).Range(start=0, "
          "stop=plc.now())",
          "queryDF.Result(name='range_table')",
      },
      "\n");
  auto compiler = Compiler();
  auto plan = compiler.Compile(query, compiler_state_.get());
  EXPECT_OK(plan);
  VLOG(2) << plan.ValueOrDie().DebugString();
  int64_t start_time = 0;
  int64_t stop_time = compiler_state_->time_now().val;
  auto expected_plan = absl::Substitute(kRangeNowPlan, start_time, stop_time);

  carnotpb::Plan plan_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(expected_plan, &plan_pb));
  VLOG(2) << plan_pb.DebugString();
  EXPECT_TRUE(CompareLogicalPlans(plan_pb, plan.ConsumeValueOrDie(), true /*ignore_ids*/));
}
const char* kRangeTimeUnitPlan = R"(
nodes {
  nodes {
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "sequences"
        column_idxs: 0
        column_idxs: 1
        column_names: "_time"
        column_names: "xmod10"
        column_types: TIME64NS
        column_types: FLOAT64
        start_time {
          value: $0
        }
        stop_time {
          value: $1
        }
      }
    }
  }
  nodes {
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "$2"
        column_types: TIME64NS
        column_types: FLOAT64
        column_names: "_time"
        column_names: "xmod10"
      }
    }
  }
}

)";
class CompilerTimeFnTest
    : public CompilerTest,
      public ::testing::WithParamInterface<std::tuple<std::string, std::chrono::nanoseconds>> {
 protected:
  void SetUp() {
    CompilerTest::SetUp();
    // TODO(philkuz) use Combine with the tuple to get out a set of different values for each of the
    // values.
    std::tie(time_function, chrono_ns) = GetParam();
    query = absl::StrJoin({"queryDF = From(table='sequences', select=['_time', "
                           "'xmod10']).Range(start=plc.now() - $1,stop=plc.now())",
                           "queryDF.Result(name='$0')"},
                          "\n");
    query = absl::Substitute(query, table_name_, time_function);
    VLOG(2) << query;
    int64_t now_time = compiler_state_->time_now().val;
    std::string expected_plan =
        absl::Substitute(kRangeTimeUnitPlan, now_time - chrono_ns.count(), now_time, table_name_);
    ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(expected_plan, &expected_plan_pb));
    VLOG(2) << expected_plan_pb.DebugString();
    compiler_ = Compiler();
  }
  std::string time_function;
  std::chrono::nanoseconds chrono_ns;
  std::string query;
  carnotpb::Plan expected_plan_pb;
  Compiler compiler_;

  std::string table_name_ = "range_table";
};

std::vector<std::tuple<std::string, std::chrono::nanoseconds>> compiler_time_data = {
    {"plc.minutes(2)", std::chrono::minutes(2)},
    {"plc.hours(2)", std::chrono::hours(2)},
    {"plc.seconds(2)", std::chrono::seconds(2)},
    {"plc.days(2)", std::chrono::hours(2 * 24)},
    {"plc.microseconds(2)", std::chrono::microseconds(2)},
    {"plc.milliseconds(2)", std::chrono::milliseconds(2)}};

TEST_P(CompilerTimeFnTest, range_now_keyword_test) {
  auto plan = compiler_.Compile(query, compiler_state_.get());
  ASSERT_OK(plan);
  VLOG(2) << plan.ValueOrDie().DebugString();

  EXPECT_TRUE(CompareLogicalPlans(expected_plan_pb, plan.ConsumeValueOrDie(), true /*ignore_ids*/));
}

INSTANTIATE_TEST_CASE_P(CompilerTimeFnTestSuites, CompilerTimeFnTest,
                        ::testing::ValuesIn(compiler_time_data));

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
  // EXPECT_OK(plan_status);
  ASSERT_OK(plan_status);
  auto logical_plan = plan_status.ConsumeValueOrDie();
  VLOG(2) << logical_plan.DebugString();
  carnotpb::Plan expected_logical_plan;
  // google::protobuf::util::MessageDifferencer differ();
  ASSERT_TRUE(
      google::protobuf::TextFormat::MergeFromString(kGroupByAllPlan, &expected_logical_plan));
  EXPECT_TRUE(CompareLogicalPlans(expected_logical_plan, logical_plan, false /*ignore_ids*/));
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
            id: 1
            args {
              column {
                node: 7
                index: 1
              }
            }
            args {
              func {
                name: "pl.modulo"
                id: 0
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
  VLOG(2) << plan.ValueOrDie().DebugString();
  EXPECT_TRUE(CompareLogicalPlans(plan_pb, plan.ConsumeValueOrDie(), false /*ignore_ids*/));
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
  auto query =
      absl::StrJoin({"queryDF = From(table='sequences', select=['_time', 'xmod10', 'PIx'])",
                     "map_out = queryDF.Map(fn=lambda r : {'res': r.PIx, 'c1': r.xmod10})",
                     "agg_out = map_out.Agg(by=lambda r: [r.res, r.c1], fn=lambda r: {'count': "
                     "pl.count(r.c1)})",
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
  std::string query = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(start=0, stop=10)",
       "aggDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : "
       "pl.count}).Result(name='cpu_out')"},
      "\n");
  auto compiler = Compiler();
  auto plan = compiler.Compile(query, compiler_state_.get());
  ASSERT_OK(plan);
  VLOG(2) << plan.ValueOrDie().DebugString();
}

TEST_F(CompilerTest, implied_stop_params) {
  std::string query = absl::StrJoin({"queryDF = From(table='sequences', select=['_time', "
                                     "'xmod10']).Range(start=plc.now() - plc.minutes(2))",
                                     "queryDF.Result(name='$0')"},
                                    "\n");
  std::string table_name = "ranged_table";
  query = absl::Substitute(query, table_name);
  auto compiler = Compiler();
  auto plan = compiler.Compile(query, compiler_state_.get());
  ASSERT_OK(plan);
  VLOG(1) << plan.ValueOrDie().DebugString();
  int64_t now_time = compiler_state_->time_now().val;
  std::chrono::nanoseconds time_diff = std::chrono::minutes(2);
  std::string expected_plan =
      absl::Substitute(kRangeTimeUnitPlan, now_time - time_diff.count(), now_time, table_name);
  carnotpb::Plan plan_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(expected_plan, &plan_pb));
  VLOG(1) << plan_pb.DebugString();
  EXPECT_TRUE(CompareLogicalPlans(plan_pb, plan.ConsumeValueOrDie(), true /*ignore_ids*/));
}

TEST_F(CompilerTest, string_start_param) {
  std::string query = absl::StrJoin({"queryDF = From(table='sequences', select=['_time', "
                                     "'xmod10']).Range(start='-2m')",
                                     "queryDF.Result(name='$0')"},
                                    "\n");
  std::string table_name = "ranged_table";
  query = absl::Substitute(query, table_name);
  auto compiler = Compiler();
  auto plan = compiler.Compile(query, compiler_state_.get());
  ASSERT_OK(plan);
  VLOG(1) << plan.ValueOrDie().DebugString();
  int64_t now_time = compiler_state_->time_now().val;
  std::chrono::nanoseconds time_diff = std::chrono::minutes(2);
  std::string expected_plan =
      absl::Substitute(kRangeTimeUnitPlan, now_time - time_diff.count(), now_time, table_name);
  carnotpb::Plan plan_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(expected_plan, &plan_pb));
  VLOG(1) << plan_pb.DebugString();
  EXPECT_TRUE(CompareLogicalPlans(plan_pb, plan.ConsumeValueOrDie(), true /*ignore_ids*/));
}

TEST_F(CompilerTest, string_start_stop_param) {
  std::string query = absl::StrJoin({"queryDF = From(table='sequences', select=['_time', "
                                     "'xmod10']).Range(start='-5m', stop='-1m')",
                                     "queryDF.Result(name='$0')"},
                                    "\n");
  std::string table_name = "ranged_table";
  query = absl::Substitute(query, table_name);
  auto compiler = Compiler();
  auto plan = compiler.Compile(query, compiler_state_.get());
  ASSERT_OK(plan);
  VLOG(1) << plan.ValueOrDie().DebugString();
  int64_t now_time = compiler_state_->time_now().val;
  std::chrono::nanoseconds time_diff_start = std::chrono::minutes(5);
  std::chrono::nanoseconds time_diff_end = std::chrono::minutes(1);
  std::string expected_plan =
      absl::Substitute(kRangeTimeUnitPlan, now_time - time_diff_start.count(),
                       now_time - time_diff_end.count(), table_name);
  carnotpb::Plan plan_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(expected_plan, &plan_pb));
  VLOG(1) << plan_pb.DebugString();
  EXPECT_TRUE(CompareLogicalPlans(plan_pb, plan.ConsumeValueOrDie(), true /*ignore_ids*/));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
