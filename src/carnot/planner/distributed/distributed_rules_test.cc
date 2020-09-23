#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <unordered_map>
#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/analyzer.h"
#include "src/carnot/planner/compiler/compiler.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/distributed_coordinator.h"
#include "src/carnot/planner/distributed/distributed_rules.h"
#include "src/carnot/planner/parser/parser.h"
#include "src/carnot/planner/rules/rule_mock.h"
#include "src/carnot/planner/test_utils.h"

namespace pl {
namespace carnot {
namespace planner {
namespace distributed {

using table_store::schema::Relation;
using table_store::schemapb::Schema;
using ::testing::_;
using ::testing::Return;
using testutils::CreateTwoPEMsOneKelvinPlannerState;
using testutils::DistributedRulesTest;
using testutils::kHttpEventsSchema;

using PruneUnavailableSourcesRuleTest = DistributedRulesTest;
TEST_F(DistributedRulesTest, DistributedIRRuleTest) {
  auto physical_plan = std::make_unique<distributed::DistributedPlan>();
  distributedpb::DistributedState physical_state =
      LoadDistributedStatePb(kOneAgentDistributedState);

  for (int64_t i = 0; i < physical_state.carnot_info_size(); ++i) {
    int64_t carnot_id =
        physical_plan->AddCarnot(physical_state.carnot_info()[i]).ConsumeValueOrDie();

    auto plan_uptr = std::make_unique<IR>();
    physical_plan->Get(carnot_id)->AddPlan(plan_uptr.get());
    physical_plan->AddPlan(std::move(plan_uptr));
  }

  DistributedIRRule<MockRule> rule;
  MockRule* subrule = rule.subrule();
  EXPECT_CALL(*subrule, Execute(_)).Times(4).WillOnce(Return(true)).WillRepeatedly(Return(false));

  auto result = rule.Execute(physical_plan.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  result = rule.Execute(physical_plan.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ConsumeValueOrDie());
}

TEST_F(PruneUnavailableSourcesRuleTest, UDTFOnKelvinFiltersOutPEMPlan) {
  udfspb::UDTFSourceSpec udtf_spec;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kUDTFServiceUpTimePb, &udtf_spec));
  // Should only run on one kelvin.
  EXPECT_EQ(udtf_spec.executor(), udfspb::UDTF_ONE_KELVIN);

  // Sub-plan 1 should be deleted.
  auto udtf_src = MakeUDTFSource(udtf_spec, {});
  auto grpc_sink1 = MakeGRPCSink(udtf_src, 123);
  auto udtf_src_id = udtf_src->id();
  auto grpc_sink1_id = grpc_sink1->id();

  // Sub-plan 2 should not be affected.
  auto mem_src = MakeMemSource("http_events");
  auto grpc_sink2 = MakeGRPCSink(mem_src, 456);
  auto mem_src_id = mem_src->id();
  auto grpc_sink2_id = grpc_sink2->id();

  // We want to grab a PEM.
  auto carnot_info = logical_state_.distributed_state().carnot_info()[0];
  ASSERT_TRUE(IsPEM(carnot_info));

  ASSERT_OK_AND_ASSIGN(sole::uuid uuid, ParseUUID(carnot_info.agent_id()));
  ASSERT_OK_AND_ASSIGN(auto schema_map,
                       LoadSchemaMap(logical_state_.distributed_state(), uuid_to_id_map_));
  PruneUnavailableSourcesRule rule(uuid_to_id_map_[uuid], carnot_info, schema_map);

  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_TRUE(rule_or_s.ConsumeValueOrDie());

  // Sub-plan 1 is deleted.
  EXPECT_FALSE(graph->HasNode(udtf_src_id));
  EXPECT_FALSE(graph->HasNode(grpc_sink1_id));

  // Sub-plan 2 is not deleted.
  EXPECT_TRUE(graph->HasNode(mem_src_id));
  EXPECT_TRUE(graph->HasNode(grpc_sink2_id));
}

// TODO(philkuz) (PL-1468) Handle Join removal in a good way and test with other types of joins.
TEST_F(PruneUnavailableSourcesRuleTest, DISABLED_UDTFOnKelvinShouldBeRemovedIfOtherJoinRemoved) {
  udfspb::UDTFSourceSpec udtf_spec;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kUDTFServiceUpTimePb, &udtf_spec));
  // Should only run on one kelvin.
  EXPECT_EQ(udtf_spec.executor(), udfspb::UDTF_ONE_KELVIN);

  auto udtf_src = MakeUDTFSource(udtf_spec, {});
  Relation udtf_relation;
  ASSERT_OK(udtf_relation.FromProto(&udtf_spec.relation()));
  EXPECT_OK(udtf_src->SetRelation(udtf_relation));

  Relation src_relation{{types::STRING, types::INT64}, {"service", "rx_bytes"}};
  // This mem source can't be run on the Kelvin, so we should delete it.
  auto mem_src = MakeMemSource(src_relation);

  // Note this happens after the splitting stage, so if we have a regular Join here we shouldn't be
  // streaming the data over.
  auto join =
      MakeJoin({mem_src, udtf_src}, "inner", src_relation, udtf_relation, {"service"}, {"service"});

  auto mem_sink = MakeMemSink(join, "output");

  // IDs.
  auto udtf_src_id = udtf_src->id();
  auto mem_sink_id = mem_sink->id();
  auto join_id = join->id();
  auto mem_src_id = mem_src->id();

  auto carnot_info = logical_state_.distributed_state().carnot_info()[2];
  // Should be a kelvin.
  ASSERT_TRUE(!IsPEM(carnot_info));

  ASSERT_OK_AND_ASSIGN(sole::uuid uuid, ParseUUID(carnot_info.agent_id()));
  ASSERT_OK_AND_ASSIGN(auto schema_map,
                       LoadSchemaMap(logical_state_.distributed_state(), uuid_to_id_map_));
  PruneUnavailableSourcesRule rule(uuid_to_id_map_[uuid], carnot_info, schema_map);
  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_TRUE(rule_or_s.ConsumeValueOrDie());

  // All the ndoes are deleted.
  EXPECT_FALSE(graph->HasNode(udtf_src_id));
  EXPECT_FALSE(graph->HasNode(mem_sink_id));
  EXPECT_FALSE(graph->HasNode(join_id));
  EXPECT_FALSE(graph->HasNode(mem_src_id));
}

TEST_F(PruneUnavailableSourcesRuleTest, UDTFOnKelvinKeepsAllKelvinNodes) {
  udfspb::UDTFSourceSpec udtf_spec;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kUDTFServiceUpTimePb, &udtf_spec));
  EXPECT_EQ(udtf_spec.executor(), udfspb::UDTF_ONE_KELVIN);

  auto udtf_src = MakeUDTFSource(udtf_spec, {});
  auto grpc_sink = MakeGRPCSink(udtf_src, 123);
  auto udtf_src_id = udtf_src->id();
  auto grpc_sink_id = grpc_sink->id();

  auto carnot_info = logical_state_.distributed_state().carnot_info()[2];
  // Should be a kelvin.
  ASSERT_TRUE(!IsPEM(carnot_info));
  ASSERT_OK_AND_ASSIGN(sole::uuid uuid, ParseUUID(carnot_info.agent_id()));
  ASSERT_OK_AND_ASSIGN(auto schema_map,
                       LoadSchemaMap(logical_state_.distributed_state(), uuid_to_id_map_));
  PruneUnavailableSourcesRule rule(uuid_to_id_map_[uuid], carnot_info, schema_map);
  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_FALSE(rule_or_s.ConsumeValueOrDie());

  EXPECT_TRUE(graph->HasNode(udtf_src_id));
  EXPECT_TRUE(graph->HasNode(grpc_sink_id));
}

TEST_F(PruneUnavailableSourcesRuleTest, UDTFOnPEMsRemovesKelvin) {
  udfspb::UDTFSourceSpec udtf_spec;
  ASSERT_TRUE(
      google::protobuf::TextFormat::MergeFromString(kUDTFOpenNetworkConnections, &udtf_spec));
  EXPECT_EQ(udtf_spec.executor(), udfspb::UDTF_SUBSET_PEM);

  // Sub-plan 1, should be deleted.
  auto udtf_src =
      MakeUDTFSource(udtf_spec, {{"upid", MakeUInt128("11285cdd-1de9-4ab1-ae6a-0ba08c8c676c")}});
  auto grpc_sink1 = MakeGRPCSink(udtf_src, 123);
  auto udtf_src_id = udtf_src->id();
  auto grpc_sink1_id = grpc_sink1->id();

  // Sub-plan 2, should not be affected.
  auto grpc_source1 = MakeGRPCSource(udtf_src->relation());
  auto grpc_source2 = MakeGRPCSource(udtf_src->relation());
  auto union_node = MakeUnion({grpc_source1, grpc_source2});
  auto grpc_source_id1 = grpc_source1->id();
  auto grpc_source_id2 = grpc_source2->id();
  auto union_node_id = union_node->id();

  auto kelvin_info = logical_state_.distributed_state().carnot_info()[2];
  ASSERT_FALSE(IsPEM(kelvin_info));
  ASSERT_OK_AND_ASSIGN(sole::uuid uuid, ParseUUID(kelvin_info.agent_id()));
  ASSERT_OK_AND_ASSIGN(auto schema_map,
                       LoadSchemaMap(logical_state_.distributed_state(), uuid_to_id_map_));
  PruneUnavailableSourcesRule rule(uuid_to_id_map_[uuid], kelvin_info, schema_map);
  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_TRUE(rule_or_s.ConsumeValueOrDie());

  // Sub-plan 1 is deleted.
  EXPECT_FALSE(graph->HasNode(udtf_src_id));
  EXPECT_FALSE(graph->HasNode(grpc_sink1_id));

  // Sub-plan 2 is not deleted.
  EXPECT_TRUE(graph->HasNode(grpc_source_id1));
  EXPECT_TRUE(graph->HasNode(grpc_source_id2));
  EXPECT_TRUE(graph->HasNode(union_node_id));
}

TEST_F(PruneUnavailableSourcesRuleTest, UDTFOnPEMsKeepsPEM) {
  udfspb::UDTFSourceSpec udtf_spec;
  ASSERT_TRUE(
      google::protobuf::TextFormat::MergeFromString(kUDTFOpenNetworkConnections, &udtf_spec));
  // Should only run on one kelvin.
  EXPECT_EQ(udtf_spec.executor(), udfspb::UDTF_SUBSET_PEM);

  auto upid = md::UPID(123, 456, 789);
  // Sub-plan 1 should be deleted.
  auto udtf_src = MakeUDTFSource(
      udtf_spec, {{"upid", graph->CreateNode<UInt128IR>(ast, upid.value()).ConsumeValueOrDie()}});
  auto grpc_sink1 = MakeGRPCSink(udtf_src, 123);
  auto udtf_src_id = udtf_src->id();
  auto grpc_sink1_id = grpc_sink1->id();

  // Sub-plan 2 should not be affected.
  auto mem_src = MakeMemSource("http_events");
  auto grpc_sink2 = MakeGRPCSink(mem_src, 456);
  auto mem_src_id = mem_src->id();
  auto grpc_sink2_id = grpc_sink2->id();

  // We want to grab a PEM.
  auto carnot_info = logical_state_.distributed_state().carnot_info()[0];
  carnot_info.set_asid(upid.asid());
  ASSERT_TRUE(IsPEM(carnot_info));

  ASSERT_OK_AND_ASSIGN(sole::uuid uuid, ParseUUID(carnot_info.agent_id()));
  ASSERT_OK_AND_ASSIGN(auto schema_map,
                       LoadSchemaMap(logical_state_.distributed_state(), uuid_to_id_map_));
  PruneUnavailableSourcesRule rule(uuid_to_id_map_[uuid], carnot_info, schema_map);
  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  // Should not change anything.
  ASSERT_FALSE(rule_or_s.ConsumeValueOrDie());

  // Sub-plan 1 is not deleted.
  EXPECT_TRUE(graph->HasNode(udtf_src_id));
  EXPECT_TRUE(graph->HasNode(grpc_sink1_id));

  // Sub-plan 2 is not deleted.
  EXPECT_TRUE(graph->HasNode(mem_src_id));
  EXPECT_TRUE(graph->HasNode(grpc_sink2_id));
}

TEST_F(PruneUnavailableSourcesRuleTest, UDTFOnAllAgentsKeepsPEM) {
  udfspb::UDTFSourceSpec udtf_spec;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kUDTFAllAgents, &udtf_spec));
  // Should only run on one kelvin.
  EXPECT_EQ(udtf_spec.executor(), udfspb::UDTF_ALL_AGENTS);

  // Sub-plan 1 should not be deleted.
  auto udtf_src = MakeUDTFSource(udtf_spec, {});
  auto grpc_sink1 = MakeGRPCSink(udtf_src, 123);
  auto udtf_src_id = udtf_src->id();
  auto grpc_sink1_id = grpc_sink1->id();

  // Sub-plan 2 should not be deleted.
  auto mem_src = MakeMemSource("http_events");
  auto grpc_sink2 = MakeGRPCSink(mem_src, 456);
  auto mem_src_id = mem_src->id();
  auto grpc_sink2_id = grpc_sink2->id();

  // We want to grab a PEM.
  auto carnot_info = logical_state_.distributed_state().carnot_info()[0];
  ASSERT_TRUE(IsPEM(carnot_info));

  ASSERT_OK_AND_ASSIGN(sole::uuid uuid, ParseUUID(carnot_info.agent_id()));
  ASSERT_OK_AND_ASSIGN(auto schema_map,
                       LoadSchemaMap(logical_state_.distributed_state(), uuid_to_id_map_));
  PruneUnavailableSourcesRule rule(uuid_to_id_map_[uuid], carnot_info, schema_map);
  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_FALSE(rule_or_s.ConsumeValueOrDie());

  // Sub-plan 1 is not deleted.
  EXPECT_TRUE(graph->HasNode(udtf_src_id));
  EXPECT_TRUE(graph->HasNode(grpc_sink1_id));

  // Sub-plan 2 is not deleted.
  EXPECT_TRUE(graph->HasNode(mem_src_id));
  EXPECT_TRUE(graph->HasNode(grpc_sink2_id));
}

TEST_F(PruneUnavailableSourcesRuleTest, UDTFOnAllAgentsKeepsAllKelvinNodes) {
  udfspb::UDTFSourceSpec udtf_spec;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kUDTFAllAgents, &udtf_spec));
  // Should only run on one kelvin.
  EXPECT_EQ(udtf_spec.executor(), udfspb::UDTF_ALL_AGENTS);

  // Sub-plan 1 should not be deleted.
  auto udtf_src = MakeUDTFSource(udtf_spec, {});
  auto grpc_sink = MakeGRPCSink(udtf_src, 123);
  auto udtf_src_id = udtf_src->id();
  auto grpc_sink_id = grpc_sink->id();

  // Sub-plan 2 should not be deleted.
  auto grpc_source1 = MakeGRPCSource(udtf_src->relation());
  auto grpc_source2 = MakeGRPCSource(udtf_src->relation());
  auto union_node = MakeUnion({grpc_source1, grpc_source2});
  auto grpc_source_id1 = grpc_source1->id();
  auto grpc_source_id2 = grpc_source2->id();
  auto union_node_id = union_node->id();

  auto carnot_info = logical_state_.distributed_state().carnot_info()[2];
  // Should be a kelvin.
  ASSERT_TRUE(!IsPEM(carnot_info));
  ASSERT_OK_AND_ASSIGN(sole::uuid uuid, ParseUUID(carnot_info.agent_id()));
  PruneUnavailableSourcesRule rule(uuid_to_id_map_[uuid], carnot_info, {});

  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_FALSE(rule_or_s.ConsumeValueOrDie());

  // Sub-plan 1 is not deleted.
  EXPECT_TRUE(graph->HasNode(udtf_src_id));
  EXPECT_TRUE(graph->HasNode(grpc_sink_id));

  // Sub-plan 2 is not deleted.
  EXPECT_TRUE(graph->HasNode(grpc_source_id1));
  EXPECT_TRUE(graph->HasNode(grpc_source_id2));
  EXPECT_TRUE(graph->HasNode(union_node_id));
}

TEST_F(PruneUnavailableSourcesRuleTest, UDTFOnAllAgentsFilterOnAgentUIDKeepAgent) {
  udfspb::UDTFSourceSpec udtf_spec;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kUDTFAgentUID, &udtf_spec));
  // Should only run on one kelvin.
  EXPECT_EQ(udtf_spec.executor(), udfspb::UDTF_ALL_AGENTS);
  auto carnot_info = logical_state_.distributed_state().carnot_info()[2];

  // Sub-plan 1 should not be deleted.
  auto udtf_src =
      MakeUDTFSource(udtf_spec, {{"agent_uid", MakeString(carnot_info.agent_id().data())}});
  auto grpc_sink = MakeGRPCSink(udtf_src, 123);
  auto udtf_src_id = udtf_src->id();
  auto grpc_sink_id = grpc_sink->id();

  // Sub-plan 2 should not be deleted.
  auto grpc_source1 = MakeGRPCSource(udtf_src->relation());
  auto grpc_source2 = MakeGRPCSource(udtf_src->relation());
  auto union_node = MakeUnion({grpc_source1, grpc_source2});
  auto grpc_source_id1 = grpc_source1->id();
  auto grpc_source_id2 = grpc_source2->id();
  auto union_node_id = union_node->id();

  // Should be a kelvin.
  ASSERT_TRUE(!IsPEM(carnot_info));

  ASSERT_OK_AND_ASSIGN(sole::uuid uuid, ParseUUID(carnot_info.agent_id()));
  PruneUnavailableSourcesRule rule(uuid_to_id_map_[uuid], carnot_info, {});

  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_FALSE(rule_or_s.ConsumeValueOrDie());

  // Sub-plan 1 is not deleted.
  EXPECT_TRUE(graph->HasNode(udtf_src_id));
  EXPECT_TRUE(graph->HasNode(grpc_sink_id));

  // Sub-plan 2 is not deleted.
  EXPECT_TRUE(graph->HasNode(grpc_source_id1));
  EXPECT_TRUE(graph->HasNode(grpc_source_id2));
  EXPECT_TRUE(graph->HasNode(union_node_id));
}

TEST_F(PruneUnavailableSourcesRuleTest, UDTFOnAllAgentsFilterOutNonMatchingAgentUID) {
  udfspb::UDTFSourceSpec udtf_spec;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kUDTFAgentUID, &udtf_spec));
  // Should only run on one kelvin.
  EXPECT_EQ(udtf_spec.executor(), udfspb::UDTF_ALL_AGENTS);
  auto carnot_info = logical_state_.distributed_state().carnot_info()[0];

  // Sub-plan 1 should be removed.
  auto udtf_src = MakeUDTFSource(udtf_spec, {{"agent_uid", MakeString("kelvin")}});
  auto grpc_sink = MakeGRPCSink(udtf_src, 123);
  auto udtf_src_id = udtf_src->id();
  auto grpc_sink_id = grpc_sink->id();

  ASSERT_NE("kelvin", carnot_info.query_broker_address());

  // Sub-plan 2 should not be removed.
  auto grpc_source1 = MakeGRPCSource(udtf_src->relation());
  auto grpc_source2 = MakeGRPCSource(udtf_src->relation());
  auto union_node = MakeUnion({grpc_source1, grpc_source2});
  auto grpc_source_id1 = grpc_source1->id();
  auto grpc_source_id2 = grpc_source2->id();
  auto union_node_id = union_node->id();

  // Should be a PEM.
  ASSERT_TRUE(IsPEM(carnot_info));
  ASSERT_OK_AND_ASSIGN(sole::uuid uuid, ParseUUID(carnot_info.agent_id()));
  PruneUnavailableSourcesRule rule(uuid_to_id_map_[uuid], carnot_info, {});
  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_TRUE(rule_or_s.ConsumeValueOrDie());

  // Sub-plan 1 is deleted.
  EXPECT_FALSE(graph->HasNode(udtf_src_id));
  EXPECT_FALSE(graph->HasNode(grpc_sink_id));

  // Sub-plan 2 is not deleted.
  EXPECT_TRUE(graph->HasNode(grpc_source_id1));
  EXPECT_TRUE(graph->HasNode(grpc_source_id2));
  EXPECT_TRUE(graph->HasNode(union_node_id));
}

using DistributedPruneUnavailableSourcesRuleTest = DistributedRulesTest;
TEST_F(DistributedPruneUnavailableSourcesRuleTest, AllAgentsUDTFFiltersNoOne) {
  auto plan = CoordinateQuery("import px\npx.display(px._Test_MD_State())");
  // id = 1 && id = 2 should be agents.
  auto agent1_instance = plan->Get(1);
  ASSERT_TRUE(IsPEM(agent1_instance->carnot_info()));
  auto agent2_instance = plan->Get(2);
  ASSERT_TRUE(IsPEM(agent2_instance->carnot_info()));

  // id = 0  should be a Kelvin.
  auto kelvin_instance = plan->Get(0);
  ASSERT_TRUE(!IsPEM(kelvin_instance->carnot_info()));

  // Before the rule, we should have UDTFs on every node.
  auto udtf_sources_agent1 = agent1_instance->plan()->FindNodesOfType(IRNodeType::kUDTFSource);
  EXPECT_EQ(udtf_sources_agent1.size(), 1);
  auto udtf_sources_agent2 = agent2_instance->plan()->FindNodesOfType(IRNodeType::kUDTFSource);
  EXPECT_EQ(udtf_sources_agent2.size(), 1);
  auto kelvin_sources = kelvin_instance->plan()->FindNodesOfType(IRNodeType::kUDTFSource);
  EXPECT_EQ(kelvin_sources.size(), 1);

  DistributedPruneUnavailableSourcesRule rule({});
  auto result_or_s = rule.Execute(plan.get());
  ASSERT_OK(result_or_s);
  ASSERT_FALSE(result_or_s.ConsumeValueOrDie());

  // After the rule, we should still have UDTFs on every node.
  udtf_sources_agent1 = agent1_instance->plan()->FindNodesOfType(IRNodeType::kUDTFSource);
  EXPECT_EQ(udtf_sources_agent1.size(), 1);
  udtf_sources_agent2 = agent2_instance->plan()->FindNodesOfType(IRNodeType::kUDTFSource);
  EXPECT_EQ(udtf_sources_agent2.size(), 1);
  kelvin_sources = kelvin_instance->plan()->FindNodesOfType(IRNodeType::kUDTFSource);
  EXPECT_EQ(kelvin_sources.size(), 1);
}

using AnnotateAbortableSrcsForLimitsRuleTest = DistributedRulesTest;
TEST_F(AnnotateAbortableSrcsForLimitsRuleTest, SourceLimitSink) {
  auto mem_src = MakeMemSource("http_events");
  auto limit = MakeLimit(mem_src, 10);
  MakeMemSink(limit, "output");

  AnnotateAbortableSrcsForLimitsRule rule;

  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_TRUE(rule_or_s.ConsumeValueOrDie());

  // Limit should have mem_src as abortable source.
  EXPECT_THAT(limit->abortable_srcs(), ::testing::UnorderedElementsAre(mem_src->id()));
}

TEST_F(AnnotateAbortableSrcsForLimitsRuleTest, MultipleSourcesUnioned) {
  auto mem_src1 = MakeMemSource("http_events");
  auto mem_src2 = MakeMemSource("http_events");
  auto mem_src3 = MakeMemSource("http_events");
  auto union_node = MakeUnion({mem_src1, mem_src2, mem_src3});
  auto limit = MakeLimit(union_node, 10);
  MakeMemSink(limit, "output");

  AnnotateAbortableSrcsForLimitsRule rule;

  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_TRUE(rule_or_s.ConsumeValueOrDie());

  // Limit should have all mem_src as abortable sources.
  EXPECT_THAT(limit->abortable_srcs(),
              ::testing::UnorderedElementsAre(mem_src1->id(), mem_src2->id(), mem_src3->id()));
}

TEST_F(AnnotateAbortableSrcsForLimitsRuleTest, DisjointGraphs) {
  auto mem_src1 = MakeMemSource("http_events");
  auto limit1 = MakeLimit(mem_src1, 10);
  MakeMemSink(limit1, "output1");

  auto mem_src2 = MakeMemSource("http_events");
  auto limit2 = MakeLimit(mem_src2, 10);
  MakeMemSink(limit2, "output2");

  AnnotateAbortableSrcsForLimitsRule rule;

  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_TRUE(rule_or_s.ConsumeValueOrDie());

  // First limit should have first mem src as abortable source
  EXPECT_THAT(limit1->abortable_srcs(), ::testing::UnorderedElementsAre(mem_src1->id()));
  // Second limit should have second mem src as abortable source
  EXPECT_THAT(limit2->abortable_srcs(), ::testing::UnorderedElementsAre(mem_src2->id()));
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
