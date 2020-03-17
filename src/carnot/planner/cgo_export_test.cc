#include "src/carnot/planner/cgo_export.h"

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <string>
#include <vector>

#include <absl/strings/str_join.h>
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/compilerpb/compiler_status.pb.h"
#include "src/carnot/planner/distributedpb/distributed_plan.pb.h"
#include "src/carnot/planner/plannerpb/query_flags.pb.h"
#include "src/carnot/planner/test_utils.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/base/error.h"
#include "src/common/base/macros.h"
#include "src/common/base/statusor.h"
#include "src/common/testing/protobuf.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace carnot {
namespace planner {

using pl::testing::proto::EqualsProto;
using pl::testing::proto::Partially;

constexpr char kUDFInfoPb[] = R"proto(
scalar_udfs {
  name: "px.greaterThanEqual"
  exec_arg_types: INT64
  exec_arg_types: INT64
  return_type: BOOLEAN
}
)proto";

class PlannerExportTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // TODO(philkuz/zasgar) need to import the udf_info str here once we have the genrule.
    // or figure out a different way to handle this.
    udf_info_str_ = kUDFInfoPb;
  }

  plannerpb::QueryRequest MakeQueryRequest(const std::string& query) {
    plannerpb::QueryRequest query_request;
    query_request.set_query_str(query);
    return query_request;
  }

  void TearDown() override { PlannerFree(planner_); }
  PlannerPtr MakePlanner() { return PlannerNew(udf_info_str_.c_str(), udf_info_str_.length()); }
  PlannerPtr planner_;
  std::string udf_info_str_;
};

StatusOr<std::string> PlannerPlanGoStr(PlannerPtr planner_ptr, std::string planner_state,
                                       std::string query_result, int* resultLen) {
  char* result = PlannerPlan(planner_ptr, planner_state.c_str(), planner_state.length(),
                             query_result.c_str(), query_result.length(), resultLen);
  if (*resultLen == 0) {
    return error::InvalidArgument("Planner failed to return.");
  }

  std::string lp_str(result, result + *resultLen);
  delete[] result;
  return lp_str;
}

StatusOr<std::string> PlannerGetAvailableFlagsGoStr(PlannerPtr planner_ptr,
                                                    std::string query_request, int* resultLen) {
  char* result = PlannerGetAvailableFlags(planner_ptr, query_request.c_str(),
                                          query_request.length(), resultLen);

  if (*resultLen == 0) {
    return error::InvalidArgument("GetAvailableFlags failed to return");
  }

  std::string flags_spec_str(result, result + *resultLen);
  delete[] result;
  return flags_spec_str;
}

StatusOr<std::string> PlannerVizFuncsInfoGoStr(PlannerPtr planner_ptr, std::string query,
                                               int* resultLen) {
  char* result = PlannerVizFuncsInfo(planner_ptr, query.c_str(), query.length(), resultLen);

  if (*resultLen == 0) {
    return error::InvalidArgument("VizFuncsInfo failed to return");
  }

  std::string result_str(result, result + *resultLen);
  delete[] result;
  return result_str;
}

// TODO(philkuz/nserrino): Fix test broken with clang-9/gcc-9.
TEST_F(PlannerExportTest, DISABLED_one_agent_one_kelvin_query_test) {
  planner_ = MakePlanner();
  int result_len;
  std::string query = "df = px.DataFrame(table='table1')\npx.display(df, 'out')";
  auto query_request = MakeQueryRequest(query);

  auto logical_planner_state = testutils::CreateTwoAgentsOneKelvinPlannerState();
  auto interface_result = PlannerPlanGoStr(planner_, logical_planner_state.DebugString(),
                                           query_request.DebugString(), &result_len);
  ASSERT_OK(interface_result);

  distributedpb::LogicalPlannerResult planner_result;
  ASSERT_TRUE(planner_result.ParseFromString(interface_result.ConsumeValueOrDie()));
  ASSERT_OK(planner_result.status());
  std::string expected_planner_result_str =
      absl::Substitute("plan {$0}", testutils::kExpectedPlanTwoAgentOneKelvin);
  EXPECT_THAT(planner_result, Partially(EqualsProto(expected_planner_result_str)))
      << planner_result.DebugString();
}

TEST_F(PlannerExportTest, bad_queries) {
  planner_ = MakePlanner();
  int result_len;
  // Bad table name query that should yield a compiler error.
  std::string bad_table_query =
      "df = px.DataFrame(table='bad_table_name')\n"
      "px.display(df, 'out')";
  auto logical_planner_state = testutils::CreateTwoAgentsPlannerState();
  auto query_request = MakeQueryRequest(bad_table_query);
  auto interface_result = PlannerPlanGoStr(planner_, logical_planner_state.DebugString(),
                                           query_request.DebugString(), &result_len);
  // The compiler should successfully compile and a proto should be returned.
  ASSERT_OK(interface_result);
  distributedpb::LogicalPlannerResult planner_result_pb;
  ASSERT_TRUE(planner_result_pb.ParseFromString(interface_result.ConsumeValueOrDie()));
  EXPECT_NOT_OK(planner_result_pb.status());
  EXPECT_THAT(planner_result_pb.status(), HasCompilerError("Table 'bad_table_name' not found."));
}

constexpr char kUDFQuery[] = R"query(
t1 = px.DataFrame(table='table1', start_time='-30s')
t1 = t1[t1['cpu_cycles'] >= 0]
px.display(t1)
)query";

// Previously had an issue where the UDF registry's memory was improperly handled, and this query
// would cause a segfault. If this unit test passes, then that bug should be gone.
TEST_F(PlannerExportTest, udf_in_query) {
  planner_ = MakePlanner();
  auto logical_planner_state = testutils::CreateTwoAgentsOneKelvinPlannerState();
  int result_len;
  auto query_request = MakeQueryRequest(kUDFQuery);
  auto interface_result = PlannerPlanGoStr(planner_, logical_planner_state.DebugString(),
                                           query_request.DebugString(), &result_len);
  // The compiler should successfully compile and a proto should be returned.
  ASSERT_OK(interface_result);
  distributedpb::LogicalPlannerResult planner_result_pb;
  ASSERT_TRUE(planner_result_pb.ParseFromString(interface_result.ConsumeValueOrDie()));
  EXPECT_OK(planner_result_pb.status());
}

TEST_F(PlannerExportTest, pass_query_string_instead_of_req_should_fail) {
  planner_ = MakePlanner();
  auto logical_planner_state = testutils::CreateTwoAgentsOneKelvinPlannerState();
  int result_len;
  // Pass in kUDFQuery instead of query_request object here.
  auto interface_result =
      PlannerPlanGoStr(planner_, logical_planner_state.DebugString(), kUDFQuery, &result_len);
  // The compiler should successfully compile and a proto should be returned.
  ASSERT_OK(interface_result);
  distributedpb::LogicalPlannerResult planner_result_pb;
  ASSERT_TRUE(planner_result_pb.ParseFromString(interface_result.ConsumeValueOrDie()));
  ASSERT_NOT_OK(planner_result_pb.status());
  EXPECT_THAT(planner_result_pb.status().msg(),
              ::testing::ContainsRegex("Failed to process the query request.*"));
}

constexpr char kFlagValueQuery[] = R"pxl(
px.flags('foo', type=str, description='a random param', default='default')
px.flags.parse()
queryDF = px.DataFrame(table='cpu', select=['cpu0'])
queryDF['foo_flag'] = px.flags.foo
px.display(queryDF, 'map')
)pxl";

constexpr char kAvailableFlags[] = R"(
flags {
  data_type: STRING
  semantic_type: ST_NONE
  name: "foo"
  description: "a random param"
  default_value: {
    data_type: STRING
    string_value: "default"
  }
}
)";

// Tests whether we can successfully send a query and get back the available flags schema.
TEST_F(PlannerExportTest, get_available_flags_empty_flags) {
  planner_ = MakePlanner();
  int result_len;
  auto query_request = MakeQueryRequest(kFlagValueQuery);
  auto interface_result =
      PlannerGetAvailableFlagsGoStr(planner_, query_request.DebugString(), &result_len);

  ASSERT_OK(interface_result);
  pl::carnot::planner::plannerpb::GetAvailableFlagsResult get_flags_result;
  ASSERT_TRUE(get_flags_result.ParseFromString(interface_result.ConsumeValueOrDie()));
  EXPECT_OK(get_flags_result.status());
  EXPECT_THAT(get_flags_result.query_flags(), EqualsProto(kAvailableFlags));
}

constexpr char kVizFuncsQuery[] = R"pxl(
import px
@px.viz.vega("vega spec for f")
def f(start_time: px.Time, end_time: px.Time, svc: str):
  """Doc string for f"""
  return 1

@px.viz.vega("vega spec for g")
def g(a: int, b: float):
  """Doc string for g"""
  return 1
)pxl";

constexpr char kExpectedVizFuncsInfoPb[] = R"(
doc_string_map {
  key: "f"
  value: "Doc string for f"
}
doc_string_map {
  key: "g"
  value: "Doc string for g"
}
viz_spec_map {
  key: "f"
  value {
    vega_spec: "vega spec for f"
  }
}
viz_spec_map {
  key: "g"
  value {
    vega_spec: "vega spec for g"
  }
}
fn_args_map {
  key: "f"
  value {
    args {
      semantic_type: ST_NONE
      data_type: TIME64NS
      name: "start_time"
    }
    args {
      semantic_type: ST_NONE
      data_type: TIME64NS
      name: "end_time"
    }
    args {
      semantic_type: ST_NONE
      data_type: STRING
      name: "svc"
    }
  }
}
fn_args_map {
  key: "g"
  value {
    args {
      data_type: INT64
      semantic_type: ST_NONE
      name: "a"
    }
    args {
      data_type: FLOAT64
      semantic_type: ST_NONE
      name: "b"
    }
  }
})";

// Tests whether we can get viz funcs info for a given query.
TEST_F(PlannerExportTest, get_viz_funcs_info) {
  planner_ = MakePlanner();
  int result_len;
  auto interface_result = PlannerVizFuncsInfoGoStr(planner_, kVizFuncsQuery, &result_len);

  ASSERT_OK(interface_result);
  pl::shared::scriptspb::VizFuncsInfoResult viz_funcs_result;
  ASSERT_TRUE(viz_funcs_result.ParseFromString(interface_result.ConsumeValueOrDie()));
  EXPECT_OK(viz_funcs_result.status());
  EXPECT_THAT(viz_funcs_result.info(), EqualsProto(kExpectedVizFuncsInfoPb));
}

}  // namespace planner
}  // namespace carnot
}  // namespace pl
