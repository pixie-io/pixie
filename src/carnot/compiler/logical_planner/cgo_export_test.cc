#include "src/carnot/compiler/logical_planner/cgo_export.h"

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <string>
#include <vector>

#include "absl/strings/str_join.h"
#include "src/carnot/compiler/compilerpb/compiler_status.pb.h"
#include "src/carnot/compiler/distributedpb/distributed_plan.pb.h"
#include "src/carnot/compiler/distributedpb/test_proto.h"
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
using pl::testing::proto::EqualsProto;
using pl::testing::proto::Partially;

class PlannerExportTest : public ::testing::Test {
 protected:
  void SetUp() override {
    planner_ = PlannerNew();
    // Setup the schema from a proto.
  }
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

// TODO(philkuz/zasgar) (PL-873) remove or disable this test when enabling kelvin support.
TEST_F(PlannerExportTest, two_agents_query_test) {
  int result_len;
  std::string query = "queryDF = dataframe(table='table1').result(name='out')";

  auto logical_planner_state = distributedpb::testutils::CreateTwoAgentsPlannerState();
  auto interface_result =
      PlannerPlanGoStr(planner_, logical_planner_state.DebugString(), query, &result_len);
  ASSERT_OK(interface_result);

  distributedpb::LogicalPlannerResult planner_result;
  ASSERT_TRUE(planner_result.ParseFromString(interface_result.ConsumeValueOrDie()));
  ASSERT_OK(planner_result.status());
  std::string expected_planner_result_str =
      absl::Substitute("plan {$0}", distributedpb::testutils::kExpectedPlanTwoAgents);
  EXPECT_THAT(planner_result, Partially(EqualsProto(expected_planner_result_str)))
      << planner_result.DebugString();
}

// TODO(philkuz/zasgar) (PL-873) enable this test when switching to use Kelvin.
TEST_F(PlannerExportTest, DISABLED_one_agent_one_kelvin_query_test) {
  int result_len;
  std::string query = "queryDF = dataframe(table='table1').result(name='out')";

  auto logical_planner_state = distributedpb::testutils::CreateTwoAgentsOneKelvinPlannerState();
  auto interface_result =
      PlannerPlanGoStr(planner_, logical_planner_state.DebugString(), query, &result_len);
  ASSERT_OK(interface_result);

  distributedpb::LogicalPlannerResult planner_result;
  ASSERT_TRUE(planner_result.ParseFromString(interface_result.ConsumeValueOrDie()));
  ASSERT_OK(planner_result.status());
  std::string expected_planner_result_str =
      absl::Substitute("plan {$0}", distributedpb::testutils::kExpectedPlanTwoAgentOneKelvin);
  EXPECT_THAT(planner_result, Partially(EqualsProto(expected_planner_result_str)))
      << planner_result.DebugString();
}

TEST_F(PlannerExportTest, bad_queries) {
  int result_len;
  // Bad table name query that should yield a compiler error.
  std::string bad_table_query = "queryDF = dataframe(table='bad_table_name').result(name='out')";
  auto logical_planner_state = distributedpb::testutils::CreateTwoAgentsPlannerState();
  auto interface_result =
      PlannerPlanGoStr(planner_, logical_planner_state.DebugString(), bad_table_query, &result_len);
  // The compiler should successfully compile and a proto should be returned.
  ASSERT_OK(interface_result);
  distributedpb::LogicalPlannerResult planner_result_pb;
  ASSERT_TRUE(planner_result_pb.ParseFromString(interface_result.ConsumeValueOrDie()));
  EXPECT_NOT_OK(planner_result_pb.status());
  EXPECT_THAT(planner_result_pb.status(), HasCompilerError("Table 'bad_table_name' not found."));
}

const char* kUDFQuery = R"query(
t1 = dataframe(table='table1', start_time='-30s')
t1 = t1[t1['cpu_cycles'] >= 0]
t1.result(name="")
)query";

// Previously had an issue where the UDF registry's memory was improperly handled, and this query
// would cause a segfault. If this unit test passes, then that bug should be gone.
TEST_F(PlannerExportTest, udf_in_query) {
  auto logical_planner_state = distributedpb::testutils::CreateTwoAgentsPlannerState();
  int result_len;
  auto interface_result =
      PlannerPlanGoStr(planner_, logical_planner_state.DebugString(), kUDFQuery, &result_len);
  // The compiler should successfully compile and a proto should be returned.
  ASSERT_OK(interface_result);
  distributedpb::LogicalPlannerResult planner_result_pb;
  ASSERT_TRUE(planner_result_pb.ParseFromString(interface_result.ConsumeValueOrDie()));
  EXPECT_OK(planner_result_pb.status());
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
