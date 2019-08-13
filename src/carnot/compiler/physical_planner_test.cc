#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/metadata_handler.h"
#include "src/carnot/compiler/physical_planner.h"
#include "src/carnot/compiler/rule_mock.h"
#include "src/carnot/compiler/rules.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace physical {

const char* kOneAgentPhysicalState = R"proto(
carnot_info {
  query_broker_address: "agent"
  has_grpc_server: false
  has_data_store: true
  processes_data: true
  accepts_remote_sources: false
}
carnot_info {
  query_broker_address: "kelvin"
  grpc_address: "1111"
  has_grpc_server: true
  has_data_store: false
  processes_data: false
  accepts_remote_sources: true
}
)proto";

class PhysicalPlannerTest : public ::testing::Test {
 protected:
  void SetUp() override { ::testing::Test::SetUp(); }
  std::unique_ptr<IR> QueryToLogicalPlan(const std::string& query) {
    // TODO(philkuz) refactor what's in compiler.cc/compiler.h into LogicalPlanner and connect it
    // with this.
    PL_UNUSED(query);
    return std::make_unique<IR>();
  }
  compilerpb::PhysicalState LoadPhysicalStatePb(const std::string& physical_state_txt) {
    compilerpb::PhysicalState physical_state_pb;
    CHECK(google::protobuf::TextFormat::MergeFromString(physical_state_txt, &physical_state_pb));
    return physical_state_pb;
  }
};

TEST_F(PhysicalPlannerTest, can_compile) {
  std::string query;
  std::unique_ptr<IR> logical_plan = QueryToLogicalPlan(query);
  compilerpb::PhysicalState ps_pb = LoadPhysicalStatePb(kOneAgentPhysicalState);
  std::unique_ptr<PhysicalPlanner> physical_planner =
      PhysicalPlanner::Create(ps_pb).ConsumeValueOrDie();
  std::unique_ptr<PhysicalPlan> physical_plan =
      physical_planner->Plan(logical_plan.get()).ConsumeValueOrDie();
}

}  // namespace physical

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
