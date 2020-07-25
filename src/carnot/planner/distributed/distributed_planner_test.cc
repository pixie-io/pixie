#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/distributed_planner.h"
#include "src/carnot/planner/ir/ir_nodes.h"
#include "src/carnot/planner/rules/rules.h"
#include "src/carnot/planner/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/testing/protobuf.h"

namespace pl {
namespace carnot {
namespace planner {
namespace distributed {
using pl::testing::proto::EqualsProto;
using ::testing::ContainsRegex;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAreArray;
using testutils::kThreePEMsOneKelvinDistributedState;

constexpr char kOnePEMOneKelvinDistributedState[] = R"proto(
carnot_info {
  query_broker_address: "pem"
  agent_id {
    data: "00000001-0000-0000-0000-000000000001"
  }
  has_grpc_server: false
  has_data_store: true
  processes_data: true
  accepts_remote_sources: false
  table_info {
    table: "table"
  }
}
carnot_info {
  query_broker_address: "kelvin"
  agent_id {
    data: "00000001-0000-0000-0000-000000000002"
  }
  grpc_address: "1111"
  has_grpc_server: true
  has_data_store: false
  processes_data: true
  accepts_remote_sources: true
}
schema_info {
  name: "table"
  relation {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "cpu_cycles"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_NONE
    }
  }
  agent_list {
    data: "00000001-0000-0000-0000-000000000001"
  }
  agent_list {
    data: "00000001-0000-0000-0000-000000000002"
  }
}
)proto";

class DistributedPlannerTest : public OperatorTests {
 protected:
  void SetUpImpl() override { compiler_state_ = nullptr; }
  distributedpb::DistributedState LoadDistributedStatePb(const std::string& physical_state_txt) {
    distributedpb::DistributedState physical_state_pb;
    CHECK(google::protobuf::TextFormat::MergeFromString(physical_state_txt, &physical_state_pb));
    return physical_state_pb;
  }

  std::unique_ptr<CompilerState> compiler_state_;
};

TEST_F(DistributedPlannerTest, one_pem_one_kelvin) {
  auto mem_src = MakeMemSource(MakeRelation());
  auto mem_sink = MakeMemSink(mem_src, "out");
  PL_CHECK_OK(mem_sink->SetRelation(MakeRelation()));

  distributedpb::DistributedState ps_pb = LoadDistributedStatePb(kOnePEMOneKelvinDistributedState);
  std::unique_ptr<DistributedPlanner> physical_planner =
      DistributedPlanner::Create().ConsumeValueOrDie();
  // TODO(philkuz) fix nullptr for compiler_state.
  std::unique_ptr<DistributedPlan> physical_plan =
      physical_planner->Plan(ps_pb, compiler_state_.get(), graph.get()).ConsumeValueOrDie();

  ASSERT_THAT(physical_plan->dag().TopologicalSort(), ElementsAre(1, 0));

  // Agent should be plan 1.
  auto agent_instance = physical_plan->Get(1);
  EXPECT_THAT(agent_instance->carnot_info().query_broker_address(), ContainsRegex("pem"));

  std::vector<IRNode*> grpc_sinks = agent_instance->plan()->FindNodesOfType(IRNodeType::kGRPCSink);
  ASSERT_EQ(grpc_sinks.size(), 1);
  auto grpc_sink = static_cast<GRPCSinkIR*>(grpc_sinks[0]);

  auto kelvin_instance = physical_plan->Get(0);
  EXPECT_THAT(kelvin_instance->carnot_info().query_broker_address(), ContainsRegex("kelvin"));

  std::vector<IRNode*> grpc_sources =
      kelvin_instance->plan()->FindNodesOfType(IRNodeType::kGRPCSource);
  EXPECT_EQ(grpc_sources.size(), 1);
  ASSERT_EQ(grpc_sources[0]->type(), IRNodeType::kGRPCSource);

  auto grpc_source = static_cast<GRPCSourceIR*>(grpc_sources[0]);
  // Make sure that the destinations are setup properly.
  EXPECT_THAT(grpc_sink->destination_id(), grpc_source->id());
}

TEST_F(DistributedPlannerTest, three_agents_one_kelvin) {
  auto mem_src = MakeMemSource(MakeRelation());
  auto mem_sink = MakeMemSink(mem_src, "out");
  PL_CHECK_OK(mem_sink->SetRelation(MakeRelation()));

  distributedpb::DistributedState ps_pb =
      LoadDistributedStatePb(kThreePEMsOneKelvinDistributedState);
  std::unique_ptr<DistributedPlanner> physical_planner =
      DistributedPlanner::Create().ConsumeValueOrDie();
  std::unique_ptr<DistributedPlan> physical_plan =
      physical_planner->Plan(ps_pb, compiler_state_.get(), graph.get()).ConsumeValueOrDie();

  ASSERT_THAT(physical_plan->dag().TopologicalSort(), ElementsAre(3, 2, 1, 0));

  // Agents should be ids 1,2,3.
  std::vector<int64_t> grpc_sink_destinations;
  for (int64_t i = 1; i <= 3; ++i) {
    SCOPED_TRACE(absl::Substitute("agent id = $0", i));
    auto agent_instance = physical_plan->Get(i);
    EXPECT_THAT(agent_instance->carnot_info().query_broker_address(), ContainsRegex("pem"));

    std::vector<IRNode*> grpc_sinks =
        agent_instance->plan()->FindNodesOfType(IRNodeType::kGRPCSink);
    ASSERT_EQ(grpc_sinks.size(), 1);
    auto grpc_sink = static_cast<GRPCSinkIR*>(grpc_sinks[0]);
    grpc_sink_destinations.push_back(grpc_sink->destination_id());
  }

  auto kelvin_instance = physical_plan->Get(0);
  EXPECT_THAT(kelvin_instance->carnot_info().query_broker_address(), ContainsRegex("kelvin"));

  std::vector<IRNode*> nodes = kelvin_instance->plan()->FindNodesOfType(IRNodeType::kUnion);
  ASSERT_EQ(nodes.size(), 1);
  UnionIR* kelvin_union = static_cast<UnionIR*>(nodes[0]);
  EXPECT_EQ(kelvin_union->parents().size(), 3);
  std::vector<int64_t> grpc_source_ids;
  for (OperatorIR* union_parent : kelvin_union->parents()) {
    ASSERT_EQ(union_parent->type(), IRNodeType::kGRPCSource);
    auto grpc_source = static_cast<GRPCSourceIR*>(union_parent);
    grpc_source_ids.push_back(grpc_source->id());
  }

  // Make sure that the destinations are setup properly.
  EXPECT_THAT(grpc_sink_destinations, UnorderedElementsAreArray(grpc_source_ids));
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
