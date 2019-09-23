#include "src/carnot/compiler/logical_planner/cgo_export.h"

#include <glog/logging.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
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

StatusOr<std::string> PlannerPlanGoStr(PlannerPtr planner_ptr, std::string schema,
                                       std::string query, int* resultLen) {
  char* result = PlannerPlan(planner_ptr, schema.c_str(), schema.length(), query.c_str(),
                             query.length(), resultLen);
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
  std::string query = "queryDF = From(table='table1').Result(name='out')";

  auto logical_planner_state = distributedpb::testutils::CreateTwoAgentsPlannerState();
  auto interface_result =
      PlannerPlanGoStr(planner_, logical_planner_state.DebugString(), query, &result_len);
  ASSERT_OK(interface_result);

  distributedpb::LogicalPlannerResult planner_result;
  ASSERT_TRUE(planner_result.ParseFromString(interface_result.ConsumeValueOrDie()));
  ASSERT_OK(planner_result.status());
  std::string expected_planner_result_str =
      absl::Substitute("plan {$0}", distributedpb::testutils::kExpectedPlanTwoAgents);
  EXPECT_THAT(planner_result, Partially(EqualsProto(expected_planner_result_str)));
}

// TODO(philkuz/zasgar) (PL-873) enable this test when switching to use Kelvin.
TEST_F(PlannerExportTest, DISABLED_one_agent_one_kelvin_query_test) {
  int result_len;
  std::string query = "queryDF = From(table='table1').Result(name='out')";

  auto logical_planner_state = distributedpb::testutils::CreateTwoAgentsOneKelvinPlannerState();
  auto interface_result =
      PlannerPlanGoStr(planner_, logical_planner_state.DebugString(), query, &result_len);
  ASSERT_OK(interface_result);

  distributedpb::LogicalPlannerResult planner_result;
  ASSERT_TRUE(planner_result.ParseFromString(interface_result.ConsumeValueOrDie()));
  ASSERT_OK(planner_result.status());
  std::string expected_planner_result_str =
      absl::Substitute("plan {$0}", distributedpb::testutils::kExpectedPlanTwoAgentOneKelvin);
  EXPECT_THAT(planner_result, Partially(EqualsProto(expected_planner_result_str)));
}

TEST_F(PlannerExportTest, bad_queries) {
  int result_len;
  // Bad table name query that should yield a compiler error.
  std::string bad_table_query = "queryDF = From(table='bad_table_name').Result(name='out')";
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
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
