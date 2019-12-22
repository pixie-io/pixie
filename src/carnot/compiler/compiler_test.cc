#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>

#include <tuple>
#include <unordered_map>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/compiler.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/carnot/funcs/metadata/metadata_ops.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace carnot {
namespace compiler {

using ::pl::table_store::schema::Relation;
using ::pl::testing::proto::EqualsProto;
using planpb::testutils::CompareLogicalPlans;
using ::testing::_;
using ::testing::ContainsRegex;

const char* kExtraScalarUDFs = R"proto(
scalar_udfs {
  name: "pl.equal"
  exec_arg_types: UINT128
  exec_arg_types: UINT128
  return_type: BOOLEAN
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
    funcs::metadata::RegisterMetadataOpsOrDie(scalar_udf_registry.get());
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
    rel_map->emplace("sequences", Relation(
                                      {
                                          types::TIME64NS,
                                          types::FLOAT64,
                                          types::FLOAT64,
                                      },
                                      {"time_", "xmod10", "PIx"}));

    rel_map->emplace("cpu", Relation({types::INT64, types::FLOAT64, types::FLOAT64, types::FLOAT64,
                                      types::UINT128},
                                     {"count", "cpu0", "cpu1", "cpu2", "upid"}));
    cgroups_relation_ =
        Relation({types::TIME64NS, types::STRING, types::STRING}, {"time_", "qos", "_attr_pod_id"});

    rel_map->emplace("cgroups", cgroups_relation_);

    rel_map->emplace("http_table",
                     Relation({types::TIME64NS, types::UINT128, types::INT64, types::INT64},

                              {"time_", "upid", "http_resp_status", "http_resp_latency_ns"}));
    rel_map->emplace("network", Relation({types::UINT128, types::INT64, types::INT64, types::INT64},
                                         {MetadataProperty::kUniquePIDColumn, "bytes_in",
                                          "bytes_out", "agent_id"}));
    Relation http_events_relation;
    http_events_relation.AddColumn(types::TIME64NS, "time_");
    http_events_relation.AddColumn(types::UINT128, MetadataProperty::kUniquePIDColumn);
    http_events_relation.AddColumn(types::STRING, "remote_addr");
    http_events_relation.AddColumn(types::INT64, "remote_port");
    http_events_relation.AddColumn(types::INT64, "http_major_version");
    http_events_relation.AddColumn(types::INT64, "http_minor_version");
    http_events_relation.AddColumn(types::INT64, "http_content_type");
    http_events_relation.AddColumn(types::STRING, "http_req_headers");
    http_events_relation.AddColumn(types::STRING, "http_req_method");
    http_events_relation.AddColumn(types::STRING, "http_req_path");
    http_events_relation.AddColumn(types::STRING, "http_req_body");
    http_events_relation.AddColumn(types::STRING, "http_resp_headers");
    http_events_relation.AddColumn(types::INT64, "http_resp_status");
    http_events_relation.AddColumn(types::STRING, "http_resp_message");
    http_events_relation.AddColumn(types::STRING, "http_resp_body");
    http_events_relation.AddColumn(types::INT64, "http_resp_latency_ns");
    rel_map->emplace("http_events", http_events_relation);

    compiler_state_ = std::make_unique<CompilerState>(std::move(rel_map), info_.get(), time_now);

    compiler_ = Compiler();
  }
  std::unique_ptr<CompilerState> compiler_state_;
  std::unique_ptr<RegistryInfo> info_;
  int64_t time_now = 1552607213931245000;
  Compiler compiler_;
  Relation cgroups_relation_;
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
      id: 6
      sorted_children: 14
    }
    nodes {
      id: 14
      sorted_children: 27
      sorted_parents: 6
    }
    nodes {
      id: 27
      sorted_children: 30
      sorted_parents: 14
    }
    nodes {
      id: 30
      sorted_parents: 27
    }
  }
  nodes {
    id: 6
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
    id: 14
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
            node: 6
          }
        }
        expressions {
          column {
            node: 6
            index: 1
          }
        }
        expressions {
          func {
            name: "pl.divide"
            args {
              column {
                node: 6
                index: 1
              }
            }
            args {
              column {
                node: 6
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
    id: 27
    op {
      op_type: AGGREGATE_OPERATOR
      agg_op {
        values {
          name: "pl.mean"
          args {
            column {
              node: 14
              index: 2
            }
          }
          args_data_types: FLOAT64
        }
        values {
          name: "pl.mean"
          args {
            column {
              node: 14
              index: 1
            }
          }
          args_data_types: FLOAT64
        }
        groups {
          node: 14
        }
        group_names: "cpu0"
        value_names: "quotient_mean"
        value_names: "cpu1_mean"
      }
    }
  }
  nodes {
    id: 30
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
          "queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'])",
          "queryDF['quotient'] = queryDF['cpu1'] / queryDF['cpu0']",
          "aggDF = queryDF.groupby(['cpu0']).agg(",
          "quotient_mean=('quotient', pl.mean),",
          "cpu1_mean=('cpu1', pl.mean))",
          "pl.display(aggDF, 'cpu2')",
      },
      "\n");
  auto plan_status = compiler_.Compile(query, compiler_state_.get());
  ASSERT_OK(plan_status);

  planpb::Plan logical_plan = plan_status.ValueOrDie();

  EXPECT_THAT(logical_plan, EqualsProto(kExpectedLogicalPlan)) << logical_plan.DebugString();
}

// Test for select order that is different than the schema.
const char* kSelectOrderLogicalPlan = R"(
nodes {
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
          "queryDF = pl.DataFrame(table='cpu', select=['cpu2', 'count', 'cpu1'])",
          "pl.display(queryDF, 'cpu_out')",
      },
      "\n");

  auto plan = compiler_.Compile(query, compiler_state_.get());
  ASSERT_OK(plan);

  EXPECT_THAT(plan.ConsumeValueOrDie(), Partially(EqualsProto(kSelectOrderLogicalPlan)));
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
        column_names: "time_"
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
        column_names: "time_"
        column_names: "xmod10"
      }
    }
  }
}

)";

TEST_F(CompilerTest, range_now_test) {
  auto query = absl::StrJoin(
      {
          "queryDF = pl.DataFrame(table='sequences', select=['time_', 'xmod10'], start_time=0, "
          "end_time=pl.now())",
          "pl.display(queryDF,'range_table')",
      },
      "\n");

  auto plan = compiler_.Compile(query, compiler_state_.get());
  ASSERT_OK(plan);
  int64_t start_time = 0;
  int64_t stop_time = compiler_state_->time_now().val;
  auto expected_plan = absl::Substitute(kRangeNowPlan, start_time, stop_time);

  EXPECT_THAT(plan.ConsumeValueOrDie(), Partially(EqualsProto(expected_plan)));
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
        column_names: "time_"
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
        column_names: "time_"
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
    std::tie(time_function, chrono_ns) = GetParam();
    query = absl::StrJoin({"queryDF = pl.DataFrame(table='sequences', select=['time_', "
                           "'xmod10'], start_time=pl.now() - $1, end_time=pl.now())",
                           "pl.display(queryDF, '$0')"},
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
    {"pl.minutes(2)", std::chrono::minutes(2)},
    {"pl.hours(2)", std::chrono::hours(2)},
    {"pl.seconds(2)", std::chrono::seconds(2)},
    {"pl.days(2)", std::chrono::hours(2 * 24)},
    {"pl.microseconds(2)", std::chrono::microseconds(2)},
    {"pl.milliseconds(2)", std::chrono::milliseconds(2)}};

TEST_P(CompilerTimeFnTest, range_now_keyword_test) {
  auto plan = compiler_.Compile(query, compiler_state_.get());
  ASSERT_OK(plan);
  VLOG(2) << plan.ValueOrDie().DebugString();

  EXPECT_TRUE(CompareLogicalPlans(expected_plan_pb, plan.ConsumeValueOrDie(), true /*ignore_ids*/));
}

INSTANTIATE_TEST_SUITE_P(CompilerTimeFnTestSuites, CompilerTimeFnTest,
                         ::testing::ValuesIn(compiler_time_data));

const char* kGroupByAllPlan = R"(
nodes {
  nodes {
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
          "queryDF = pl.DataFrame(table='cpu', select=['cpu1', 'cpu0'])",
          "aggDF = queryDF.agg(mean=('cpu0', pl.mean))",
          "pl.display(aggDF, 'cpu_out')",
      },
      "\n");

  auto plan_status = compiler_.Compile(query, compiler_state_.get());
  ASSERT_OK(plan_status);
  auto logical_plan = plan_status.ConsumeValueOrDie();
  VLOG(2) << logical_plan.DebugString();
  EXPECT_THAT(logical_plan, Partially(EqualsProto(kGroupByAllPlan)));
}

TEST_F(CompilerTest, multiple_group_by_agg_test) {
  std::string query =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1', 'cpu2'], "
                     "start_time=0, end_time=10)",
                     "aggDF = queryDF.groupby(['cpu0', 'cpu2']).agg(cpu_count=('cpu1', pl.count),",
                     "cpu_mean=('cpu1', pl.mean))", "pl.display(aggDF, 'cpu_out')"},
                    "\n");

  auto plan = compiler_.Compile(query, compiler_state_.get());
  VLOG(2) << plan.ToString();
  ASSERT_OK(plan);
}

TEST_F(CompilerTest, multiple_group_by_map_then_agg) {
  std::string query =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1', 'cpu2'], "
                     "start_time=0, end_time=10)",
                     "queryDF['cpu_sum'] = queryDF['cpu1'] + queryDF['cpu2']",
                     "aggDF = queryDF.groupby(['cpu0', 'cpu2']).agg(cpu_count=('cpu1', pl.count),",
                     "cpu_mean=('cpu1', pl.mean))", "pl.display(aggDF, 'cpu_out')"},
                    "\n");

  auto plan = compiler_.Compile(query, compiler_state_.get());
  VLOG(2) << plan.ToString();
  ASSERT_OK(plan);
}

TEST_F(CompilerTest, rename_then_group_by_test) {
  auto query =
      absl::StrJoin({"queryDF = pl.DataFrame(table='sequences', select=['time_', 'xmod10', 'PIx'])",
                     "queryDF['res'] = queryDF['PIx']", "queryDF['c1'] = queryDF['xmod10']",
                     "map_out = queryDF[['res', 'c1']]",
                     "agg_out = map_out.groupby(['res', 'c1']).agg(count=('c1', pl.count))",
                     "pl.display(agg_out, 't15')"},
                    "\n");

  auto plan = compiler_.Compile(query, compiler_state_.get());
  VLOG(2) << plan.ToString();
  ASSERT_OK(plan);
}

// Test to see whether comparisons work.
TEST_F(CompilerTest, comparison_test) {
  auto query = absl::StrJoin(
      {"queryDF = pl.DataFrame(table='sequences', select=['time_', 'xmod10', 'PIx'])",
       "queryDF['res'] = queryDF['PIx']", "queryDF['c1'] = queryDF['xmod10']",
       "queryDF['gt'] = queryDF['xmod10'] > 10.0", "queryDF['lt'] = queryDF['xmod10'] < 10.0",
       "queryDF['gte'] = queryDF['PIx'] >= 1.0", "queryDF['lte'] = queryDF['PIx'] <= 1.0",
       "queryDF['eq'] = queryDF['PIx'] == 1.0",
       "map_out = queryDF[['res', 'c1', 'gt', 'lt', 'gte', 'lte', 'eq']]",
       "pl.display(map_out, 't15')"},
      "\n");

  auto plan = compiler_.Compile(query, compiler_state_.get());
  VLOG(2) << plan.ToString();
  ASSERT_OK(plan);
}

const char* kFilterPlan = R"(
nodes {
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
    op {
      op_type: FILTER_OPERATOR
      filter_op {
        expression {
          func {
            name: "pl.$0"
            args {
              column {
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
        }
        columns {
          index: 1
        }
      }
    }
  }
  nodes {
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
    query =
        absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', "
                       "'cpu1'])",
                       "queryDF = queryDF[queryDF['cpu0'] $0 0.5]", "pl.display(queryDF, '$1')"},
                      "\n");
    query = absl::Substitute(query, compare_op_, table_name_);
    VLOG(2) << query;
    expected_plan = absl::Substitute(kFilterPlan, compare_op_proto_, table_name_);
  }
  std::string compare_op_;
  std::string compare_op_proto_;
  std::string query;
  std::string expected_plan;
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

  EXPECT_THAT(plan.ConsumeValueOrDie(), Partially(EqualsProto(expected_plan)));
}

INSTANTIATE_TEST_SUITE_P(FilterTestSuite, FilterTest, ::testing::ValuesIn(comparison_fns));

// TODO(nserrino/phlkuz) create expectations for filter errors.
TEST_F(CompilerTest, filter_errors) {
  std::string non_bool_filter =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'])",
                     "queryDF = queryDF[queryDF['cpu0'] + 0.5]", "pl.display(queryDF, 'blah')"},
                    "\n");
  EXPECT_NOT_OK(compiler_.Compile(non_bool_filter, compiler_state_.get()));

  std::string int_val =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'])",
                     "d = queryDF[1]", "pl.display(d, 'filtered')"},
                    "\n");
  EXPECT_NOT_OK(compiler_.Compile(int_val, compiler_state_.get()));
}

const char* kExpectedLimitPlan = R"(
nodes {
  nodes {
    id: 6
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
    id: 8
    op {
      op_type: LIMIT_OPERATOR
      limit_op {
        limit: 1000
        columns {
          node: 6
        }
        columns {
          node: 6
          index: 1
        }
      }
    }
  }
  nodes {
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
  std::string query = absl::StrJoin({"queryDF = pl.DataFrame(table='cpu', select=['cpu0', "
                                     "'cpu1']).head(n=1000)",
                                     "pl.display(queryDF, 'out_table')"},
                                    "\n");
  auto plan_or_s = compiler_.Compile(query, compiler_state_.get());
  ASSERT_OK(plan_or_s);
  auto plan = plan_or_s.ConsumeValueOrDie();
  EXPECT_THAT(plan, Partially(EqualsProto(kExpectedLimitPlan))) << plan.DebugString();
}

TEST_F(CompilerTest, reused_result) {
  std::string query = absl::StrJoin(
      {
          "queryDF = pl.DataFrame(table='http_table', select=['time_', 'upid', 'http_resp_status', "
          "'http_resp_latency_ns'], start_time='-1m')",
          "x = queryDF[queryDF['http_resp_latency_ns'] < 1000000]",
          "pl.display(queryDF, 'out');",
      },

      "\n");
  auto plan_status = compiler_.CompileToIR(query, compiler_state_.get());
  VLOG(2) << plan_status.ToString();
  ASSERT_OK(plan_status);
  auto plan = plan_status.ConsumeValueOrDie();
  std::vector<IRNode*> mem_srcs = plan->FindNodesOfType(IRNodeType::kMemorySource);
  ASSERT_EQ(mem_srcs.size(), 1);
  MemorySourceIR* mem_src = static_cast<MemorySourceIR*>(mem_srcs[0]);

  EXPECT_EQ(mem_src->Children().size(), 2);

  OperatorIR* src_child1 = mem_src->Children()[0];
  ASSERT_TRUE(Match(src_child1, Filter()));
  FilterIR* filter_child = static_cast<FilterIR*>(src_child1);
  EXPECT_TRUE(Match(filter_child->filter_expr(), LessThan(ColumnNode(), Int(1000000))));
  // Filter should not have children.
  EXPECT_EQ(filter_child->Children().size(), 0);

  OperatorIR* src_child2 = mem_src->Children()[1];
  ASSERT_TRUE(Match(src_child2, MemorySink()));
  MemorySinkIR* mem_sink_child = static_cast<MemorySinkIR*>(src_child2);
  EXPECT_EQ(mem_sink_child->name(), "out");
}

TEST_F(CompilerTest, multiple_result_sinks) {
  std::string query = absl::StrJoin(
      {
          "queryDF = pl.DataFrame(table='http_table', select=['time_', 'upid', 'http_resp_status', "
          "'http_resp_latency_ns'], start_time='-1m')",
          "x = queryDF[queryDF['http_resp_latency_ns'] < "
          "1000000]",
          "pl.display(x, 'filtered_result')",
          "pl.display(queryDF, 'result');",
      },
      "\n");
  auto plan_status = compiler_.Compile(query, compiler_state_.get());
  ASSERT_OK(plan_status);
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
      sorted_children: 0
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
        column_types: UINT128
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
        column_types: UINT128
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
  std::string no_select_arg = "df = pl.DataFrame(table='cpu')\npl.display(df, 'out')";
  auto plan_status = compiler_.Compile(no_select_arg, compiler_state_.get());
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
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 4
      sorted_children: 22
    }
    nodes {
      id: 22
      sorted_children: 9
      sorted_parents: 4
    }
    nodes {
      id: 9
      sorted_children: 12
      sorted_parents: 22
    }
    nodes {
      id: 12
      sorted_parents: 9
    }
  }
  nodes {
    id: 4
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
        column_types: UINT128
      }
    }
  }
  nodes {
    id: 22
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
            node: 4
          }
        }
        expressions {
          column {
            node: 4
            index: 1
          }
        }
        expressions {
          column {
            node: 4
            index: 2
          }
        }
        expressions {
          column {
            node: 4
            index: 3
          }
        }
        expressions {
          column {
            node: 4
            index: 4
          }
        }
        expressions {
          func {
            name: "pl.upid_to_service_name"
            args {
              column {
                node: 4
                index: 4
              }
            }
            id: 1
            args_data_types: UINT128
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
    id: 9
    op {
      op_type: FILTER_OPERATOR
      filter_op {
        expression {
          func {
            name: "pl.equal"
            args {
              column {
                node: 22
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
          node: 22
        }
        columns {
          node: 22
          index: 1
        }
        columns {
          node: 22
          index: 2
        }
        columns {
          node: 22
          index: 3
        }
        columns {
          node: 22
          index: 4
        }
        columns {
          node: 22
          index: 5
        }
      }
    }
  }
  nodes {
    id: 12
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "out"
        column_types: INT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: UINT128
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
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 4
      sorted_children: 31
    }
    nodes {
      id: 31
      sorted_children: 9
      sorted_parents: 4
    }
    nodes {
      id: 9
      sorted_children: 13
      sorted_parents: 31
    }
    nodes {
      id: 13
      sorted_children: 16
      sorted_parents: 9
    }
    nodes {
      id: 16
      sorted_parents: 13
    }
  }
  nodes {
    id: 4
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
        column_types: UINT128
      }
    }
  }
  nodes {
    id: 31
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
            node: 4
          }
        }
        expressions {
          column {
            node: 4
            index: 1
          }
        }
        expressions {
          column {
            node: 4
            index: 2
          }
        }
        expressions {
          column {
            node: 4
            index: 3
          }
        }
        expressions {
          column {
            node: 4
            index: 4
          }
        }
        expressions {
          func {
            name: "pl.upid_to_service_name"
            args {
              column {
                node: 4
                index: 4
              }
            }
            args_data_types: UINT128
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
    id: 9
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
            node: 31
          }
        }
        expressions {
          column {
            node: 31
            index: 1
          }
        }
        expressions {
          column {
            node: 31
            index: 2
          }
        }
        expressions {
          column {
            node: 31
            index: 3
          }
        }
        expressions {
          column {
            node: 31
            index: 4
          }
        }
        expressions {
          column {
            node: 31
            index: 5
          }
        }
        expressions {
          column {
            node: 31
            index: 5
          }
        }
        column_names: "count"
        column_names: "cpu0"
        column_names: "cpu1"
        column_names: "cpu2"
        column_names: "upid"
        column_names: "_attr_service_name"
        column_names: "service"
      }
    }
  }
  nodes {
    id: 13
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
            node: 9
            index: 6
          }
        }
        column_names: "service"
      }
    }
  }
  nodes {
    id: 16
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

const char* kExpectedAgg1MetadataPlan = R"proto(
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 4
      sorted_children: 36
    }
    nodes {
      id: 36
      sorted_children: 9
      sorted_parents: 4
    }
    nodes {
      id: 9
      sorted_children: 17
      sorted_parents: 36
    }
    nodes {
      id: 17
      sorted_children: 20
      sorted_parents: 9
    }
    nodes {
      id: 20
      sorted_parents: 17
    }
  }
  nodes {
    id: 4
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
        column_types: UINT128
      }
    }
  }
  nodes {
    id: 36
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
            node: 4
          }
        }
        expressions {
          column {
            node: 4
            index: 1
          }
        }
        expressions {
          column {
            node: 4
            index: 2
          }
        }
        expressions {
          column {
            node: 4
            index: 3
          }
        }
        expressions {
          column {
            node: 4
            index: 4
          }
        }
        expressions {
          func {
            name: "pl.upid_to_service_name"
            args {
              column {
                node: 4
                index: 4
              }
            }
            args_data_types: UINT128
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
    id: 9
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
            node: 36
          }
        }
        expressions {
          column {
            node: 36
            index: 1
          }
        }
        expressions {
          column {
            node: 36
            index: 2
          }
        }
        expressions {
          column {
            node: 36
            index: 3
          }
        }
        expressions {
          column {
            node: 36
            index: 4
          }
        }
        expressions {
          column {
            node: 36
            index: 5
          }
        }
        expressions {
          column {
            node: 36
            index: 5
          }
        }
        column_names: "count"
        column_names: "cpu0"
        column_names: "cpu1"
        column_names: "cpu2"
        column_names: "upid"
        column_names: "_attr_service_name"
        column_names: "service"
      }
    }
  }
  nodes {
    id: 17
    op {
      op_type: AGGREGATE_OPERATOR
      agg_op {
        values {
          name: "pl.mean"
          args {
            column {
              node: 9
              index: 1
            }
          }
          args_data_types: FLOAT64
        }
        groups {
          node: 9
          index: 6
        }
        group_names: "service"
        value_names: "mean_cpu"
      }
    }
  }
  nodes {
    id: 20
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "out"
        column_types: STRING
        column_types: FLOAT64
        column_names: "service"
        column_names: "mean_cpu"
      }
    }
  }
}
)proto";

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
      id: 4
      sorted_children: 40
    }
    nodes {
      id: 40
      sorted_children: 9
      sorted_parents: 4
    }
    nodes {
      id: 9
      sorted_children: 20
      sorted_parents: 40
    }
    nodes {
      id: 20
      sorted_children: 23
      sorted_parents: 9
    }
    nodes {
      id: 23
      sorted_parents: 20
    }
  }
  nodes {
    id: 4
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
        column_types: UINT128
      }
    }
  }
  nodes {
    id: 40
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
            node: 4
          }
        }
        expressions {
          column {
            node: 4
            index: 1
          }
        }
        expressions {
          column {
            node: 4
            index: 2
          }
        }
        expressions {
          column {
            node: 4
            index: 3
          }
        }
        expressions {
          column {
            node: 4
            index: 4
          }
        }
        expressions {
          func {
            name: "pl.upid_to_service_name"
            args {
              column {
                node: 4
                index: 4
              }
            }
            args_data_types: UINT128
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
    id: 9
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
            node: 40
          }
        }
        expressions {
          column {
            node: 40
            index: 1
          }
        }
        expressions {
          column {
            node: 40
            index: 2
          }
        }
        expressions {
          column {
            node: 40
            index: 3
          }
        }
        expressions {
          column {
            node: 40
            index: 4
          }
        }
        expressions {
          column {
            node: 40
            index: 5
          }
        }
        expressions {
          column {
            node: 40
            index: 5
          }
        }
        column_names: "count"
        column_names: "cpu0"
        column_names: "cpu1"
        column_names: "cpu2"
        column_names: "upid"
        column_names: "_attr_service_name"
        column_names: "service"
      }
    }
  }
  nodes {
    id: 20
    op {
      op_type: AGGREGATE_OPERATOR
      agg_op {
        values {
          name: "pl.mean"
          args {
            column {
              node: 9
              index: 1
            }
          }
          args_data_types: FLOAT64
        }
        groups {
          node: 9
          index: 1
        }
        groups {
          node: 9
          index: 6
        }
        group_names: "cpu0"
        group_names: "service"
        value_names: "mean_cpu"
      }
    }
  }
  nodes {
    id: 23
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "out"
        column_types: FLOAT64
        column_types: STRING
        column_types: FLOAT64
        column_names: "cpu0"
        column_names: "service"
        column_names: "mean_cpu"
      }
    }
  }
}
)proto";

const char* kExpectedAggFilter1MetadataPlan = R"proto(
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 4
      sorted_children: 47
    }
    nodes {
      id: 47
      sorted_children: 9
      sorted_parents: 4
    }
    nodes {
      id: 9
      sorted_children: 20
      sorted_parents: 47
    }
    nodes {
      id: 20
      sorted_children: 53
      sorted_parents: 9
    }
    nodes {
      id: 53
      sorted_children: 25
      sorted_parents: 20
    }
    nodes {
      id: 25
      sorted_children: 28
      sorted_parents: 53
    }
    nodes {
      id: 28
      sorted_parents: 25
    }
  }
  nodes {
    id: 4
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
        column_types: UINT128
      }
    }
  }
  nodes {
    id: 47
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
            node: 4
          }
        }
        expressions {
          column {
            node: 4
            index: 1
          }
        }
        expressions {
          column {
            node: 4
            index: 2
          }
        }
        expressions {
          column {
            node: 4
            index: 3
          }
        }
        expressions {
          column {
            node: 4
            index: 4
          }
        }
        expressions {
          func {
            name: "pl.upid_to_service_name"
            args {
              column {
                node: 4
                index: 4
              }
            }
            id: 1
            args_data_types: UINT128
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
    id: 9
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
            node: 47
          }
        }
        expressions {
          column {
            node: 47
            index: 1
          }
        }
        expressions {
          column {
            node: 47
            index: 2
          }
        }
        expressions {
          column {
            node: 47
            index: 3
          }
        }
        expressions {
          column {
            node: 47
            index: 4
          }
        }
        expressions {
          column {
            node: 47
            index: 5
          }
        }
        expressions {
          column {
            node: 47
            index: 5
          }
        }
        column_names: "count"
        column_names: "cpu0"
        column_names: "cpu1"
        column_names: "cpu2"
        column_names: "upid"
        column_names: "_attr_service_name"
        column_names: "service"
      }
    }
  }
  nodes {
    id: 20
    op {
      op_type: AGGREGATE_OPERATOR
      agg_op {
        values {
          name: "pl.mean"
          args {
            column {
              node: 9
              index: 1
            }
          }
          args_data_types: FLOAT64
        }
        groups {
          node: 9
          index: 4
        }
        groups {
          node: 9
          index: 6
        }
        group_names: "upid"
        group_names: "service"
        value_names: "mean_cpu"
      }
    }
  }
  nodes {
    id: 53
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
            node: 20
          }
        }
        expressions {
          column {
            node: 20
            index: 1
          }
        }
        expressions {
          column {
            node: 20
            index: 2
          }
        }
        expressions {
          func {
            name: "pl.upid_to_service_name"
            args {
              column {
                node: 20
              }
            }
            id: 1
            args_data_types: UINT128
          }
        }
        column_names: "upid"
        column_names: "service"
        column_names: "mean_cpu"
        column_names: "_attr_service_name"
      }
    }
  }
  nodes {
    id: 25
    op {
      op_type: FILTER_OPERATOR
      filter_op {
        expression {
          func {
            name: "pl.equal"
            args {
              column {
                node: 53
                index: 3
              }
            }
            args {
              constant {
                data_type: STRING
                string_value: "pl/service-name"
              }
            }
            args_data_types: STRING
            args_data_types: STRING
          }
        }
        columns {
          node: 53
        }
        columns {
          node: 53
          index: 1
        }
        columns {
          node: 53
          index: 2
        }
        columns {
          node: 53
          index: 3
        }
      }
    }
  }
  nodes {
    id: 28
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "out"
        column_types: UINT128
        column_types: STRING
        column_types: FLOAT64
        column_types: STRING
        column_names: "upid"
        column_names: "service"
        column_names: "mean_cpu"
        column_names: "_attr_service_name"
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
      id: 2
      sorted_children: 28
    }
    nodes {
      id: 28
      sorted_children: 10
      sorted_parents: 2
    }
    nodes {
      id: 10
      sorted_children: 15
      sorted_parents: 28
    }
    nodes {
      id: 15
      sorted_children: 17
      sorted_parents: 10
    }
    nodes {
      id: 17
      sorted_parents: 15
    }
  }
  nodes {
    id: 2
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
        column_types: UINT128
      }
    }
  }
  nodes {
    id: 28
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
            args_data_types: UINT128
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
    id: 10
    op {
      op_type: AGGREGATE_OPERATOR
      agg_op {
        values {
          name: "pl.mean"
          args {
            column {
              index: 1
            }
          }
          args_data_types: FLOAT64
        }
        groups {
          index: 1
        }
        groups {
          index: 5
        }
        group_names: "cpu0"
        group_names: "_attr_service_name"
        value_names: "mean_cpu"
      }
    }
  }
  nodes {
    id: 15
    op {
      op_type: FILTER_OPERATOR
      filter_op {
        expression {
          func {
            name: "pl.equal"
            args {
              column {
                index: 1
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
        }
        columns {
          index: 1
        }
        columns {
          index: 2
        }
      }
    }
  }
  nodes {
    id: 17
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "out"
        column_types: FLOAT64
        column_types: STRING
        column_types: FLOAT64
        column_names: "cpu0"
        column_names: "_attr_service_name"
        column_names: "mean_cpu"
      }
    }
  }
}
)proto";

const char* kExpectedAliasingMetadataPlan = R"proto(
nodes {
  id: 1
  dag {
    nodes {
      id: 2
      sorted_children: 28
    }
    nodes {
      id: 28
      sorted_children: 10
      sorted_parents: 2
    }
    nodes {
      id: 10
      sorted_children: 34
      sorted_parents: 28
    }
    nodes {
      id: 34
      sorted_children: 15
      sorted_parents: 10
    }
    nodes {
      id: 15
      sorted_children: 17
      sorted_parents: 34
    }
    nodes {
      id: 17
      sorted_parents: 15
    }
  }
  nodes {
    id: 2
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
        column_types: UINT128
      }
    }
  }
  nodes {
    id: 28
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
            args_data_types: UINT128
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
    id: 10
    op {
      op_type: AGGREGATE_OPERATOR
      agg_op {
        values {
          name: "pl.mean"
          args {
            column {
              index: 1
            }
          }
          args_data_types: FLOAT64
        }
        groups {
          index: 1
        }
        groups {
          index: 5
        }
        group_names: "cpu0"
        group_names: "_attr_service_id"
        value_names: "mean_cpu"
      }
    }
  }
  nodes {
    id: 34
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
          func {
            name: "pl.service_id_to_service_name"
            args {
              column {
                index: 1
              }
            }
            id: 2
            args_data_types: STRING
          }
        }
        column_names: "cpu0"
        column_names: "_attr_service_id"
        column_names: "mean_cpu"
        column_names: "_attr_service_name"
      }
    }
  }
  nodes {
    id: 15
    op {
      op_type: FILTER_OPERATOR
      filter_op {
        expression {
          func {
            name: "pl.equal"
            args {
              column {
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
        }
        columns {
          index: 1
        }
        columns {
          index: 2
        }
        columns {
          index: 3
        }
      }
    }
  }
  nodes {
    id: 17
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "out"
        column_types: FLOAT64
        column_types: STRING
        column_types: FLOAT64
        column_types: STRING
        column_names: "cpu0"
        column_names: "_attr_service_id"
        column_names: "mean_cpu"
        column_names: "_attr_service_name"
      }
    }
  }
}
)proto";

class MetadataSingleOps
    : public CompilerTest,
      public ::testing::WithParamInterface<std::tuple<std::string, std::string>> {};

// Having this indirection makes reading failed tests much easier.
absl::flat_hash_map<std::string, std::string> metadata_name_to_plan_map{
    {"filter_metadata_plan", kExpectedFilterMetadataPlan},
    {"map_metadata_plan", kExpectedMapMetadataPlan},
    {"agg_metadata_plan1", kExpectedAgg1MetadataPlan},
    {"agg_metadata_plan2", kExpectedAgg2MetadataPlan},
    {"agg_filter_metadata_plan1", kExpectedAggFilter1MetadataPlan},
    {"agg_filter_metadata_plan2", kExpectedAggFilter2MetadataPlan},
    {"aliasing_metadata_plan", kExpectedAliasingMetadataPlan},
};

TEST_P(MetadataSingleOps, valid_filter_metadata_proto) {
  std::string op_call, expected_pb_name;
  std::tie(op_call, expected_pb_name) = GetParam();
  auto expected_pb_iter = metadata_name_to_plan_map.find(expected_pb_name);
  ASSERT_TRUE(expected_pb_iter != metadata_name_to_plan_map.end()) << expected_pb_name;
  std::string expected_pb = expected_pb_iter->second;
  std::string valid_query =
      absl::StrJoin({"df = pl.DataFrame(table='cpu') ", "$0", "pl.display(df, 'out')"}, "\n");
  valid_query = absl::Substitute(valid_query, op_call);

  auto plan_status = compiler_.Compile(valid_query, compiler_state_.get());
  ASSERT_OK(plan_status) << valid_query;

  auto plan = plan_status.ConsumeValueOrDie();
  // Check the select columns match the expected values.
  EXPECT_THAT(plan, Partially(EqualsProto(expected_pb))) << "Actual proto: " << plan.DebugString();
}

// Indirectly maps to metadata_name_to_plan_map
std::vector<std::tuple<std::string, std::string>> metadata_operators{
    {"df = df[df.attr['service'] == 'pl/orders']", "filter_metadata_plan"},
    {"df['service'] = df.attr['service']\ndf = df[['service']]", "map_metadata_plan"},
    {"df['service'] =  df.attr['service']\n"
     "df = df.groupby('service').agg(mean_cpu = ('cpu0', pl.mean))",
     "agg_metadata_plan1"},
    {"df['service'] =  df.attr['service']\n"
     "df = df.groupby(['cpu0', 'service']).agg(mean_cpu = ('cpu0', pl.mean))",
     "agg_metadata_plan2"},
    {"df['service'] =  df.attr['service']\n"
     "aggDF = df.groupby(['upid', 'service']).agg(mean_cpu = ('cpu0', pl.mean))\n"
     "df =aggDF[aggDF.attr['service'] == 'pl/service-name']",
     "agg_filter_metadata_plan1"}};

INSTANTIATE_TEST_SUITE_P(MetadataAttributesSuite, MetadataSingleOps,
                         ::testing::ValuesIn(metadata_operators));

TEST_F(CompilerTest, cgroups_pod_id) {
  std::string query =
      absl::StrJoin({"queryDF = pl.DataFrame(table='cgroups')",
                     "range_out = queryDF[queryDF.attr['pod_name'] == 'pl/pl-nats-1']",
                     "pl.display(range_out, 'out')"},
                    "\n");
  auto plan_status = compiler_.Compile(query, compiler_state_.get());
  ASSERT_OK(plan_status);
}

const char* kJoinInnerQueryPlan = R"proto(
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 15
      sorted_children: 26
    }
    nodes {
      id: 7
      sorted_children: 26
    }
    nodes {
      id: 26
      sorted_children: 38
      sorted_parents: 7
      sorted_parents: 15
    }
    nodes {
      id: 38
      sorted_children: 41
      sorted_parents: 26
    }
    nodes {
      id: 41
      sorted_parents: 38
    }
  }
  nodes {
    id: 15
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "http_table"
        column_idxs: 2
        column_idxs: 1
        column_idxs: 3
        column_names: "http_resp_status"
        column_names: "upid"
        column_names: "http_resp_latency_ns"
        column_types: INT64
        column_types: UINT128
        column_types: INT64
      }
    }
  }
  nodes {
    id: 7
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "cpu"
        column_idxs: 1
        column_idxs: 4
        column_idxs: 2
        column_names: "cpu0"
        column_names: "upid"
        column_names: "cpu1"
        column_types: FLOAT64
        column_types: UINT128
        column_types: FLOAT64
      }
    }
  }
  nodes {
    id: 26
    op {
      op_type: JOIN_OPERATOR
      join_op {
        equality_conditions {
          left_column_index: 1
          right_column_index: 1
        }
        output_columns {
        }
        output_columns {
          column_index: 1
        }
        output_columns {
          column_index: 2
        }
        output_columns {
          parent_index: 1
        }
        output_columns {
          parent_index: 1
          column_index: 1
        }
        output_columns {
          parent_index: 1
          column_index: 2
        }
        column_names: "cpu0"
        column_names: "upid"
        column_names: "cpu1"
        column_names: "http_resp_status"
        column_names: "upid_x"
        column_names: "http_resp_latency_ns"
      }
    }
  }
  nodes {
    id: 38
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
            node: 26
            index: 1
          }
        }
        expressions {
          column {
            node: 26
            index: 3
          }
        }
        expressions {
          column {
            node: 26
            index: 5
          }
        }
        expressions {
          column {
            node: 26
          }
        }
        expressions {
          column {
            node: 26
            index: 2
          }
        }
        column_names: "upid"
        column_names: "http_resp_status"
        column_names: "http_resp_latency_ns"
        column_names: "cpu0"
        column_names: "cpu1"
      }
    }
  }
  nodes {
    id: 41
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "joined"
        column_types: UINT128
        column_types: INT64
        column_types: INT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_names: "upid"
        column_names: "http_resp_status"
        column_names: "http_resp_latency_ns"
        column_names: "cpu0"
        column_names: "cpu1"
      }
    }
  }
}
)proto";

const char* kJoinQueryTypeTpl = R"query(
src1 = pl.DataFrame(table='cpu', select=['cpu0', 'upid', 'cpu1'])
src2 = pl.DataFrame(table='http_table', select=['http_resp_status', 'upid',  'http_resp_latency_ns'])
join = src1.merge(src2, how='$0', left_on=['upid'], right_on=['upid'], suffixes=['', '_x'])
output = join[["upid", "http_resp_status", "http_resp_latency_ns", "cpu0", "cpu1"]]
pl.display(output, 'joined')
)query";

// TODO(philkuz/nserrino): Fix test broken with clang-9/gcc-9.
TEST_F(CompilerTest, DISABLED_inner_join) {
  auto plan_status =
      compiler_.Compile(absl::Substitute(kJoinQueryTypeTpl, "inner"), compiler_state_.get());
  ASSERT_OK(plan_status);
  auto plan = plan_status.ConsumeValueOrDie();
  EXPECT_THAT(plan, EqualsProto(kJoinInnerQueryPlan)) << plan.DebugString();
}

const char* kJoinRightQueryPlan = R"proto(
  dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 15
      sorted_children: 26
    }
    nodes {
      id: 7
      sorted_children: 26
    }
    nodes {
      id: 26
      sorted_children: 38
      sorted_parents: 15
      sorted_parents: 7
    }
    nodes {
      id: 38
      sorted_children: 41
      sorted_parents: 26
    }
    nodes {
      id: 41
      sorted_parents: 38
    }
  }
  nodes {
    id: 15
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "http_table"
        column_idxs: 2
        column_idxs: 1
        column_idxs: 3
        column_names: "http_resp_status"
        column_names: "upid"
        column_names: "http_resp_latency_ns"
        column_types: INT64
        column_types: UINT128
        column_types: INT64
      }
    }
  }
  nodes {
    id: 7
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "cpu"
        column_idxs: 1
        column_idxs: 4
        column_idxs: 2
        column_names: "cpu0"
        column_names: "upid"
        column_names: "cpu1"
        column_types: FLOAT64
        column_types: UINT128
        column_types: FLOAT64
      }
    }
  }
  nodes {
    id: 26
    op {
      op_type: JOIN_OPERATOR
      join_op {
        type: LEFT_OUTER
        equality_conditions {
          left_column_index: 1
          right_column_index: 1
        }
        output_columns {
          parent_index: 1
        }
        output_columns {
          parent_index: 1
          column_index: 1
        }
        output_columns {
          parent_index: 1
          column_index: 2
        }
        output_columns {
        }
        output_columns {
          column_index: 1
        }
        output_columns {
          column_index: 2
        }
        column_names: "cpu0"
        column_names: "upid"
        column_names: "cpu1"
        column_names: "http_resp_status"
        column_names: "upid_x"
        column_names: "http_resp_latency_ns"
      }
    }
  }
  nodes {
    id: 38
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
            node: 26
            index: 1
          }
        }
        expressions {
          column {
            node: 26
            index: 3
          }
        }
        expressions {
          column {
            node: 26
            index: 5
          }
        }
        expressions {
          column {
            node: 26
          }
        }
        expressions {
          column {
            node: 26
            index: 2
          }
        }
        column_names: "upid"
        column_names: "http_resp_status"
        column_names: "http_resp_latency_ns"
        column_names: "cpu0"
        column_names: "cpu1"
      }
    }
  }
  nodes {
    id: 41
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "joined"
        column_types: UINT128
        column_types: INT64
        column_types: INT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_names: "upid"
        column_names: "http_resp_status"
        column_names: "http_resp_latency_ns"
        column_names: "cpu0"
        column_names: "cpu1"
      }
    }
  }
}
)proto";

// TODO(philkuz/nserrino): Fix test broken with clang-9/gcc-9.
TEST_F(CompilerTest, DISABLED_right_join) {
  auto plan_status =
      compiler_.Compile(absl::Substitute(kJoinQueryTypeTpl, "right"), compiler_state_.get());
  ASSERT_OK(plan_status);
  auto plan = plan_status.ConsumeValueOrDie();
  EXPECT_THAT(plan, EqualsProto(kJoinRightQueryPlan)) << plan.DebugString();
}

const char* kSelfJoinQueryPlan = R"proto(
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
      sorted_children: 19
      sorted_children: 18
    }
    nodes {
      id: 19
      sorted_children: 18
      sorted_parents: 7
    }
    nodes {
      id: 18
      sorted_children: 22
      sorted_parents: 7
      sorted_parents: 19
    }
    nodes {
      id: 22
      sorted_parents: 18
    }
  }
  nodes {
    id: 7
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "cpu"
        column_idxs: 1
        column_idxs: 4
        column_idxs: 2
        column_names: "cpu0"
        column_names: "upid"
        column_names: "cpu1"
        column_types: FLOAT64
        column_types: UINT128
        column_types: FLOAT64
      }
    }
  }
  nodes {
    id: 19
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
            node: 7
          }
        }
        expressions {
          column {
            node: 7
            index: 1
          }
        }
        expressions {
          column {
            node: 7
            index: 2
          }
        }
        column_names: "cpu0"
        column_names: "upid"
        column_names: "cpu1"
      }
    }
  }
  nodes {
    id: 18
    op {
      op_type: JOIN_OPERATOR
      join_op {
        equality_conditions {
          left_column_index: 1
          right_column_index: 1
        }
        output_columns {
        }
        output_columns {
          column_index: 1
        }
        output_columns {
          column_index: 2
        }
        output_columns {
          parent_index: 1
        }
        output_columns {
          parent_index: 1
          column_index: 1
        }
        output_columns {
          parent_index: 1
          column_index: 2
        }
        column_names: "cpu0"
        column_names: "upid"
        column_names: "cpu1"
        column_names: "cpu0_x"
        column_names: "upid_x"
        column_names: "cpu1_x"
      }
    }
  }
  nodes {
    id: 22
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "joined"
        column_types: FLOAT64
        column_types: UINT128
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: UINT128
        column_types: FLOAT64
        column_names: "cpu0"
        column_names: "upid"
        column_names: "cpu1"
        column_names: "cpu0_x"
        column_names: "upid_x"
        column_names: "cpu1_x"
      }
    }
  }
}
)proto";

const char* kSelfJoinQuery = R"query(
src1 = pl.DataFrame(table='cpu', select=['cpu0', 'upid', 'cpu1'])
join = src1.merge(src1, how='inner', left_on=['upid'], right_on=['upid'], suffixes=['', '_x'])
pl.display(join, 'joined')
)query";

TEST_F(CompilerTest, self_join) {
  auto plan_status = compiler_.Compile(kSelfJoinQuery, compiler_state_.get());
  ASSERT_OK(plan_status);
  auto plan = plan_status.ConsumeValueOrDie();
  EXPECT_THAT(plan, EqualsProto(kSelfJoinQueryPlan)) << "ACTUAL PLAN: " << plan.DebugString();
}

// Test to make sure syntax errors are properly parsed.
TEST_F(CompilerTest, syntax_error_test) {
  auto syntax_error_query = "pl.DataFrame(";
  auto plan_status = compiler_.Compile(syntax_error_query, compiler_state_.get());
  ASSERT_NOT_OK(plan_status);
  EXPECT_THAT(plan_status.status(), HasCompilerError("SyntaxError: Expected `\\)`"));
}

TEST_F(CompilerTest, indentation_error_test) {
  auto indent_error_query =
      absl::StrJoin({"t = pl.DataFrame(table='blah')", "    pl.display(t, 'blah')"}, "\n");
  auto plan_status = compiler_.Compile(indent_error_query, compiler_state_.get());
  ASSERT_NOT_OK(plan_status);
  EXPECT_THAT(plan_status.status(), HasCompilerError("SyntaxError: invalid syntax"));
}

TEST_F(CompilerTest, missing_result) {
  // Missing the result call at the end of the query.
  auto missing_result_call = "queryDF = pl.DataFrame(table='cpu', select=['cpu0', 'cpu1'])";
  auto missing_result_status = compiler_.Compile(missing_result_call, compiler_state_.get());
  ASSERT_NOT_OK(missing_result_status);

  EXPECT_THAT(missing_result_status.status().msg(),
              ContainsRegex("query does not output a result, please add a print.* statement"));
}

const char* kBadDropQuery = R"pxl(
t1 = pl.DataFrame(table='http_events', start_time='-300s')
t1['service'] = t1.attr['service']
t1['http_resp_latency_ms'] = t1['http_resp_latency_ns'] / 1.0E6
t1['failure'] = t1['http_resp_status'] >= 400
# edit this to increase/decrease window. Dont go lower than 1 second.
t1['window1'] = pl.bin(t1['time_'], pl.seconds(10))
t1['window2'] = pl.bin(t1['time_'] + pl.seconds(5), pl.seconds(10))
window1_agg = t1.groupby(['service', 'window1']).agg(
  quantiles=('http_resp_latency_ms', pl.quantiles),
)
window1_agg['p50'] = pl.pluck_float64(window1_agg['quantiles'], 'p50')
window1_agg['p90'] = pl.pluck_float64(window1_agg['quantiles'], 'p90')
window1_agg['p99'] = pl.pluck_float64(window1_agg['quantiles'], 'p99')
window1_agg['time_'] = window1_agg['window1']
window1_agg = window1_agg.drop(['window1', 'quantiles'])
window = window1_agg[window1_agg['service'] != '']
pl.display(window)
)pxl";

TEST_F(CompilerTest, BadDropQuery) {
  auto graph_or_s = compiler_.CompileToIR(kBadDropQuery, compiler_state_.get());
  ASSERT_OK(graph_or_s);

  MemorySinkIR* mem_sink;
  auto graph = graph_or_s.ConsumeValueOrDie();
  for (int64_t i : graph->dag().TopologicalSort()) {
    auto node = graph->Get(i);
    if (Match(node, MemorySink())) {
      mem_sink = static_cast<MemorySinkIR*>(node);
      break;
    }
  }
  ASSERT_NE(mem_sink, nullptr);
  Relation expected_relation(
      {types::STRING, types::FLOAT64, types::FLOAT64, types::FLOAT64, types::TIME64NS},
      {"service", "p50", "p90", "p99", "time_"});
  EXPECT_EQ(mem_sink->relation(), expected_relation);
  ASSERT_TRUE(Match(mem_sink->parents()[0], Filter()));
  FilterIR* filter = static_cast<FilterIR*>(mem_sink->parents()[0]);
  ASSERT_TRUE(Match(filter->parents()[0], Map()));
  MapIR* map = static_cast<MapIR*>(filter->parents()[0]);
  EXPECT_EQ(map->relation(), expected_relation);
}

TEST_F(CompilerTest, AndExpressionFailsGracefully) {
  auto query =
      absl::StrJoin({"df = pl.DataFrame('bar')", "df[df['service'] != '' && pl.asid() != 10]",
                     "pl.display(df, 'out')"},
                    "\n");
  auto ir_graph_or_s = compiler_.Compile(query, compiler_state_.get());
  ASSERT_NOT_OK(ir_graph_or_s);

  EXPECT_THAT(ir_graph_or_s.status(),
              HasCompilerError("SyntaxError: Expected expression after operator"));
}

TEST_F(CompilerTest, CommentOnlyCodeShouldFailGracefullly) {
  auto query = "# this is a comment";
  auto ir_graph_or_s = compiler_.Compile(query, compiler_state_.get());
  ASSERT_NOT_OK(ir_graph_or_s);

  EXPECT_THAT(ir_graph_or_s.status(), HasCompilerError("No runnable code found"));
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
