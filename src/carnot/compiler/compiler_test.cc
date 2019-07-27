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
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace carnot {
namespace compiler {

using ::pl::testing::proto::EqualsProto;
using planpb::testutils::CompareLogicalPlans;
using ::testing::_;

const char* kExtraScalarUDFs = R"proto(
scalar_udfs {
  name: "pl.upid_to_service_id"
  exec_arg_types: INT64
  return_type: STRING
}
scalar_udfs {
  name: "pl.upid_to_service_name"
  exec_arg_types: INT64
  return_type: STRING
}
scalar_udfs {
  name: "pl.service_id_to_service_name"
  exec_arg_types: STRING
  return_type: STRING
}
scalar_udfs {
  name: "pl.upid_to_pod_id"
  exec_arg_types: INT64
  return_type: STRING
}
scalar_udfs {
  name: "pl.upid_to_pod_name"
  exec_arg_types: INT64
  return_type: STRING
}
scalar_udfs {
  name: "pl.pod_id_to_pod_name"
  exec_arg_types: STRING
  return_type: STRING
}
)proto";
class CompilerTest : public ::testing::Test {
 protected:
  void SetUpRegistryInfo() {
    // TODO(philkuz) replace the following call info_
    // info_ = udfexporter::ExportUDFInfo().ConsumeValueOrDie();
    auto scalar_udf_registry = std::make_unique<udf::ScalarUDFRegistry>("udf_registry");
    auto uda_registry = std::make_unique<udf::UDARegistry>("uda_registry");
    builtins::RegisterBuiltinsOrDie(scalar_udf_registry.get());
    builtins::RegisterBuiltinsOrDie(uda_registry.get());
    auto udf_proto = udf::RegistryInfoExporter()
                         .Registry(*uda_registry)
                         .Registry(*scalar_udf_registry)
                         .ToProto();

    std::string new_udf_info = absl::Substitute("$0$1", udf_proto.DebugString(), kExtraScalarUDFs);
    google::protobuf::TextFormat::MergeFromString(new_udf_info, &udf_proto);

    info_ = std::make_unique<compiler::RegistryInfo>();
    PL_CHECK_OK(info_->Init(udf_proto));
  }

  void SetUp() override {
    SetUpRegistryInfo();

    auto rel_map = std::make_unique<RelationMap>();
    rel_map->emplace("sequences", table_store::schema::Relation(
                                      std::vector<types::DataType>({
                                          types::DataType::TIME64NS,
                                          types::DataType::FLOAT64,
                                          types::DataType::FLOAT64,
                                      }),
                                      std::vector<std::string>({"_time", "xmod10", "PIx"})));

    rel_map->emplace(
        "cpu", table_store::schema::Relation(
                   std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64,
                                                 types::DataType::FLOAT64, types::DataType::FLOAT64,
                                                 types::DataType::INT64}),
                   std::vector<std::string>({"count", "cpu0", "cpu1", "cpu2", "upid"})));
    cgroups_relation_ = table_store::schema::Relation(
        std::vector<types::DataType>(
            {types::DataType::TIME64NS, types::DataType::STRING, types::DataType::STRING}),
        std::vector<std::string>({"time_", "qos", "_attr_pod_id"}));

    rel_map->emplace("cgroups", cgroups_relation_);

    rel_map->emplace(
        "http_table",
        table_store::schema::Relation(
            std::vector<types::DataType>({types::DataType::TIME64NS, types::DataType::INT64,
                                          types::DataType::INT64, types::DataType::INT64}),
            std::vector<std::string>(
                {"time_", "pid", "http_resp_status", "http_resp_latency_ns"})));

    compiler_state_ = std::make_unique<CompilerState>(std::move(rel_map), info_.get(), time_now);

    compiler_ = Compiler();
  }
  std::unique_ptr<CompilerState> compiler_state_;
  std::unique_ptr<RegistryInfo> info_;
  int64_t time_now = 1552607213931245000;
  Compiler compiler_;
  table_store::schema::Relation cgroups_relation_;
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
          }
        }
        expressions {
          column {
            index: 1
          }
        }
        expressions {
          func {
            name: "pl.divide"
            args {
              column {
                index: 1
              }
            }
            args {
              column {
              }
            }
            args_data_types: FLOAT64
            args_data_types: FLOAT64
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
      op_type: AGGREGATE_OPERATOR
      agg_op {
        windowed: false
        values {
          name: "pl.mean"
          args {
            column {
              node: 5
              index: 2
            }
          }
          args_data_types: FLOAT64
        }
        values {
          name: "pl.mean"
          args {
            column {
              node: 5
              index: 1
            }
          }
          args_data_types: FLOAT64
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
  auto plan_status = compiler_.Compile(query, compiler_state_.get());
  VLOG(2) << plan_status.ToString();
  ASSERT_OK(plan_status);

  planpb::Plan logical_plan = plan_status.ValueOrDie();
  VLOG(2) << logical_plan.DebugString();

  planpb::Plan expected_logical_plan;
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

  auto plan = compiler_.Compile(query, compiler_state_.get());
  EXPECT_OK(plan);
  planpb::Plan plan_pb;
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

  auto plan = compiler_.Compile(query, compiler_state_.get());
  EXPECT_OK(plan);
  VLOG(2) << plan.ValueOrDie().DebugString();
  int64_t start_time = 0;
  int64_t stop_time = compiler_state_->time_now().val;
  auto expected_plan = absl::Substitute(kRangeNowPlan, start_time, stop_time);

  planpb::Plan plan_pb;
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
  planpb::Plan expected_plan_pb;
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
      op_type: AGGREGATE_OPERATOR
      agg_op {
        windowed: false
        values {
          name: "pl.mean"
          args {
            column {
              index: 1
            }
          }
          args_data_types: FLOAT64
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
          "aggDF = queryDF.Agg(fn=lambda r : {'mean' : "
          "pl.mean(r.cpu0)}).Result(name='cpu_out')",
      },
      "\n");

  auto plan_status = compiler_.Compile(query, compiler_state_.get());
  // EXPECT_OK(plan_status);
  ASSERT_OK(plan_status);
  auto logical_plan = plan_status.ConsumeValueOrDie();
  VLOG(2) << logical_plan.DebugString();
  planpb::Plan expected_logical_plan;
  // google::protobuf::util::MessageDifferencer differ();
  ASSERT_TRUE(
      google::protobuf::TextFormat::MergeFromString(kGroupByAllPlan, &expected_logical_plan));
  EXPECT_TRUE(CompareLogicalPlans(expected_logical_plan, logical_plan, false /*ignore_ids*/));
}

TEST_F(CompilerTest, group_by_all_none_by_fails) {
  // by=None is no longer supported. Adding a test to make sure that it doesn't work.
  auto query = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu1', 'cpu0'])",
          "aggDF = queryDF.Agg(by=None, fn=lambda r : {'mean' : "
          "pl.mean(r.cpu0)}).Result(name='cpu_out')",
      },
      "\n");

  auto plan_status = compiler_.Compile(query, compiler_state_.get());
  VLOG(2) << plan_status.ToString();
  EXPECT_NOT_OK(plan_status);
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
      sorted_deps: 13
    }
    nodes {
      id: 13
      sorted_deps: 19
    }
    nodes {
      id: 19
      sorted_deps: 6
    }
    nodes {
      id: 6
    }
  }
  nodes {
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
                index: 1
              }
            }
            args {
              func {
                name: "pl.modulo"
                id: 0
                args {
                  column {
                    index: 1
                  }
                }
                args {
                  constant {
                    data_type: INT64
                    int64_value: 2
                  }
                }
                args_data_types: INT64
                args_data_types: INT64
              }
            }
            args_data_types: INT64
            args_data_types: INT64
          }
        }
        expressions {
          column {
            index: 2
          }
        }
        column_names: "group"
        column_names: "cpu1"
      }
    }
  }
  nodes {
    id: 19
    op {
      op_type: AGGREGATE_OPERATOR
      agg_op {
        windowed: false
        values {
          name: "pl.mean"
          args {
            column {
              node: 13
              index: 1
            }
          }
          args_data_types: FLOAT64
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
    id: 6
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
          "queryDF = From(table='cpu', select=['cpu2', 'count', 'cpu1'])",
          "queryDF.RangeAgg(by=lambda r: r.count, size=2, fn= lambda r: { 'mean': "
          "pl.mean(r.cpu1)}).Result(name='cpu_out')",
      },
      "\n");

  auto plan = compiler_.Compile(query, compiler_state_.get());
  EXPECT_OK(plan);
  planpb::Plan plan_pb;
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

  auto plan = compiler_.Compile(query, compiler_state_.get());
  VLOG(2) << plan.ToString();
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

  auto plan = compiler_.Compile(query, compiler_state_.get());
  VLOG(2) << plan.ToString();
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

  auto plan = compiler_.Compile(query, compiler_state_.get());
  VLOG(2) << plan.ToString();
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

  auto plan = compiler_.Compile(query, compiler_state_.get());
  VLOG(2) << plan.ToString();
  EXPECT_OK(plan);
}

// Test to make sure that we can have no args to pl count.
// The compiler will allow it if you have a udf definition for it
// however, translating to the executor is more than a quick fix.
// TODO(philkuz) fix up the builtins to allow for arg-less count.
TEST_F(CompilerTest, DISABLED_no_arg_pl_count_test) {
  std::string query = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(start=0, stop=10)",
       "aggDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : "
       "pl.count}).Result(name='cpu_out')"},
      "\n");

  auto plan = compiler_.Compile(query, compiler_state_.get());
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

  auto plan = compiler_.Compile(query, compiler_state_.get());
  ASSERT_OK(plan);
  VLOG(2) << plan.ValueOrDie().DebugString();
  int64_t now_time = compiler_state_->time_now().val;
  std::chrono::nanoseconds time_diff = std::chrono::minutes(2);
  std::string expected_plan =
      absl::Substitute(kRangeTimeUnitPlan, now_time - time_diff.count(), now_time, table_name);
  planpb::Plan plan_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(expected_plan, &plan_pb));
  VLOG(2) << plan_pb.DebugString();
  EXPECT_TRUE(CompareLogicalPlans(plan_pb, plan.ConsumeValueOrDie(), true /*ignore_ids*/));
}

TEST_F(CompilerTest, string_start_param) {
  std::string query = absl::StrJoin({"queryDF = From(table='sequences', select=['_time', "
                                     "'xmod10']).Range(start='-2m')",
                                     "queryDF.Result(name='$0')"},
                                    "\n");
  std::string table_name = "ranged_table";
  query = absl::Substitute(query, table_name);

  auto plan = compiler_.Compile(query, compiler_state_.get());
  ASSERT_OK(plan);
  VLOG(2) << plan.ValueOrDie().DebugString();
  int64_t now_time = compiler_state_->time_now().val;
  std::chrono::nanoseconds time_diff = std::chrono::minutes(2);
  std::string expected_plan =
      absl::Substitute(kRangeTimeUnitPlan, now_time - time_diff.count(), now_time, table_name);
  planpb::Plan plan_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(expected_plan, &plan_pb));
  VLOG(2) << plan_pb.DebugString();
  EXPECT_TRUE(CompareLogicalPlans(plan_pb, plan.ConsumeValueOrDie(), true /*ignore_ids*/));
}

TEST_F(CompilerTest, string_start_stop_param) {
  std::string query = absl::StrJoin({"queryDF = From(table='sequences', select=['_time', "
                                     "'xmod10']).Range(start='-5m', stop='-1m')",
                                     "queryDF.Result(name='$0')"},
                                    "\n");
  std::string table_name = "ranged_table";
  query = absl::Substitute(query, table_name);

  auto plan = compiler_.Compile(query, compiler_state_.get());
  ASSERT_OK(plan);
  VLOG(2) << plan.ValueOrDie().DebugString();
  int64_t now_time = compiler_state_->time_now().val;
  std::chrono::nanoseconds time_diff_start = std::chrono::minutes(5);
  std::chrono::nanoseconds time_diff_end = std::chrono::minutes(1);
  std::string expected_plan =
      absl::Substitute(kRangeTimeUnitPlan, now_time - time_diff_start.count(),
                       now_time - time_diff_end.count(), table_name);
  planpb::Plan plan_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(expected_plan, &plan_pb));
  VLOG(2) << plan_pb.DebugString();
  EXPECT_TRUE(CompareLogicalPlans(plan_pb, plan.ConsumeValueOrDie(), true /*ignore_ids*/));
}

const char* kFilterPlan = R"(
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 1
      sorted_deps: 0
    }
    nodes {
      sorted_deps: 10
    }
    nodes {
      id: 10
    }
  }
  nodes {
    id: 1
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
    op {
      op_type: FILTER_OPERATOR
      filter_op {
        expression {
          func {
            name: "pl.$0"
            args {
              column {
                node: 1
              }
            }
            args {
              constant {
                data_type: FLOAT64
                float64_value: 0.5
              }
            }
            args_data_types: FLOAT64
            args_data_types: FLOAT64
          }
        }
        columns {
          node: 1
        }
        columns {
          node: 1
          index: 1
        }
      }
    }
  }
  nodes {
    id: 10
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "$1"
        column_types: FLOAT64
        column_types: FLOAT64
        column_names: "cpu0"
        column_names: "cpu1"
      }
    }
  }
}
)";

class FilterTest : public CompilerTest,
                   public ::testing::WithParamInterface<std::tuple<std::string, std::string>> {
 protected:
  void SetUp() {
    CompilerTest::SetUp();
    std::tie(compare_op_, compare_op_proto_) = GetParam();
    query = absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                           "'cpu1']).Filter(fn=lambda r : r.cpu0 $0 0.5)",
                           "queryDF.Result(name='$1')"},
                          "\n");
    query = absl::Substitute(query, compare_op_, table_name_);
    VLOG(2) << query;
    std::string expected_plan = absl::Substitute(kFilterPlan, compare_op_proto_, table_name_);
    ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(expected_plan, &expected_plan_pb));
    VLOG(2) << expected_plan_pb.DebugString();
  }
  std::string compare_op_;
  std::string compare_op_proto_;
  std::string query;
  planpb::Plan expected_plan_pb;
  Compiler compiler_;

  std::string table_name_ = "range_table";
};

std::vector<std::tuple<std::string, std::string>> comparison_fns = {
    {">", "greaterThan"},       {"<", "lessThan"},       {"==", "equal"},
    {">=", "greaterThanEqual"}, {"<=", "lessThanEqual"}, {"!=", "notEqual"}};

TEST_P(FilterTest, basic) {
  auto plan = compiler_.Compile(query, compiler_state_.get());
  ASSERT_OK(plan);
  VLOG(2) << plan.ValueOrDie().DebugString();

  EXPECT_TRUE(CompareLogicalPlans(expected_plan_pb, plan.ConsumeValueOrDie(), true /*ignore_ids*/));
}

INSTANTIATE_TEST_CASE_P(FilterTestSuite, FilterTest, ::testing::ValuesIn(comparison_fns));

TEST_F(CompilerTest, filter_errors) {
  std::string non_bool_filter = absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                                               "'cpu1']).Filter(fn=lambda r : r.cpu0 + 0.5)",
                                               "queryDF.Result(name='blah')"},
                                              "\n");
  EXPECT_NOT_OK(compiler_.Compile(non_bool_filter, compiler_state_.get()));
}

const char* kExpectedLimitPlan = R"(
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 1
      sorted_deps: 0
    }
    nodes {
      sorted_deps: 7
    }
    nodes {
      id: 7
    }
  }
  nodes {
    id: 1
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
    op {
      op_type: LIMIT_OPERATOR
      limit_op {
        limit: 1000
        columns {
          node: 1
        }
        columns {
          node: 1
          index: 1
        }
      }
    }
  }
  nodes {
    id: 7
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "out_table"
        column_types: FLOAT64
        column_types: FLOAT64
        column_names: "cpu0"
        column_names: "cpu1"
      }
    }
  }
}
)";

TEST_F(CompilerTest, limit_test) {
  std::string query = absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', "
                                     "'cpu1']).Limit(rows=1000)",
                                     "queryDF.Result(name='out_table')"},
                                    "\n");
  planpb::Plan expected_plan_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kExpectedLimitPlan, &expected_plan_pb));
  VLOG(2) << "expected";
  VLOG(2) << expected_plan_pb.DebugString();

  auto plan = compiler_.Compile(query, compiler_state_.get());
  ASSERT_OK(plan);
  VLOG(2) << "actual";
  VLOG(2) << plan.ValueOrDie().DebugString();

  EXPECT_TRUE(CompareLogicalPlans(expected_plan_pb, plan.ConsumeValueOrDie(), true /*ignore_ids*/));
}

TEST_F(CompilerTest, reused_result) {
  std::string query = absl::StrJoin(
      {
          "queryDF = From(table='http_table', select=['time_', 'pid', 'http_resp_status', "
          "'http_resp_latency_ns'])",
          "range_out = queryDF.Range(start='-1m')",
          "x = range_out.Filter(fn=lambda r: r.http_resp_latency_ns < 1000000)",
          "result_= range_out.Result(name='out');",
      },

      "\n");
  auto plan_status = compiler_.Compile(query, compiler_state_.get());
  VLOG(2) << plan_status.ToString();
  // This used to not work, but with rearchitecture this actually works.
  EXPECT_OK(plan_status);
}

TEST_F(CompilerTest, multiple_result_sinks) {
  std::string query = absl::StrJoin(
      {
          "queryDF = From(table='http_table', select=['time_', 'pid', 'http_resp_status', "
          "'http_resp_latency_ns'])",
          "range_out = queryDF.Range(start='-1m')",
          "x = range_out.Filter(fn=lambda r: r.http_resp_latency_ns < "
          "1000000).Result(name='filtered_result')",
          "result_= range_out.Result(name='result');",
      },
      "\n");
  auto plan_status = compiler_.Compile(query, compiler_state_.get());
  VLOG(2) << plan_status.ToString();
  EXPECT_OK(plan_status);
}

const char* kExpectedSelectDefaultArg = R"proto(
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 1
      sorted_deps: 0
    }
    nodes {
    }
  }
  nodes {
    id: 1
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "cpu"
        column_idxs: 0
        column_idxs: 1
        column_idxs: 2
        column_idxs: 3
        column_idxs: 4
        column_names: "count"
        column_names: "cpu0"
        column_names: "cpu1"
        column_names: "cpu2"
        column_names: "upid"
        column_types: INT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: INT64
      }
    }
  }
  nodes {
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "out"
        column_types: INT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: INT64
        column_names: "count"
        column_names: "cpu0"
        column_names: "cpu1"
        column_names: "cpu2"
        column_names: "upid"
      }
    }
  }
}
)proto";
TEST_F(CompilerTest, from_select_default_arg) {
  std::string no_select_arg = "From(table='cpu').Result(name='out')";
  auto plan_status = compiler_.Compile(no_select_arg, compiler_state_.get());
  VLOG(2) << plan_status.ToString();
  ASSERT_OK(plan_status);
  auto plan = plan_status.ValueOrDie();
  VLOG(2) << plan.DebugString();

  // Check the select columns match the expected values.
  planpb::Plan expected_plan_pb;
  ASSERT_TRUE(
      google::protobuf::TextFormat::MergeFromString(kExpectedSelectDefaultArg, &expected_plan_pb));
  EXPECT_TRUE(CompareLogicalPlans(expected_plan_pb, plan, true /*ignore_ids*/));
}

const char* kExpectedFilterMetadataPlan = R"proto(
nodes {
  dag {
    nodes {
      sorted_deps: 23
    }
    nodes {
      id: 23
      sorted_deps: 2
    }
    nodes {
      id: 2
      sorted_deps: 7
    }
    nodes {
      id: 7
    }
  }
  nodes {
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "cpu"
        column_idxs: 0
        column_idxs: 1
        column_idxs: 2
        column_idxs: 3
        column_idxs: 4
        column_names: "count"
        column_names: "cpu0"
        column_names: "cpu1"
        column_names: "cpu2"
        column_names: "upid"
        column_types: INT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: INT64
      }
    }
  }
  nodes {
    id: 23
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
          }
        }
        expressions {
          column {
            index: 1
          }
        }
        expressions {
          column {
            index: 2
          }
        }
        expressions {
          column {
            index: 3
          }
        }
        expressions {
          column {
            index: 4
          }
        }
        expressions {
          func {
            name: "pl.upid_to_service_name"
            args {
              column {
                index: 4
              }
            }
            id: 1
            args_data_types: INT64
          }
        }
        column_names: "count"
        column_names: "cpu0"
        column_names: "cpu1"
        column_names: "cpu2"
        column_names: "upid"
        column_names: "_attr_service_name"
      }
    }
  }
  nodes {
    id: 2
    op {
      op_type: FILTER_OPERATOR
      filter_op {
        expression {
          func {
            name: "pl.equal"
            args {
              column {
                node: 23
                index: 5
              }
            }
            args {
              constant {
                data_type: STRING
                string_value: "pl/orders"
              }
            }
            args_data_types: STRING
            args_data_types: STRING
          }
        }
        columns {
          node: 23
        }
        columns {
          node: 23
          index: 1
        }
        columns {
          node: 23
          index: 2
        }
        columns {
          node: 23
          index: 3
        }
        columns {
          node: 23
          index: 4
        }
        columns {
          node: 23
          index: 5
        }
      }
    }
  }
  nodes {
    id: 7
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "out"
        column_types: INT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: INT64
        column_types: STRING
        column_names: "count"
        column_names: "cpu0"
        column_names: "cpu1"
        column_names: "cpu2"
        column_names: "upid"
        column_names: "_attr_service_name"
      }
    }
  }
}
)proto";
const char* kExpectedMapMetadataPlan = R"proto(
nodes {
  id: 1
  dag {
    nodes {
      sorted_deps: 20
    }
    nodes {
      id: 20
      sorted_deps: 2
    }
    nodes {
      id: 2
      sorted_deps: 5
    }
    nodes {
      id: 5
    }
  }
  nodes {
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "cpu"
        column_idxs: 0
        column_idxs: 1
        column_idxs: 2
        column_idxs: 3
        column_idxs: 4
        column_names: "count"
        column_names: "cpu0"
        column_names: "cpu1"
        column_names: "cpu2"
        column_names: "upid"
        column_types: INT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: INT64
      }
    }
  }
  nodes {
    id: 20
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
          }
        }
        expressions {
          column {
            index: 1
          }
        }
        expressions {
          column {
            index: 2
          }
        }
        expressions {
          column {
            index: 3
          }
        }
        expressions {
          column {
            index: 4
          }
        }
        expressions {
          func {
            name: "pl.upid_to_service_name"
            args {
              column {
                index: 4
              }
            }
            args_data_types: INT64
            id: 0
          }
        }
        column_names: "count"
        column_names: "cpu0"
        column_names: "cpu1"
        column_names: "cpu2"
        column_names: "upid"
        column_names: "_attr_service_name"
      }
    }
  }
  nodes {
    id: 2
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
            node: 20
            index: 5
          }
        }
        column_names: "service"
      }
    }
  }
  nodes {
    id: 5
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "out"
        column_types: STRING
        column_names: "service"
      }
    }
  }
}
)proto";

const char* kExpectedAgg1MetadataPlan = R"proto(dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      sorted_deps: 23
    }
    nodes {
      id: 23
      sorted_deps: 2
    }
    nodes {
      id: 2
      sorted_deps: 8
    }
    nodes {
      id: 8
    }
  }
  nodes {
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "cpu"
        column_idxs: 0
        column_idxs: 1
        column_idxs: 2
        column_idxs: 3
        column_idxs: 4
        column_names: "count"
        column_names: "cpu0"
        column_names: "cpu1"
        column_names: "cpu2"
        column_names: "upid"
        column_types: INT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: INT64
      }
    }
  }
  nodes {
    id: 23
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
          }
        }
        expressions {
          column {
            index: 1
          }
        }
        expressions {
          column {
            index: 2
          }
        }
        expressions {
          column {
            index: 3
          }
        }
        expressions {
          column {
            index: 4
          }
        }
        expressions {
          func {
            name: "pl.upid_to_service_name"
            args {
              column {
                index: 4
              }
            }
            args_data_types: INT64
            id: 0
          }
        }
        column_names: "count"
        column_names: "cpu0"
        column_names: "cpu1"
        column_names: "cpu2"
        column_names: "upid"
        column_names: "_attr_service_name"
      }
    }
  }
  nodes {
    id: 2
    op {
      op_type: AGGREGATE_OPERATOR
      agg_op {
        values {
          name: "pl.mean"
          args {
            column {
              node: 23
              index: 1
            }
          }
          args_data_types: FLOAT64
        }
        groups {
          node: 23
          index: 5
        }
        group_names: "_attr_service_name"
        value_names: "mean_cpu"
      }
    }
  }
  nodes {
    id: 8
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "out"
        column_types: STRING
        column_types: FLOAT64
        column_names: "_attr_service_name"
        column_names: "mean_cpu"
      }
    }
  }
})proto";
const char* kExpectedAgg2MetadataPlan = R"proto(
  dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      sorted_deps: 25
    }
    nodes {
      id: 25
      sorted_deps: 2
    }
    nodes {
      id: 2
      sorted_deps: 10
    }
    nodes {
      id: 10
    }
  }
  nodes {
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "cpu"
        column_idxs: 0
        column_idxs: 1
        column_idxs: 2
        column_idxs: 3
        column_idxs: 4
        column_names: "count"
        column_names: "cpu0"
        column_names: "cpu1"
        column_names: "cpu2"
        column_names: "upid"
        column_types: INT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: INT64
      }
    }
  }
  nodes {
    id: 25
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
          }
        }
        expressions {
          column {
            index: 1
          }
        }
        expressions {
          column {
            index: 2
          }
        }
        expressions {
          column {
            index: 3
          }
        }
        expressions {
          column {
            index: 4
          }
        }
        expressions {
          func {
            name: "pl.upid_to_service_name"
            args {
              column {
                index: 4
              }
            }
            id: 0
            args_data_types: INT64
          }
        }
        column_names: "count"
        column_names: "cpu0"
        column_names: "cpu1"
        column_names: "cpu2"
        column_names: "upid"
        column_names: "_attr_service_name"
      }
    }
  }
  nodes {
    id: 2
    op {
      op_type: AGGREGATE_OPERATOR
      agg_op {
        values {
          name: "pl.mean"
          args {
            column {
              node: 25
              index: 1
            }
          }
          args_data_types: FLOAT64
        }
        groups {
          node: 25
        }
        groups {
          node: 25
          index: 5
        }
        group_names: "count"
        group_names: "_attr_service_name"
        value_names: "mean_cpu"
      }
    }
  }
  nodes {
    id: 10
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "out"
        column_types: INT64
        column_types: STRING
        column_types: FLOAT64
        column_names: "count"
        column_names: "_attr_service_name"
        column_names: "mean_cpu"
      }
    }
  }
}
)proto";

const char* kExpectedAggFilter1MetadataPlan = R"proto(
nodes {
  id: 1
  dag {
    nodes {
      sorted_deps: 32
    }
    nodes {
      id: 32
      sorted_deps: 3
    }
    nodes {
      id: 3
      sorted_deps: 2
    }
    nodes {
      id: 2
      sorted_deps: 15
    }
    nodes {
      id: 15
    }
  }
  nodes {
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
      }
    }
  }
  nodes {
    id: 32
    op {
      op_type: MAP_OPERATOR
      map_op {
      }
    }
  }
  nodes {
    id: 3
    op {
      op_type: AGGREGATE_OPERATOR
      agg_op {
      }
    }
  }
  nodes {
    id: 2
    op {
      op_type: FILTER_OPERATOR
      filter_op {
      }
    }
  }
  nodes {
    id: 15
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "out"
        column_types: INT64
        column_types: STRING
        column_types: FLOAT64
        column_names: "upid"
        column_names: "_attr_service_name"
        column_names: "mean_cpu"
      }
    }
  }
}
)proto";

const char* kExpectedAggFilter2MetadataPlan = R"proto(
nodes {
  id: 1
  dag {
    nodes {
      sorted_deps: 32
    }
    nodes {
      id: 32
      sorted_deps: 3
    }
    nodes {
      id: 3
      sorted_deps: 2
    }
    nodes {
      id: 2
      sorted_deps: 15
    }
    nodes {
      id: 15
    }
  }
  nodes {
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
      }
    }
  }
  nodes {
    id: 32
    op {
      op_type: MAP_OPERATOR
      map_op {
      }
    }
  }
  nodes {
    id: 3
    op {
      op_type: AGGREGATE_OPERATOR
      agg_op {
        values {
          name: "pl.mean"
          args {
            column {
              node: 32
              index: 1
            }
          }
          args_data_types: FLOAT64
        }
        groups {
          node: 32
        }
        groups {
          node: 32
          index: 5
        }
        group_names: "count"
        group_names: "_attr_service_name"
        value_names: "mean_cpu"
      }
    }
  }
  nodes {
    id: 2
    op {
      op_type: FILTER_OPERATOR
      filter_op {
        expression {
          func {
            name: "pl.equal"
            args {
              column {
                node: 3
                index: 1
              }
            }
            args {
              constant {
                data_type: STRING
                string_value: "pl/orders"
              }
            }
          }
        }
      }
    }
  }
  nodes {
    id: 15
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "out"
        column_types: INT64
        column_types: STRING
        column_types: FLOAT64
        column_names: "count"
        column_names: "_attr_service_name"
        column_names: "mean_cpu"
      }
    }
  }
})proto";

const char* kExpectedAliasingMetadataPlan = R"proto(
nodes {
  id: 1
  dag {
    nodes {
      sorted_deps: 32
    }
    nodes {
      id: 32
      sorted_deps: 3
    }
    nodes {
      id: 3
      sorted_deps: 39
    }
    nodes {
      id: 39
      sorted_deps: 2
    }
    nodes {
      id: 2
      sorted_deps: 15
    }
    nodes {
      id: 15
    }
  }
  nodes {
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "cpu"
        column_idxs: 0
        column_idxs: 1
        column_idxs: 2
        column_idxs: 3
        column_idxs: 4
        column_names: "count"
        column_names: "cpu0"
        column_names: "cpu1"
        column_names: "cpu2"
        column_names: "upid"
        column_types: INT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: INT64
      }
    }
  }
  nodes {
    id: 32
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
          }
        }
        expressions {
          column {
            index: 1
          }
        }
        expressions {
          column {
            index: 2
          }
        }
        expressions {
          column {
            index: 3
          }
        }
        expressions {
          column {
            index: 4
          }
        }
        expressions {
          func {
            name: "pl.upid_to_service_id"
            args {
              column {
                index: 4
              }
            }
            id: 1
            args_data_types: INT64
          }
        }
        column_names: "count"
        column_names: "cpu0"
        column_names: "cpu1"
        column_names: "cpu2"
        column_names: "upid"
        column_names: "_attr_service_id"
      }
    }
  }
  nodes {
    id: 3
    op {
      op_type: AGGREGATE_OPERATOR
      agg_op {
        values {
          name: "pl.mean"
          args {
            column {
              node: 32
              index: 1
            }
          }
          args_data_types: FLOAT64
        }
        groups {
          node: 32
        }
        groups {
          node: 32
          index: 5
        }
        group_names: "count"
        group_names: "_attr_service_id"
        value_names: "mean_cpu"
      }
    }
  }
  nodes {
    id: 39
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
            node: 3
          }
        }
        expressions {
          column {
            node: 3
            index: 1
          }
        }
        expressions {
          column {
            node: 3
            index: 2
          }
        }
        expressions {
          func {
            name: "pl.service_id_to_service_name"
            args {
              column {
                node: 3
                index: 1
              }
            }
            id: 2
            args_data_types: STRING
          }
        }
        column_names: "count"
        column_names: "_attr_service_id"
        column_names: "mean_cpu"
        column_names: "_attr_service_name"
      }
    }
  }
  nodes {
    id: 2
    op {
      op_type: FILTER_OPERATOR
      filter_op {
        expression {
          func {
            name: "pl.equal"
            args {
              column {
                node: 39
                index: 3
              }
            }
            args {
              constant {
                data_type: STRING
                string_value: "pl/orders"
              }
            }
            args_data_types: STRING
            args_data_types: STRING
          }
        }
        columns {
          node: 39
        }
        columns {
          node: 39
          index: 1
        }
        columns {
          node: 39
          index: 2
        }
        columns {
          node: 39
          index: 3
        }
      }
    }
  }
  nodes {
    id: 15
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "out"
        column_types: INT64
        column_types: STRING
        column_types: FLOAT64
        column_types: STRING
        column_names: "count"
        column_names: "_attr_service_id"
        column_names: "mean_cpu"
        column_names: "_attr_service_name"
      }
    }
  }
})proto";

class MetadataSingleOps
    : public CompilerTest,
      public ::testing::WithParamInterface<std::tuple<std::string, std::string>> {};

TEST_P(MetadataSingleOps, valid_filter_metadata_proto) {
  std::string op_call, expected_pb;
  std::tie(op_call, expected_pb) = GetParam();
  std::string valid_query = absl::StrJoin(
      {"queryDF = From(table='cpu') ", "opDF = queryDF.$0", "opDF.Result(name='out')"}, "\n");
  valid_query = absl::Substitute(valid_query, op_call);
  VLOG(2) << valid_query;
  auto plan_status = compiler_.Compile(valid_query, compiler_state_.get());
  ASSERT_OK(plan_status);
  auto plan = plan_status.ConsumeValueOrDie();
  VLOG(2) << plan.DebugString();

  // Check the select columns match the expected values.
  EXPECT_THAT(plan, Partially(EqualsProto(expected_pb)));
}
std::vector<std::tuple<std::string, std::string>> metadata_operators{
    {"Filter(fn=lambda r : r.attr.service == 'pl/orders')", kExpectedFilterMetadataPlan},
    {"Map(fn=lambda r: {'service': r.attr.service})", kExpectedMapMetadataPlan},
    {"Agg(fn=lambda r: {'mean_cpu': pl.mean(r.cpu0)}, by=lambda r : r.attr.service)",
     kExpectedAgg1MetadataPlan},
    {"Agg(fn=lambda r: {'mean_cpu': pl.mean(r.cpu0)}, by=lambda r : [r.count, "
     "r.attr.service])",
     kExpectedAgg2MetadataPlan},
    {"Agg(by=lambda r: [r.upid, r.attr.service],  fn=lambda "
     "r:{'mean_cpu': pl.mean(r.cpu0)}).Filter(fn=lambda r: r.attr.service=='pl/service-name')",
     kExpectedAggFilter1MetadataPlan},
    {"Agg(fn=lambda r: {'mean_cpu': pl.mean(r.cpu0)}, by=lambda r : [r.count, "
     "r.attr.service]).Filter(fn=lambda r : r.attr.service == 'pl/orders')",
     kExpectedAggFilter2MetadataPlan},
    {"Agg(fn=lambda r: {'mean_cpu': pl.mean(r.cpu0)}, by=lambda r : [r.count, "
     "r.attr.service_id]).Filter(fn=lambda r : r.attr.service == 'pl/orders')",
     kExpectedAliasingMetadataPlan}};

INSTANTIATE_TEST_CASE_P(MetadataAttributesSuite, MetadataSingleOps,
                        ::testing::ValuesIn(metadata_operators));

TEST_F(CompilerTest, cgroups_pod_id) {
  std::string query =
      absl::StrJoin({"queryDF = From(table='cgroups')",
                     "range_out = queryDF.Filter(fn=lambda r: r.attr.pod_name == 'pl/pl-nats-1')",
                     "range_out.Result(name='out')"},
                    "\n");
  auto plan_status = compiler_.Compile(query, compiler_state_.get());
  VLOG(1) << plan_status.ToString();
  EXPECT_OK(plan_status);
  VLOG(1) << plan_status.ValueOrDie().DebugString();
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
