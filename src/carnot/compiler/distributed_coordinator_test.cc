#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/distributed_coordinator.h"
#include "src/carnot/compiler/distributed_planner.h"
#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/logical_planner/test_utils.h"
#include "src/carnot/compiler/metadata_handler.h"
#include "src/carnot/compiler/rule_mock.h"
#include "src/carnot/compiler/rules.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"

#include "src/common/testing/protobuf.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace distributed {
using logical_planner::testutils::kOneAgentOneKelvinDistributedState;
using logical_planner::testutils::kOneAgentThreeKelvinsDistributedState;
using logical_planner::testutils::kThreeAgentsOneKelvinDistributedState;
using ::pl::testing::proto::EqualsProto;
using ::pl::testing::proto::Partially;
using ::testing::ContainsRegex;
using ::testing::ElementsAre;

class CoordinatorTest : public OperatorTests {
 protected:
  distributedpb::DistributedState LoadDistributedStatePb(const std::string& physical_state_txt) {
    distributedpb::DistributedState physical_state_pb;
    CHECK(google::protobuf::TextFormat::MergeFromString(physical_state_txt, &physical_state_pb));
    return physical_state_pb;
  }

  void MakeGraph() {
    auto mem_src = MakeMemSource(MakeRelation());
    auto mem_sink = MakeMemSink(mem_src, "out");
    PL_CHECK_OK(mem_sink->SetRelation(MakeRelation()));
  }

  template <typename TIR>
  TIR* GetEquivalentInNewPlan(IR* new_graph, TIR* old_node) {
    DCHECK(new_graph->HasNode(old_node->id()));
    IRNode* new_node = new_graph->Get(old_node->id());
    DCHECK_EQ(new_node->type(), old_node->type());
    return static_cast<TIR*>(new_node);
  }
  void VerifyHasDataSourcePlan(IR* plan) {
    auto mem_src_nodes = plan->FindNodesOfType(IRNodeType::kMemorySource);
    ASSERT_EQ(mem_src_nodes.size(), 1);
    MemorySourceIR* mem_src = static_cast<MemorySourceIR*>(mem_src_nodes[0]);
    ASSERT_EQ(mem_src->Children().size(), 1);
    EXPECT_TRUE(Match(mem_src->Children()[0], GRPCSink()));
  }

  // Verifies whether the PEM plan matches what we expect.
  void VerifyPEMPlan(IR* plan) {
    // Should have 4 operators.
    EXPECT_EQ(plan->FindNodesThatMatch(Operator()).size(), 2);
    SCOPED_TRACE("Verify PEM plan");
    VerifyHasDataSourcePlan(plan);
  }

  // Verifies whether the Kelvin Merger plan matches what we expect.
  void VerifyKelvinMergerPlan(IR* plan) {
    EXPECT_EQ(plan->FindNodesThatMatch(Operator()).size(), 4);
    SCOPED_TRACE("Verify Kelvin merger plan");
    VerifyHasDataSourcePlan(plan);

    auto grpc_src_nodes = plan->FindNodesOfType(IRNodeType::kGRPCSourceGroup);
    ASSERT_EQ(grpc_src_nodes.size(), 1);
    GRPCSourceGroupIR* grpc_src = static_cast<GRPCSourceGroupIR*>(grpc_src_nodes[0]);
    ASSERT_EQ(grpc_src->Children().size(), 1);
    EXPECT_TRUE(Match(grpc_src->Children()[0], MemorySink()))
        << grpc_src->Children()[0]->DebugString();
  }
};

TEST_F(CoordinatorTest, one_pem_one_kelvin) {
  auto ps = LoadDistributedStatePb(kOneAgentOneKelvinDistributedState);
  auto coordinator = Coordinator::Create(ps).ConsumeValueOrDie();

  MakeGraph();
  auto physical_plan = coordinator->Coordinate(graph.get()).ConsumeValueOrDie();
  ASSERT_EQ(physical_plan->dag().nodes().size(), 2UL);
  EXPECT_THAT(physical_plan->dag().TopologicalSort(), ElementsAre(1, 0));

  auto kelvin_instance = physical_plan->Get(0);
  EXPECT_THAT(kelvin_instance->carnot_info().query_broker_address(), ContainsRegex("kelvin"));
  {
    SCOPED_TRACE("one agent one kelvin -> kelvin plan");
    VerifyKelvinMergerPlan(kelvin_instance->plan());
  }

  auto pem_instance = physical_plan->Get(1);
  EXPECT_THAT(pem_instance->carnot_info().query_broker_address(), ContainsRegex("agent"));
  {
    SCOPED_TRACE("one agent one kelvin -> pem plan");
    VerifyPEMPlan(pem_instance->plan());
  }
}

TEST_F(CoordinatorTest, three_pems_one_kelvin) {
  auto ps = LoadDistributedStatePb(kThreeAgentsOneKelvinDistributedState);
  auto coordinator = Coordinator::Create(ps).ConsumeValueOrDie();

  MakeGraph();
  auto physical_plan = coordinator->Coordinate(graph.get()).ConsumeValueOrDie();
  EXPECT_EQ(physical_plan->dag().nodes().size(), 4UL);
  ASSERT_THAT(physical_plan->dag().TopologicalSort(), ElementsAre(3, 2, 1, 0));
  auto kelvin_instance = physical_plan->Get(0);
  EXPECT_THAT(kelvin_instance->carnot_info().query_broker_address(), ContainsRegex("kelvin"));
  {
    SCOPED_TRACE("three pems one kelvin -> " +
                 kelvin_instance->carnot_info().query_broker_address());
    VerifyKelvinMergerPlan(kelvin_instance->plan());
  }

  // Agents are 1,2,3.
  for (int64_t i = 1; i <= 3; ++i) {
    auto pem_instance = physical_plan->Get(i);
    SCOPED_TRACE("three pems one kelvin -> " + pem_instance->carnot_info().query_broker_address());
    EXPECT_THAT(pem_instance->carnot_info().query_broker_address(), ContainsRegex("agent"));
    VerifyPEMPlan(pem_instance->plan());
  }
}

TEST_F(CoordinatorTest, one_pem_three_kelvin) {
  auto ps = LoadDistributedStatePb(kOneAgentThreeKelvinsDistributedState);
  auto coordinator = Coordinator::Create(ps).ConsumeValueOrDie();

  MakeGraph();

  auto physical_plan = coordinator->Coordinate(graph.get()).ConsumeValueOrDie();
  ASSERT_EQ(physical_plan->dag().nodes().size(), 2UL);
  EXPECT_THAT(physical_plan->dag().TopologicalSort(), ElementsAre(1, 0));

  auto kelvin_instance = physical_plan->Get(0);
  EXPECT_THAT(kelvin_instance->carnot_info().query_broker_address(), ContainsRegex("kelvin"));
  {
    SCOPED_TRACE("one agent one kelvin -> kelvin plan");
    VerifyKelvinMergerPlan(kelvin_instance->plan());
  }

  auto pem_instance = physical_plan->Get(1);
  EXPECT_THAT(pem_instance->carnot_info().query_broker_address(), ContainsRegex("agent"));
  {
    SCOPED_TRACE("one agent one kelvin -> pem plan");
    VerifyPEMPlan(pem_instance->plan());
  }
}

constexpr char kBadAgentSpecificationState[] = R"proto(
carnot_info {
  query_broker_address: "agent"
  has_grpc_server: false
  has_data_store: true
  processes_data: false
  accepts_remote_sources: false
  asid: 123
}
carnot_info {
  query_broker_address: "kelvin1"
  grpc_address: "1111"
  has_grpc_server: true
  has_data_store: false
  processes_data: true
  accepts_remote_sources: true
  asid: 456
}
)proto";

TEST_F(CoordinatorTest, bad_agent_spec) {
  auto ps = LoadDistributedStatePb(kBadAgentSpecificationState);
  auto coordinator_status = Coordinator::Create(ps);

  ASSERT_NOT_OK(coordinator_status);
  EXPECT_EQ(coordinator_status.status().msg(),
            "Distributed state does not have a Carnot instance that satisifies the condition "
            "`has_data_store() && processes_data()`.");
}

constexpr char kBadKelvinSpecificationState[] = R"proto(
carnot_info {
  query_broker_address: "agent"
  has_grpc_server: false
  has_data_store: true
  processes_data: true
  accepts_remote_sources: false
  asid: 123
}
carnot_info {
  query_broker_address: "kelvin1"
  grpc_address: "1111"
  has_grpc_server: true
  has_data_store: false
  processes_data: false
  accepts_remote_sources: true
  asid: 456
}
)proto";

TEST_F(CoordinatorTest, bad_kelvin_spec) {
  auto ps = LoadDistributedStatePb(kBadKelvinSpecificationState);
  auto coordinator_status = Coordinator::Create(ps);

  ASSERT_NOT_OK(coordinator_status);
  EXPECT_EQ(coordinator_status.status().msg(),
            "Distributed state does not have a Carnot instance that satisifies the condition "
            "`processes_data() && accepts_remote_sources()`.");
}

}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
