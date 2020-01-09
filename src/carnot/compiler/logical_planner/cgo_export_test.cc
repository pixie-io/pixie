#include "src/carnot/compiler/logical_planner/cgo_export.h"

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <string>
#include <vector>

#include <absl/strings/str_join.h>
#include "src/carnot/compiler/compilerpb/compiler_status.pb.h"
#include "src/carnot/compiler/distributedpb/distributed_plan.pb.h"
#include "src/carnot/compiler/logical_planner/test_utils.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/error.h"
#include "src/common/base/macros.h"
#include "src/common/base/statusor.h"
#include "src/common/testing/protobuf.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace logical_planner {

using pl::testing::proto::EqualsProto;
using pl::testing::proto::Partially;

class PlannerExportTest : public ::testing::Test {
 protected:
  void TearDown() override { PlannerFree(planner_); }
  PlannerPtr planner_;
};

StatusOr<std::string> PlannerPlanGoStr(PlannerPtr planner_ptr, std::string planner_state,
                                       std::string query, int* resultLen) {
  char* result = PlannerPlan(planner_ptr, planner_state.c_str(), planner_state.length(),
                             query.c_str(), query.length(), resultLen);
  if (*resultLen == 0) {
    return error::InvalidArgument("Planner failed to return.");
  }

  std::string lp_str(result, result + *resultLen);
  delete[] result;
  return lp_str;
}

// TODO(philkuz/nserrino): Fix test broken with clang-9/gcc-9.
TEST_F(PlannerExportTest, DISABLED_two_agents_query_test) {
  planner_ = PlannerNew(/* distributed */ false);
  int result_len;
  std::string query = "df = pl.DataFrame(table='table1')\npl.display(df, 'out')";

  auto logical_planner_state = testutils::CreateTwoAgentsPlannerState();
  auto interface_result =
      PlannerPlanGoStr(planner_, logical_planner_state.DebugString(), query, &result_len);
  ASSERT_OK(interface_result);

  distributedpb::LogicalPlannerResult planner_result;
  ASSERT_TRUE(planner_result.ParseFromString(interface_result.ConsumeValueOrDie()));
  ASSERT_OK(planner_result.status());
  std::string expected_planner_result_str =
      absl::Substitute("plan {$0}", testutils::kExpectedPlanTwoAgents);
  EXPECT_THAT(planner_result, Partially(EqualsProto(expected_planner_result_str)))
      << planner_result.DebugString();
}

// TODO(philkuz/nserrino): Fix test broken with clang-9/gcc-9.
TEST_F(PlannerExportTest, DISABLED_one_agent_one_kelvin_query_test) {
  planner_ = PlannerNew(/* distributed */ true);
  int result_len;
  std::string query = "df = pl.DataFrame(table='table1')\npl.display(df, 'out')";

  auto logical_planner_state = testutils::CreateTwoAgentsOneKelvinPlannerState();
  auto interface_result =
      PlannerPlanGoStr(planner_, logical_planner_state.DebugString(), query, &result_len);
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
  planner_ = PlannerNew(/* distributed */ true);
  int result_len;
  // Bad table name query that should yield a compiler error.
  std::string bad_table_query =
      "df = pl.DataFrame(table='bad_table_name')\n"
      "pl.display(df, 'out')";
  auto logical_planner_state = testutils::CreateTwoAgentsPlannerState();
  auto interface_result =
      PlannerPlanGoStr(planner_, logical_planner_state.DebugString(), bad_table_query, &result_len);
  // The compiler should successfully compile and a proto should be returned.
  ASSERT_OK(interface_result);
  distributedpb::LogicalPlannerResult planner_result_pb;
  ASSERT_TRUE(planner_result_pb.ParseFromString(interface_result.ConsumeValueOrDie()));
  EXPECT_NOT_OK(planner_result_pb.status());
  EXPECT_THAT(planner_result_pb.status(), HasCompilerError("Table 'bad_table_name' not found."));
}

constexpr char kUDFQuery[] = R"query(
t1 = pl.DataFrame(table='table1', start_time='-30s')
t1 = t1[t1['cpu_cycles'] >= 0]
pl.display(t1)
)query";

// Previously had an issue where the UDF registry's memory was improperly handled, and this query
// would cause a segfault. If this unit test passes, then that bug should be gone.
TEST_F(PlannerExportTest, udf_in_query) {
  planner_ = PlannerNew(/* distributed */ true);
  auto logical_planner_state = testutils::CreateTwoAgentsOneKelvinPlannerState();
  int result_len;
  auto interface_result =
      PlannerPlanGoStr(planner_, logical_planner_state.DebugString(), kUDFQuery, &result_len);
  // The compiler should successfully compile and a proto should be returned.
  ASSERT_OK(interface_result);
  distributedpb::LogicalPlannerResult planner_result_pb;
  ASSERT_TRUE(planner_result_pb.ParseFromString(interface_result.ConsumeValueOrDie()));
  EXPECT_OK(planner_result_pb.status());
}

}  // namespace logical_planner
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
