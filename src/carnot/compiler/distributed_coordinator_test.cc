#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/distributed_coordinator.h"
#include "src/carnot/compiler/distributed_planner.h"
#include "src/carnot/compiler/distributed_splitter.h"
#include "src/carnot/compiler/distributedpb/test_proto.h"
#include "src/carnot/compiler/ir/ir_nodes.h"
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
using distributedpb::testutils::kOneAgentOneKelvinDistributedState;
using distributedpb::testutils::kOneAgentThreeKelvinsDistributedState;
using distributedpb::testutils::kThreeAgentsOneKelvinDistributedState;
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
};

const char* kAgentSimplePlan = R"proto(
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      sorted_children: 2
    }
    nodes {
      id: 2
    }
  }
  nodes {
    op {
      op_type: MEMORY_SOURCE_OPERATOR
    }
  }
  nodes {
    id: 2
    op {
      op_type: GRPC_SINK_OPERATOR
      grpc_sink_op {
      }
    }
  }
}
)proto";

const char* kKelvinSimplePlan = R"proto(
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 3
      sorted_children: 1
    }
    nodes {
      id: 1
    }
  }
  nodes {
    id: 3
    op {
      op_type: GRPC_SOURCE_OPERATOR
    }
  }
  nodes {
    id: 1
    op {
      op_type: MEMORY_SINK_OPERATOR
    }
  }
}
)proto";

TEST_F(CoordinatorTest, one_agent_one_kelvin) {
  auto ps = LoadDistributedStatePb(kOneAgentOneKelvinDistributedState);
  auto coordinator = Coordinator::Create(ps).ConsumeValueOrDie();

  MakeGraph();
  auto physical_plan = coordinator->Coordinate(graph.get()).ConsumeValueOrDie();
  ASSERT_EQ(physical_plan->dag().nodes().size(), 2UL);
  EXPECT_THAT(physical_plan->dag().TopologicalSort(), ElementsAre(1, 0));

  auto kelvin_instance = physical_plan->Get(0);
  EXPECT_THAT(kelvin_instance->plan()->ToProto().ConsumeValueOrDie(),
              Partially(EqualsProto(kKelvinSimplePlan)));
  EXPECT_THAT(kelvin_instance->carnot_info().query_broker_address(), ContainsRegex("kelvin"));

  // Agents should be instance 1
  auto agent_instance = physical_plan->Get(1);
  EXPECT_THAT(agent_instance->carnot_info().query_broker_address(), ContainsRegex("agent"));

  EXPECT_THAT(agent_instance->plan()->ToProto().ConsumeValueOrDie(),
              Partially(EqualsProto(kAgentSimplePlan)));
}

TEST_F(CoordinatorTest, three_agents_one_kelvin) {
  auto ps = LoadDistributedStatePb(kThreeAgentsOneKelvinDistributedState);
  auto coordinator = Coordinator::Create(ps).ConsumeValueOrDie();

  MakeGraph();
  auto physical_plan = coordinator->Coordinate(graph.get()).ConsumeValueOrDie();
  EXPECT_EQ(physical_plan->dag().nodes().size(), 4UL);
  ASSERT_THAT(physical_plan->dag().TopologicalSort(), ElementsAre(3, 2, 1, 0));
  auto kelvin_instance = physical_plan->Get(0);
  EXPECT_THAT(kelvin_instance->carnot_info().query_broker_address(), ContainsRegex("kelvin"));
  EXPECT_THAT(kelvin_instance->plan()->ToProto().ConsumeValueOrDie(),
              Partially(EqualsProto(kKelvinSimplePlan)));

  // Agents are 1,2,3.
  for (int64_t i = 1; i <= 3; ++i) {
    auto agent_instance = physical_plan->Get(i);
    EXPECT_THAT(agent_instance->carnot_info().query_broker_address(), ContainsRegex("agent"));

    EXPECT_THAT(agent_instance->plan()->ToProto().ConsumeValueOrDie(),
                Partially(EqualsProto(kAgentSimplePlan)));
  }
}

TEST_F(CoordinatorTest, one_agent_three_kelvin) {
  auto ps = LoadDistributedStatePb(kOneAgentThreeKelvinsDistributedState);
  auto coordinator = Coordinator::Create(ps).ConsumeValueOrDie();

  MakeGraph();

  auto physical_plan = coordinator->Coordinate(graph.get()).ConsumeValueOrDie();
  ASSERT_EQ(physical_plan->dag().nodes().size(), 2UL);
  EXPECT_THAT(physical_plan->dag().TopologicalSort(), ElementsAre(1, 0));

  auto kelvin_instance = physical_plan->Get(0);
  EXPECT_THAT(kelvin_instance->carnot_info().query_broker_address(), ContainsRegex("kelvin"));
  EXPECT_THAT(kelvin_instance->plan()->ToProto().ConsumeValueOrDie(),
              Partially(EqualsProto(kKelvinSimplePlan)));

  // Agents should be instance 1
  auto agent_instance = physical_plan->Get(1);
  EXPECT_THAT(agent_instance->carnot_info().query_broker_address(), ContainsRegex("agent"));

  EXPECT_THAT(agent_instance->plan()->ToProto().ConsumeValueOrDie(),
              Partially(EqualsProto(kAgentSimplePlan)));
}

const char* kBadAgentSpecificationState = R"proto(
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

const char* kBadKelvinSpecificationState = R"proto(
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

const char* kUDTFServiceUpTimePb = R"proto(
name: "ServiceUpTime"
executor: UDTF_ONE_KELVIN
relation {
  columns {
    column_name: "service"
    column_type: STRING
  }
  columns {
    column_name: "up_time"
    column_type: INT64
  }
}
)proto";

TEST_F(CoordinatorTest, udtf_on_one_kelvin) {
  udfspb::UDTFSourceSpec udtf_spec;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kUDTFServiceUpTimePb, &udtf_spec));
  Relation udtf_relation;
  ASSERT_OK(udtf_relation.FromProto(&udtf_spec.relation()));

  auto udtf = MakeUDTFSource(udtf_spec, {}, {});
  auto mem_sink = MakeMemSink(udtf, "out");

  auto ps = LoadDistributedStatePb(kThreeAgentsOneKelvinDistributedState);
  auto coordinator = Coordinator::Create(ps).ConsumeValueOrDie();

  auto physical_plan = coordinator->Coordinate(graph.get()).ConsumeValueOrDie();
  ASSERT_EQ(physical_plan->dag().nodes().size(), 1UL);
  IR* plan = physical_plan->Get(0)->plan();

  auto new_udtf = GetEquivalentInNewPlan(plan, udtf);
  EXPECT_EQ(new_udtf->Children().size(), 1UL);
  EXPECT_EQ(new_udtf->Children()[0], GetEquivalentInNewPlan(plan, mem_sink));
}

const char* kUDTFOpenConnsPb = R"proto(
name: "OpenNetworkConnections"
args {
  name: "upid"
  arg_type: STRING
  semantic_type: ST_UPID
}
executor: UDTF_SUBSET_PEM
relation {
  columns {
    column_name: "time_"
    column_type: TIME64NS
  }
  columns {
    column_name: "fd"
    column_type: INT64
  }
  columns {
    column_name: "name"
    column_type: STRING
  }
}
)proto";

TEST_F(CoordinatorTest, udtf_run_on_som_pems) {
  udfspb::UDTFSourceSpec udtf_spec;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kUDTFOpenConnsPb, &udtf_spec));
  Relation udtf_relation;
  ASSERT_OK(udtf_relation.FromProto(&udtf_spec.relation()));
  uint32_t asid = 123;
  md::UPID upid(asid, 456, 3420030816657ULL);
  std::string upid_str =
      sole::rebuild(absl::Uint128High64(upid.value()), absl::Uint128Low64(upid.value())).str();

  auto udtf = MakeUDTFSource(udtf_spec, {"upid"}, {MakeString(upid_str)});
  auto mem_sink = MakeMemSink(udtf, "out");

  auto ps = LoadDistributedStatePb(kThreeAgentsOneKelvinDistributedState);
  auto coordinator = Coordinator::Create(ps).ConsumeValueOrDie();

  auto physical_plan = coordinator->Coordinate(graph.get()).ConsumeValueOrDie();
  ASSERT_EQ(physical_plan->dag().nodes().size(), 2UL);
  auto topo_sort = physical_plan->dag().TopologicalSort();
  auto agent_instance = physical_plan->Get(topo_sort[0]);
  EXPECT_EQ(agent_instance->carnot_info().query_broker_address(), "agent1");
  // Verify that we actually got the right agent_instance
  EXPECT_EQ(agent_instance->carnot_info().asid(), asid);
  auto kelvin_instance = physical_plan->Get(topo_sort[1]);
  EXPECT_EQ(kelvin_instance->carnot_info().query_broker_address(), "kelvin");
  auto agent_plan = agent_instance->plan();
  auto kelvin_plan = kelvin_instance->plan();
  auto new_udtf = GetEquivalentInNewPlan(agent_plan, udtf);
  ASSERT_EQ(new_udtf->Children().size(), 1UL);
  EXPECT_EQ(new_udtf->Children()[0]->type(), IRNodeType::kGRPCSink);
  auto new_mem_sink = GetEquivalentInNewPlan(kelvin_plan, mem_sink);
  ASSERT_EQ(new_mem_sink->parents().size(), 1UL);
  EXPECT_EQ(new_mem_sink->parents()[0]->type(), IRNodeType::kGRPCSourceGroup);
}

}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
