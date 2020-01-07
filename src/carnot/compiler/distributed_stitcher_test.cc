#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/distributed_coordinator.h"
#include "src/carnot/compiler/distributed_plan.h"
#include "src/carnot/compiler/distributed_planner.h"
#include "src/carnot/compiler/distributed_stitcher.h"
#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/logical_planner/test_utils.h"
#include "src/carnot/compiler/metadata_handler.h"
#include "src/carnot/compiler/rule_mock.h"
#include "src/carnot/compiler/rules.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace distributed {
using logical_planner::testutils::kOneAgentOneKelvinDistributedState;
using logical_planner::testutils::kOneAgentThreeKelvinsDistributedState;
using logical_planner::testutils::kThreeAgentsOneKelvinDistributedState;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

class StitcherTest : public OperatorTests {
 protected:
  void SetUpImpl() override {
    auto rel_map = std::make_unique<RelationMap>();
    auto cpu_relation = table_store::schema::Relation(
        std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64,
                                      types::DataType::FLOAT64, types::DataType::FLOAT64}),
        std::vector<std::string>({"count", "cpu0", "cpu1", "cpu2"}));
    rel_map->emplace("cpu", cpu_relation);

    info_ = std::make_unique<compiler::RegistryInfo>();
    compiler_state_ = std::make_unique<CompilerState>(std::move(rel_map), info_.get(), time_now);
  }
  distributedpb::DistributedState LoadDistributedStatePb(const std::string& physical_state_txt) {
    distributedpb::DistributedState physical_state_pb;
    CHECK(google::protobuf::TextFormat::MergeFromString(physical_state_txt, &physical_state_pb));
    return physical_state_pb;
  }

  void MakeSourceSinkGraph() {
    auto mem_source = MakeMemSource(MakeRelation());
    auto mem_sink = MakeMemSink(mem_source, "out");
    PL_CHECK_OK(mem_sink->SetRelation(MakeRelation()));
  }

  std::unique_ptr<DistributedPlan> MakeDistributedPlan(const distributedpb::DistributedState& ps) {
    auto coordinator = Coordinator::Create(ps).ConsumeValueOrDie();

    MakeSourceSinkGraph();

    auto physical_plan = coordinator->Coordinate(graph.get()).ConsumeValueOrDie();
    return physical_plan;
  }

  std::unique_ptr<CompilerState> compiler_state_;
  std::unique_ptr<compiler::RegistryInfo> info_;
  int64_t time_now = 1552607213931245000;
};

TEST_F(StitcherTest, one_agent_one_kelvin) {
  auto ps = LoadDistributedStatePb(kOneAgentOneKelvinDistributedState);
  auto physical_plan = MakeDistributedPlan(ps);

  EXPECT_THAT(physical_plan->dag().TopologicalSort(), ElementsAre(1, 0));
  CarnotInstance* kelvin = physical_plan->Get(0);
  IR* kelvin_plan = kelvin->plan();

  std::string kelvin_qb_address = "kelvin";
  ASSERT_EQ(kelvin->carnot_info().query_broker_address(), kelvin_qb_address);

  CarnotInstance* agent = physical_plan->Get(1);
  IR* agent_plan = agent->plan();

  // Make sure that the grpc_sink stuff is not yet stitched to avoid a false positive.
  for (int64_t node_i : agent_plan->dag().TopologicalSort()) {
    IRNode* ir_node = agent_plan->Get(node_i);
    if (Match(ir_node, GRPCSink())) {
      auto grpc_sink = static_cast<GRPCSinkIR*>(ir_node);
      EXPECT_FALSE(grpc_sink->DestinationAddressSet());
    }
  }

  for (int64_t node_i : kelvin_plan->dag().TopologicalSort()) {
    IRNode* ir_node = kelvin_plan->Get(node_i);
    if (Match(ir_node, GRPCSourceGroup())) {
      auto grpc_source_group = static_cast<GRPCSourceGroupIR*>(ir_node);
      EXPECT_FALSE(grpc_source_group->GRPCAddressSet());
    }
  }

  // Execute the stitcher.
  auto stitcher = Stitcher::Create(compiler_state_.get()).ConsumeValueOrDie();
  EXPECT_OK(stitcher->Stitch(physical_plan.get()));

  std::vector<int64_t> source_ids;
  std::vector<int64_t> destination_ids;

  // In the Kelvin plan, make sure GRPCSourceGroups don't show up in the resulting plan.
  for (int64_t node_i : kelvin_plan->dag().TopologicalSort()) {
    IRNode* ir_node = kelvin_plan->Get(node_i);
    EXPECT_FALSE(Match(ir_node, GRPCSourceGroup())) << absl::Substitute(
        "GRPCSourceGroup should not appear in processed plan. Got it for node: $0",
        ir_node->DebugString());
    // Test GRPCSources for whether they have the expected input set.
    if (Match(ir_node, GRPCSource())) {
      source_ids.emplace_back(ir_node->id());
    }
  }

  for (int64_t node_i : agent_plan->dag().TopologicalSort()) {
    IRNode* ir_node = agent_plan->Get(node_i);
    if (Match(ir_node, GRPCSink())) {
      auto sink = static_cast<GRPCSinkIR*>(ir_node);
      // Test GRPCSinks for expected GRPC destination address, as well as the proper physical id
      // being set, as well as being set to the correct value.
      EXPECT_TRUE(sink->DestinationAddressSet());
      destination_ids.emplace_back(sink->destination_id());
    }
  }

  EXPECT_THAT(source_ids, UnorderedElementsAreArray(destination_ids));
}

TEST_F(StitcherTest, three_agents_one_kelvin) {
  auto ps = LoadDistributedStatePb(kThreeAgentsOneKelvinDistributedState);
  auto physical_plan = MakeDistributedPlan(ps);

  EXPECT_THAT(physical_plan->dag().TopologicalSort(), ElementsAre(3, 2, 1, 0));

  CarnotInstance* kelvin = physical_plan->Get(0);
  IR* kelvin_plan = kelvin->plan();
  std::string kelvin_qb_address = "kelvin";
  ASSERT_EQ(kelvin->carnot_info().query_broker_address(), kelvin_qb_address);

  std::vector<CarnotInstance*> agents;
  std::vector<IR*> agent_plans;
  for (int64_t agent_id = 1; agent_id <= 3; ++agent_id) {
    CarnotInstance* agent = physical_plan->Get(agent_id);
    // Quick check to make sure agents are valid.
    ASSERT_THAT(agent->carnot_info().query_broker_address(), HasSubstr("agent"));
    agents.push_back(agent);
    agent_plans.push_back(agent->plan());
  }

  // Make sure none of the functionality si taken care of so we can detect that it does get tatken
  // care of.
  for (const auto& agent_plan : agent_plans) {
    for (int64_t node_i : agent_plan->dag().TopologicalSort()) {
      IRNode* ir_node = agent_plan->Get(node_i);
      if (Match(ir_node, GRPCSink())) {
        auto grpc_sink = static_cast<GRPCSinkIR*>(ir_node);
        EXPECT_FALSE(grpc_sink->DestinationAddressSet());
      }
    }
  }

  for (int64_t node_i : kelvin_plan->dag().TopologicalSort()) {
    IRNode* ir_node = kelvin_plan->Get(node_i);
    if (Match(ir_node, GRPCSourceGroup())) {
      auto grpc_source_group = static_cast<GRPCSourceGroupIR*>(ir_node);
      EXPECT_FALSE(grpc_source_group->GRPCAddressSet());
    }
  }

  // Execute the stitcher.
  auto stitcher = Stitcher::Create(compiler_state_.get()).ConsumeValueOrDie();
  EXPECT_OK(stitcher->Stitch(physical_plan.get()));
  // Save the remote source ids from the operators.
  std::vector<int64_t> source_ids;
  std::vector<int64_t> destination_ids;

  // In the Kelvin plan, make sure GRPCSourceGroups don't show up in the resulting plan.
  for (int64_t node_i : kelvin_plan->dag().TopologicalSort()) {
    IRNode* ir_node = kelvin_plan->Get(node_i);
    EXPECT_FALSE(Match(ir_node, GRPCSourceGroup())) << absl::Substitute(
        "GRPCSourceGroup should not appear in processed plan. Got it for node: $0",
        ir_node->DebugString());
    // Test GRPCSources for whether they have the expected input set.
    if (Match(ir_node, GRPCSource())) {
      auto source = static_cast<GRPCSourceIR*>(ir_node);
      source_ids.push_back(source->id());
    }
  }

  for (const auto& agent_plan : agent_plans) {
    for (int64_t node_i : agent_plan->dag().TopologicalSort()) {
      IRNode* ir_node = agent_plan->Get(node_i);
      if (Match(ir_node, GRPCSink())) {
        auto sink = static_cast<GRPCSinkIR*>(ir_node);
        // Test GRPCSinks for expected GRPC destination address, as well as the proper physical id
        // being set, as well as being set to the correct value.
        EXPECT_TRUE(sink->DestinationAddressSet());
        destination_ids.push_back(sink->destination_id());
      }
    }
  }

  // Check to see that all of the physical ids are in the remote_source_ids.
  EXPECT_THAT(source_ids, UnorderedElementsAreArray(destination_ids));
}

}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
