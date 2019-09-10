#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/distributed_planner.h"
#include "src/carnot/compiler/distributedpb/test_proto.h"
#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/metadata_handler.h"
#include "src/carnot/compiler/rule_mock.h"
#include "src/carnot/compiler/rules.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/testing/protobuf.h"

#include "src/carnot/compiler/logical_planner/logical_planner.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace logical_planner {
using pl::testing::proto::EqualsProto;

const char* kTwoAgentOneKelvinDistributedState = R"proto(
carnot_info {
  query_broker_address: "agent1"
  has_grpc_server: false
  has_data_store: true
  processes_data: true
  accepts_remote_sources: false
  table_info {
    table: "table1"
    tabletization_key: "upid"
    tablets: "1"
    tablets: "2"
  }
}
carnot_info {
  query_broker_address: "agent2"
  has_grpc_server: false
  has_data_store: true
  processes_data: true
  accepts_remote_sources: false
  table_info {
    table: "table1"
    tabletization_key: "upid"
    tablets: "3"
    tablets: "4"
  }
}
carnot_info {
  query_broker_address: "kelvin"
  grpc_address: "1111"
  has_grpc_server: true
  has_data_store: false
  processes_data: true
  accepts_remote_sources: true
}
)proto";

class LogicalPlannerTest : public ::testing::Test {
 protected:
};

// Future test when we actually start using kelvin nodes in the system (dependent upon a later
// diff (PL-873)).
// TODO(zasgar) (PL-873) enable test and disable/remove many_agents test.
TEST_F(LogicalPlannerTest, DISABLED_two_agents_one_kelvin) {
  auto planner = LogicalPlanner::Create().ConsumeValueOrDie();
  auto plan = planner
                  ->Plan(distributedpb::testutils::CreateTwoAgentsOneKelvinPlannerState(),
                         distributedpb::testutils::kQueryForTwoAgents)
                  .ConsumeValueOrDie();
  auto out_pb = plan->ToProto().ConsumeValueOrDie();
  EXPECT_THAT(out_pb,
              Partially(EqualsProto(distributedpb::testutils::kExpectedPlanTwoAgentOneKelvin)));
}

TEST_F(LogicalPlannerTest, many_agents) {
  auto planner = LogicalPlanner::Create().ConsumeValueOrDie();
  auto plan = planner
                  ->Plan(distributedpb::testutils::CreateTwoAgentsPlannerState(),
                         distributedpb::testutils::kQueryForTwoAgents)
                  .ConsumeValueOrDie();
  auto out_pb = plan->ToProto().ConsumeValueOrDie();
  EXPECT_THAT(out_pb, Partially(EqualsProto(distributedpb::testutils::kExpectedPlanTwoAgents)));
}

}  // namespace logical_planner
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
