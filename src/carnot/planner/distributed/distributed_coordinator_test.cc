#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/distributed_coordinator.h"
#include "src/carnot/planner/distributed/distributed_planner.h"
#include "src/carnot/planner/ir/ir_nodes.h"
#include "src/carnot/planner/rules/rules.h"
#include "src/carnot/planner/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/testing/protobuf.h"
#include "src/shared/metadata/metadata_filter.h"

namespace pl {
namespace carnot {
namespace planner {
namespace distributed {
using md::AgentMetadataFilter;
using ::pl::testing::proto::EqualsProto;
using ::pl::testing::proto::Partially;
using ::testing::ContainsRegex;
using ::testing::ElementsAre;
using testutils::kOnePEMOneKelvinDistributedState;
using testutils::kOnePEMThreeKelvinsDistributedState;
using testutils::kThreePEMsOneKelvinDistributedState;

class CoordinatorTest : public testutils::DistributedRulesTest {
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
    EXPECT_MATCH(mem_src->Children()[0], GRPCSink());
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

  distributedpb::DistributedState ThreeAgentOneKelvinStateWithMetadataInfo() {
    auto ps = LoadDistributedStatePb(kThreePEMsOneKelvinDistributedState);
    auto agent1_filter =
        AgentMetadataFilter::Create(100, 0.01, {MetadataType::POD_ID, MetadataType::SERVICE_ID})
            .ConsumeValueOrDie();
    auto agent2_filter =
        AgentMetadataFilter::Create(100, 0.01, {MetadataType::POD_ID, MetadataType::SERVICE_ID})
            .ConsumeValueOrDie();
    auto agent3_filter =
        AgentMetadataFilter::Create(100, 0.01, {MetadataType::POD_ID, MetadataType::SERVICE_ID})
            .ConsumeValueOrDie();

    PL_CHECK_OK(agent1_filter->InsertEntity(MetadataType::POD_ID, "agent1_pod"));
    PL_CHECK_OK(agent2_filter->InsertEntity(MetadataType::SERVICE_ID, "agent2_service"));

    absl::flat_hash_map<std::string, distributedpb::MetadataInfo> mds;
    mds["pem1"] = agent1_filter->ToProto();
    mds["pem2"] = agent2_filter->ToProto();
    mds["pem3"] = agent3_filter->ToProto();

    for (auto i = 0; i < ps.carnot_info_size(); ++i) {
      if (ps.carnot_info(i).query_broker_address() == "kelvin") {
        continue;
      }
      *(ps.mutable_carnot_info(i)->mutable_metadata_info()) =
          mds.at(ps.carnot_info(i).query_broker_address());
    }
    return ps;
  }

  std::unique_ptr<DistributedPlan> ThreeAgentOneKelvinCoordinateQuery(std::string_view query) {
    auto ps = ThreeAgentOneKelvinStateWithMetadataInfo();

    auto coordinator = Coordinator::Create(ps).ConsumeValueOrDie();
    compiler::Compiler compiler;
    auto graph =
        compiler.CompileToIR(std::string(query), compiler_state_.get()).ConsumeValueOrDie();

    return coordinator->Coordinate(graph.get()).ConsumeValueOrDie();
  }
};

TEST_F(CoordinatorTest, one_pem_one_kelvin) {
  auto ps = LoadDistributedStatePb(kOnePEMOneKelvinDistributedState);
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
  EXPECT_THAT(pem_instance->carnot_info().query_broker_address(), ContainsRegex("pem"));
  {
    SCOPED_TRACE("one agent one kelvin -> pem plan");
    VerifyPEMPlan(pem_instance->plan());
  }
}

TEST_F(CoordinatorTest, three_pems_one_kelvin) {
  auto ps = LoadDistributedStatePb(kThreePEMsOneKelvinDistributedState);
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
    EXPECT_THAT(pem_instance->carnot_info().query_broker_address(), ContainsRegex("pem"));
    VerifyPEMPlan(pem_instance->plan());
  }
}

TEST_F(CoordinatorTest, one_pem_three_kelvin) {
  auto ps = LoadDistributedStatePb(kOnePEMThreeKelvinsDistributedState);
  auto coordinator = Coordinator::Create(ps).ConsumeValueOrDie();

  MakeGraph();

  auto physical_plan = coordinator->Coordinate(graph.get()).ConsumeValueOrDie();
  ASSERT_EQ(physical_plan->dag().nodes().size(), 2UL);
  EXPECT_THAT(physical_plan->dag().TopologicalSort(), ElementsAre(1, 0));

  auto kelvin_instance = physical_plan->Get(0);
  EXPECT_THAT(kelvin_instance->carnot_info().query_broker_address(), ContainsRegex("kelvin"));
  {
    SCOPED_TRACE("one pem one kelvin -> kelvin plan");
    VerifyKelvinMergerPlan(kelvin_instance->plan());
  }

  auto pem_instance = physical_plan->Get(1);
  EXPECT_THAT(pem_instance->carnot_info().query_broker_address(), ContainsRegex("pem"));
  {
    SCOPED_TRACE("one pem one kelvin -> pem plan");
    VerifyPEMPlan(pem_instance->plan());
  }
}

constexpr char kBadAgentSpecificationState[] = R"proto(
carnot_info {
  query_broker_address: "pem"
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
  ssl_targetname: "kelvin.pl.svc"
}
)proto";

TEST_F(CoordinatorTest, bad_pem_spec) {
  auto ps = LoadDistributedStatePb(kBadAgentSpecificationState);
  auto coordinator_status = Coordinator::Create(ps);

  ASSERT_NOT_OK(coordinator_status);
  EXPECT_EQ(coordinator_status.status().msg(),
            "Distributed state does not have a Carnot instance that satisifies the condition "
            "`has_data_store() && processes_data()`.");
}

constexpr char kBadKelvinSpecificationState[] = R"proto(
carnot_info {
  query_broker_address: "pem"
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
  ssl_targetname: "kelvin.pl.svc"
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

constexpr char kPruneAgentsSimple[] = R"pxl(
import px

# simple case 1
t1 = px.DataFrame(table='http_events', start_time='-120s', select=['upid'])
t1 = t1[t1.ctx['pod_id'] == 'agent1_pod']
px.display(t1, 't1')

# simple case 2
t2 = px.DataFrame(table='http_events', start_time='-120s', select=['upid'])
t2 = t2['agent2_service' == t2.ctx['service_id']]
px.display(t2, 't2')
)pxl";

TEST_F(CoordinatorTest, prune_agents_simple) {
  auto physical_plan = ThreeAgentOneKelvinCoordinateQuery(kPruneAgentsSimple);
  EXPECT_EQ(physical_plan->dag().nodes().size(), 4UL);

  absl::flat_hash_map<std::string, IR*> plan_by_qb_addr;
  for (int64_t carnot_id : physical_plan->dag().nodes()) {
    auto carnot = physical_plan->Get(carnot_id);
    plan_by_qb_addr[carnot->QueryBrokerAddress()] = carnot->plan();
  }

  auto agent1_sinks = plan_by_qb_addr["pem1"]->FindNodesThatMatch(GRPCSink());
  EXPECT_EQ(1, agent1_sinks.size());
  auto agent1_sink_parents = static_cast<OperatorIR*>(agent1_sinks[0])->parents();
  EXPECT_EQ(1, agent1_sink_parents.size());
  EXPECT_MATCH(agent1_sink_parents[0],
               Filter(Equals(MetadataExpression(MetadataType::POD_ID), String("agent1_pod"))));

  auto agent2_sinks = plan_by_qb_addr["pem2"]->FindNodesThatMatch(GRPCSink());
  EXPECT_EQ(1, agent2_sinks.size());
  auto agent2_sink_parents = static_cast<OperatorIR*>(agent2_sinks[0])->parents();
  EXPECT_EQ(1, agent2_sink_parents.size());
  EXPECT_MATCH(agent2_sink_parents[0], Filter(Equals(MetadataExpression(MetadataType::SERVICE_ID),
                                                     String("agent2_service"))));

  auto agent3_sinks = plan_by_qb_addr["pem3"]->FindNodesThatMatch(GRPCSink());
  EXPECT_EQ(0, agent3_sinks.size());
  auto kelvin_sources = plan_by_qb_addr["kelvin"]->FindNodesThatMatch(GRPCSourceGroup());
  EXPECT_EQ(2, kelvin_sources.size());
}

constexpr char kPruneAgentsDoesNotExist[] = R"pxl(
import px

# does not exist
t1 = px.DataFrame(table='http_events', start_time='-120s', select=['upid'])
t1 = t1[t1.ctx['pod_id'] == 'does_not_exist']
px.display(t1, 't1')
)pxl";

TEST_F(CoordinatorTest, prune_agents_nonexistent) {
  auto physical_plan = ThreeAgentOneKelvinCoordinateQuery(kPruneAgentsDoesNotExist);

  EXPECT_EQ(physical_plan->dag().nodes().size(), 4UL);

  absl::flat_hash_map<std::string, IR*> plan_by_qb_addr;
  for (int64_t carnot_id : physical_plan->dag().nodes()) {
    auto carnot = physical_plan->Get(carnot_id);
    plan_by_qb_addr[carnot->QueryBrokerAddress()] = carnot->plan();
  }

  EXPECT_EQ(0, plan_by_qb_addr["pem1"]->FindNodesThatMatch(GRPCSink()).size());
  EXPECT_EQ(0, plan_by_qb_addr["pem2"]->FindNodesThatMatch(GRPCSink()).size());

  auto agent3_sinks = plan_by_qb_addr["pem3"]->FindNodesThatMatch(GRPCSink());
  EXPECT_EQ(1, agent3_sinks.size());
  auto agent3_sink_parents = static_cast<OperatorIR*>(agent3_sinks[0])->parents();
  EXPECT_EQ(1, agent3_sink_parents.size());
  EXPECT_MATCH(agent3_sink_parents[0],
               Filter(Equals(MetadataExpression(MetadataType::POD_ID), String("does_not_exist"))));

  EXPECT_EQ(1, plan_by_qb_addr["kelvin"]->FindNodesThatMatch(GRPCSourceGroup()).size());
}

constexpr char kPruneAgentsUnsupportedMDType[] = R"pxl(
import px

# container_id is not a supported metadata filter type in the test environment.
t1 = px.DataFrame(table='http_events', start_time='-120s', select=['upid'])
t1 = t1[t1.ctx['container_id'] == 'foobar']
px.display(t1, 't1')
)pxl";

TEST_F(CoordinatorTest, prune_agents_unsupported_metadata_type) {
  auto physical_plan = ThreeAgentOneKelvinCoordinateQuery(kPruneAgentsUnsupportedMDType);

  EXPECT_EQ(physical_plan->dag().nodes().size(), 4UL);

  absl::flat_hash_map<std::string, IR*> plan_by_qb_addr;
  for (int64_t carnot_id : physical_plan->dag().nodes()) {
    auto carnot = physical_plan->Get(carnot_id);
    plan_by_qb_addr[carnot->QueryBrokerAddress()] = carnot->plan();
  }

  for (const auto& str : {"pem1", "pem2", "pem3"}) {
    auto agent_sinks = plan_by_qb_addr[str]->FindNodesThatMatch(GRPCSink());
    EXPECT_EQ(1, agent_sinks.size());
    auto agent_sink_parents = static_cast<OperatorIR*>(agent_sinks[0])->parents();
    EXPECT_EQ(1, agent_sink_parents.size());
    EXPECT_MATCH(agent_sink_parents[0],
                 Filter(Equals(MetadataExpression(MetadataType::CONTAINER_ID), String("foobar"))));
  }
  EXPECT_EQ(1, plan_by_qb_addr["kelvin"]->FindNodesThatMatch(GRPCSourceGroup()).size());
}

constexpr char kPruneAgentsRename[] = R"pxl(
import px

# not a metadata filter (yet). TODO(nserrino): Support aliases.
t1 = px.DataFrame(table='http_events', start_time='-120s', select=['upid', 'remote_port'])
t1.pod_id = t1.ctx['pod_id']
t1 = t1[t1.pod_id == 'agent1_pod']
px.display(t1, 't1')
)pxl";

TEST_F(CoordinatorTest, prune_agents_rename_metadata_column) {
  auto physical_plan = ThreeAgentOneKelvinCoordinateQuery(kPruneAgentsRename);

  EXPECT_EQ(physical_plan->dag().nodes().size(), 4UL);

  absl::flat_hash_map<std::string, IR*> plan_by_qb_addr;
  for (int64_t carnot_id : physical_plan->dag().nodes()) {
    auto carnot = physical_plan->Get(carnot_id);
    plan_by_qb_addr[carnot->QueryBrokerAddress()] = carnot->plan();
  }

  EXPECT_EQ(0, plan_by_qb_addr["pem2"]->FindNodesThatMatch(GRPCSink()).size());
  EXPECT_EQ(0, plan_by_qb_addr["pem3"]->FindNodesThatMatch(GRPCSink()).size());

  auto agent3_sinks = plan_by_qb_addr["pem1"]->FindNodesThatMatch(GRPCSink());
  EXPECT_EQ(1, agent3_sinks.size());
  auto agent3_sink_parents = static_cast<OperatorIR*>(agent3_sinks[0])->parents();
  EXPECT_EQ(1, agent3_sink_parents.size());
  EXPECT_MATCH(agent3_sink_parents[0],
               Filter(Equals(MetadataExpression(MetadataType::POD_ID), String("agent1_pod"))));

  EXPECT_EQ(1, plan_by_qb_addr["kelvin"]->FindNodesThatMatch(GRPCSourceGroup()).size());
}

constexpr char kPruneAgentsMultiParent[] = R"pxl(
import px

left = px.DataFrame(table='http_events', start_time='-120s', select=['upid'])
left = left[left.ctx['pod_id'] == 'agent1_pod']
right = px.DataFrame(table='process_stats', start_time='-120s', select=['upid'])
joined_table = left.merge(right, how='inner', left_on=['upid'], right_on=['upid'],
                          suffixes=['', '_x'])
px.display(joined_table, 'multi_parent')
)pxl";
TEST_F(CoordinatorTest, prune_agents_multiparent) {
  auto physical_plan = ThreeAgentOneKelvinCoordinateQuery(kPruneAgentsMultiParent);

  EXPECT_EQ(physical_plan->dag().nodes().size(), 4UL);

  absl::flat_hash_map<std::string, IR*> plan_by_qb_addr;
  for (int64_t carnot_id : physical_plan->dag().nodes()) {
    auto carnot = physical_plan->Get(carnot_id);
    plan_by_qb_addr[carnot->QueryBrokerAddress()] = carnot->plan();
  }

  auto pem1_sinks = plan_by_qb_addr["pem1"]->FindNodesThatMatch(GRPCSink());
  ASSERT_EQ(2, pem1_sinks.size());

  auto pem1_src_sink = static_cast<OperatorIR*>(pem1_sinks[0]);
  auto pem1_filter_sink = static_cast<OperatorIR*>(pem1_sinks[1]);
  ASSERT_EQ(1, pem1_filter_sink->parents().size());
  ASSERT_EQ(1, pem1_src_sink->parents().size());

  // FindNodesThatMatch doesn't guarantee ordering, so this reorders the GRPC sinks if they don't
  // match.
  if (!Match(pem1_src_sink->parents()[0], MemorySource())) {
    auto tmp = pem1_src_sink;
    pem1_src_sink = pem1_filter_sink;
    pem1_filter_sink = tmp;
  }

  EXPECT_MATCH(pem1_src_sink->parents()[0], MemorySource());
  EXPECT_MATCH(pem1_filter_sink->parents()[0],
               Filter(Equals(MetadataExpression(MetadataType::POD_ID), String("agent1_pod"))));

  auto pem2_sinks = plan_by_qb_addr["pem2"]->FindNodesThatMatch(GRPCSink());
  EXPECT_EQ(1, pem2_sinks.size());
  auto pem2_sink_parents = static_cast<OperatorIR*>(pem2_sinks[0])->parents();
  EXPECT_EQ(1, pem2_sink_parents.size());
  EXPECT_MATCH(pem2_sink_parents[0], MemorySource());

  auto pem3_sinks = plan_by_qb_addr["pem3"]->FindNodesThatMatch(GRPCSink());
  EXPECT_EQ(1, pem3_sinks.size());
  auto pem3_sink_parents = static_cast<OperatorIR*>(pem3_sinks[0])->parents();
  EXPECT_EQ(1, pem3_sink_parents.size());
  EXPECT_MATCH(pem3_sink_parents[0], MemorySource());

  EXPECT_EQ(2, plan_by_qb_addr["kelvin"]->FindNodesThatMatch(GRPCSourceGroup()).size());
}

constexpr char kPruneAgentsMultiChild[] = R"pxl(
import px

# multi-child
multichild = px.DataFrame(table='http_events', start_time='-120s', select=['upid'])
px.display(multichild, 'sink1')
multichild = multichild[multichild.ctx['service_id'] == 'agent2_service']
px.display(multichild, 'sink2')

)pxl";

TEST_F(CoordinatorTest, DISABLED_prune_agents_multichild) {
  auto physical_plan = ThreeAgentOneKelvinCoordinateQuery(kPruneAgentsMultiChild);
  EXPECT_EQ(physical_plan->dag().nodes().size(), 4UL);

  absl::flat_hash_map<std::string, IR*> plan_by_qb_addr;
  for (int64_t carnot_id : physical_plan->dag().nodes()) {
    auto carnot = physical_plan->Get(carnot_id);
    plan_by_qb_addr[carnot->QueryBrokerAddress()] = carnot->plan();
  }

  auto agent1_sinks = plan_by_qb_addr["pem1"]->FindNodesThatMatch(GRPCSink());
  EXPECT_EQ(1, agent1_sinks.size());
  auto agent1_sink_parents = static_cast<OperatorIR*>(agent1_sinks[0])->parents();
  EXPECT_EQ(1, agent1_sink_parents.size());
  EXPECT_MATCH(agent1_sink_parents[0], MemorySource());

  auto pem2_sinks = plan_by_qb_addr["pem2"]->FindNodesThatMatch(GRPCSink());
  ASSERT_EQ(2, pem2_sinks.size());

  auto pem2_src_sink = static_cast<OperatorIR*>(pem2_sinks[0]);
  auto pem2_filter_sink = static_cast<OperatorIR*>(pem2_sinks[1]);
  ASSERT_EQ(1, pem2_filter_sink->parents().size());
  ASSERT_EQ(1, pem2_src_sink->parents().size());

  // FindNodesThatMatch doesn't guarantee ordering, so this should realign to our expectations.
  if (!Match(pem2_src_sink->parents()[0], MemorySource())) {
    auto tmp = pem2_src_sink;
    pem2_src_sink = pem2_filter_sink;
    pem2_filter_sink = tmp;
  }

  EXPECT_MATCH(pem2_src_sink->parents()[0], MemorySource());
  EXPECT_MATCH(
      pem2_filter_sink->parents()[0],
      Filter(Equals(MetadataExpression(MetadataType::SERVICE_ID), String("agent2_service"))));

  auto agent3_sinks = plan_by_qb_addr["pem3"]->FindNodesThatMatch(GRPCSink());
  EXPECT_EQ(1, agent3_sinks.size());
  auto agent3_sink_parents = static_cast<OperatorIR*>(agent3_sinks[0])->parents();
  EXPECT_EQ(1, agent3_sink_parents.size());
  EXPECT_MATCH(agent3_sink_parents[0], MemorySource());
}

constexpr char kPruneAgentConjunction[] = R"pxl(
import px

# and (only agent 1)
t1 = px.DataFrame(table='http_events', start_time='-120s', select=['upid'])
t1 = t1[t1.ctx['pod_id'] == 'agent1_pod' or t1.ctx['pod_id'] == 'does_not_exist']
px.display(t1, 't1')

# mixed (only agent 2)
t2 = px.DataFrame(table='http_events', start_time='-120s', select=['upid'])
t2 = t2['agent2_service' == t2.ctx['service_id'] and ('agent2_service' == t2.ctx['service_id'] or 3 == 3)]
px.display(t2, 't2')

# mixed (passes none)
t2 = px.DataFrame(table='http_events', start_time='-120s', select=['upid'])
t2 = t2['agent2_service' == t2.ctx['service_id'] and t1.ctx['pod_id'] == 'agent1_pod']
px.display(t2, 't2')

)pxl";

TEST_F(CoordinatorTest, prune_agents_logical_conjunctions) {
  auto physical_plan = ThreeAgentOneKelvinCoordinateQuery(kPruneAgentConjunction);
  EXPECT_EQ(physical_plan->dag().nodes().size(), 4UL);

  absl::flat_hash_map<std::string, IR*> plan_by_qb_addr;
  for (int64_t carnot_id : physical_plan->dag().nodes()) {
    auto carnot = physical_plan->Get(carnot_id);
    plan_by_qb_addr[carnot->QueryBrokerAddress()] = carnot->plan();
  }

  auto agent1_sinks = plan_by_qb_addr["pem1"]->FindNodesThatMatch(GRPCSink());
  EXPECT_EQ(1, agent1_sinks.size());
  auto agent1_sink_parents = static_cast<OperatorIR*>(agent1_sinks[0])->parents();
  EXPECT_EQ(1, agent1_sink_parents.size());
  EXPECT_MATCH(agent1_sink_parents[0],
               Filter(LogicalOr(
                   Equals(MetadataExpression(MetadataType::POD_ID), String("agent1_pod")),
                   Equals(MetadataExpression(MetadataType::POD_ID), String("does_not_exist")))));

  auto agent2_sinks = plan_by_qb_addr["pem2"]->FindNodesThatMatch(GRPCSink());
  EXPECT_EQ(1, agent2_sinks.size());
  auto agent2_sink_parents = static_cast<OperatorIR*>(agent2_sinks[0])->parents();
  EXPECT_EQ(1, agent2_sink_parents.size());
  EXPECT_MATCH(agent2_sink_parents[0],
               Filter(LogicalAnd(
                   Equals(MetadataExpression(MetadataType::SERVICE_ID), String("agent2_service")),
                   LogicalOr(Value(), Value()))));

  auto agent3_sinks = plan_by_qb_addr["pem3"]->FindNodesThatMatch(GRPCSink());
  EXPECT_EQ(1, agent3_sinks.size());
  auto agent3_sink_parents = static_cast<OperatorIR*>(agent3_sinks[0])->parents();
  EXPECT_EQ(1, agent3_sink_parents.size());
  EXPECT_MATCH(agent3_sink_parents[0],
               Filter(LogicalAnd(Equals(Value(), Value()), Equals(Value(), Value()))));

  auto kelvin_sources = plan_by_qb_addr["kelvin"]->FindNodesThatMatch(GRPCSourceGroup());
  EXPECT_EQ(3, kelvin_sources.size());
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
