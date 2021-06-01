/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <unordered_map>
#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/analyzer/analyzer.h"
#include "src/carnot/planner/compiler/compiler.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/distributed_coordinator.h"
#include "src/carnot/planner/distributed/distributed_rules.h"
#include "src/carnot/planner/parser/parser.h"
#include "src/carnot/planner/rules/rule_mock.h"
#include "src/carnot/planner/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

using table_store::schema::Relation;
using table_store::schemapb::Schema;
using ::testing::_;
using ::testing::ElementsAre;
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
  ASSERT_OK_AND_ASSIGN(sole::uuid uuid, ParseUUID(carnot_info.agent_id()));

  // Sub-plan 1 should not be deleted.
  auto udtf_src = MakeUDTFSource(udtf_spec, {{"agent_uid", MakeString(uuid.str())}});
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

TEST_F(DistributedRulesTest, ScalarUDFRunOnKelvinRuleTest) {
  // Kelvin-only plan
  MemorySourceIR* src1 = MakeMemSource("http_events");
  auto func1 = MakeFunc("kelvin_only", {});
  func1->SetOutputDataType(types::DataType::STRING);
  MapIR* map1 = MakeMap(src1, {{"out", func1}});
  MakeMemSink(map1, "foo", {});

  ScalarUDFsRunOnKelvinRule rule(compiler_state_.get());

  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_FALSE(rule_or_s.ConsumeValueOrDie());

  // PEM-only plan
  MemorySourceIR* src2 = MakeMemSource("http_events");
  auto func2 = MakeFunc("pem_only", {});
  func2->SetOutputDataType(types::DataType::STRING);
  MapIR* map2 = MakeMap(src2, {{"out", func2}}, false);
  MakeMemSink(map2, "foo", {});

  rule_or_s = rule.Execute(graph.get());
  ASSERT_NOT_OK(rule_or_s);
  EXPECT_THAT(
      rule_or_s.status(),
      HasCompilerError(
          "UDF 'pem_only' must execute before blocking nodes such as limit, agg, and join."));
}

TEST_F(DistributedRulesTest, ScalarUDFRunOnPEMRuleTest) {
  // PEM-only plan
  MemorySourceIR* src1 = MakeMemSource("http_events");
  auto func1 = MakeFunc("pem_only", {});
  func1->SetOutputDataType(types::DataType::STRING);
  auto equals_func1 = MakeEqualsFunc(func1, MakeString("abc"));
  equals_func1->SetOutputDataType(types::DataType::BOOLEAN);
  FilterIR* filter1 = MakeFilter(src1, equals_func1);
  MakeMemSink(filter1, "foo", {});

  ScalarUDFsRunOnPEMRule rule(compiler_state_.get());

  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_FALSE(rule_or_s.ConsumeValueOrDie());

  // Kelvin-only plan
  MemorySourceIR* src2 = MakeMemSource("http_events");
  auto func2 = MakeFunc("kelvin_only", {});
  func2->SetOutputDataType(types::DataType::STRING);
  auto equals_func2 = MakeEqualsFunc(func2, MakeString("abc"));
  equals_func2->SetOutputDataType(types::DataType::BOOLEAN);
  FilterIR* filter2 = MakeFilter(src2, equals_func2);
  MakeMemSink(filter2, "foo", {});

  rule_or_s = rule.Execute(graph.get());
  ASSERT_NOT_OK(rule_or_s);
  EXPECT_THAT(
      rule_or_s.status(),
      HasCompilerError(
          "UDF 'kelvin_only' must execute after blocking nodes such as limit, agg, and join."));
}

using SplitPEMandKelvinOnlyUDFOperatorRuleTest = DistributedRulesTest;
TEST_F(SplitPEMandKelvinOnlyUDFOperatorRuleTest, noop) {
  // Kelvin-only plan
  MemorySourceIR* src1 = MakeMemSource("http_events");
  auto func1 = MakeEqualsFunc(MakeInt(3), MakeInt(2));
  func1->SetOutputDataType(types::DataType::STRING);
  MapIR* map1 = MakeMap(src1, {{"out", func1}});
  MakeMemSink(map1, "foo", {});

  SplitPEMandKelvinOnlyUDFOperatorRule rule(compiler_state_.get());

  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_FALSE(rule_or_s.ConsumeValueOrDie());
}

TEST_F(SplitPEMandKelvinOnlyUDFOperatorRuleTest, simple) {
  // Kelvin-only plan
  MemorySourceIR* src1 = MakeMemSource("http_events");
  ASSERT_OK(
      src1->SetRelation(Relation({types::STRING, types::STRING}, {"remote_addr", "req_path"})));
  auto input1 = MakeColumn("remote_addr", 0);
  auto input2 = MakeColumn("req_path", 0);
  input1->ResolveColumnType(types::DataType::STRING);
  input2->ResolveColumnType(types::DataType::STRING);
  auto func1 = MakeFunc("pem_only", {input1});
  auto func2 = MakeFunc("kelvin_only", {input2});
  func1->SetOutputDataType(types::DataType::STRING);
  func2->SetOutputDataType(types::DataType::STRING);
  MapIR* map1 = MakeMap(src1, {{"pem", func1}, {"kelvin", func2}});
  ASSERT_OK(map1->SetRelationFromExprs());
  MemorySinkIR* sink = MakeMemSink(map1, "foo", {});

  Relation existing_map_relation({types::STRING, types::STRING}, {"pem", "kelvin"});
  EXPECT_EQ(map1->relation(), existing_map_relation);

  SplitPEMandKelvinOnlyUDFOperatorRule rule(compiler_state_.get());
  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_TRUE(rule_or_s.ConsumeValueOrDie());

  ASSERT_EQ(1, src1->Children().size());
  EXPECT_NE(src1->Children()[0], map1);
  EXPECT_MATCH(src1->Children()[0], Map());
  auto new_map = static_cast<MapIR*>(src1->Children()[0]);
  Relation expected_map_relation({types::STRING, types::STRING}, {"pem_only_0", "req_path"});
  EXPECT_EQ(new_map->relation(), expected_map_relation);
  EXPECT_THAT(new_map->parents(), ElementsAre(src1));
  EXPECT_THAT(new_map->Children(), ElementsAre(map1));

  // original map relation and children shouldn't have changed.
  // pem_only func should now be a column projection.
  EXPECT_EQ(map1->relation(), existing_map_relation);
  EXPECT_EQ(2, map1->col_exprs().size());
  EXPECT_EQ("pem", map1->col_exprs()[0].name);
  EXPECT_MATCH(map1->col_exprs()[0].node, ColumnNode("pem_only_0"));
  EXPECT_EQ("kelvin", map1->col_exprs()[1].name);
  EXPECT_MATCH(map1->col_exprs()[1].node,
               FuncNameAllArgsMatch("kelvin_only", ColumnNode("req_path")));
  EXPECT_THAT(map1->parents(), ElementsAre(new_map));
  EXPECT_THAT(map1->Children(), ElementsAre(sink));
}

TEST_F(SplitPEMandKelvinOnlyUDFOperatorRuleTest, nested) {
  // Kelvin-only plan
  MemorySourceIR* src1 = MakeMemSource("http_events");
  ASSERT_OK(src1->SetRelation(Relation({types::STRING}, {"remote_addr"})));
  auto input1 = MakeColumn("remote_addr", 0);
  input1->ResolveColumnType(types::DataType::STRING);
  auto func1 = MakeFunc("pem_only", {input1});
  auto func2 = MakeFunc("kelvin_only", {func1});
  func1->SetOutputDataType(types::DataType::STRING);
  func2->SetOutputDataType(types::DataType::STRING);
  MapIR* map1 = MakeMap(src1, {{"kelvin", func2}});
  ASSERT_OK(map1->SetRelationFromExprs());
  MemorySinkIR* sink = MakeMemSink(map1, "foo", {});

  Relation existing_map_relation({types::STRING}, {"kelvin"});
  EXPECT_EQ(map1->relation(), existing_map_relation);

  SplitPEMandKelvinOnlyUDFOperatorRule rule(compiler_state_.get());
  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_TRUE(rule_or_s.ConsumeValueOrDie());

  ASSERT_EQ(1, src1->Children().size());
  EXPECT_NE(src1->Children()[0], map1);
  EXPECT_MATCH(src1->Children()[0], Map());
  auto new_map = static_cast<MapIR*>(src1->Children()[0]);
  Relation expected_map_relation({types::STRING}, {"pem_only_0"});
  EXPECT_EQ(expected_map_relation, new_map->relation());
  EXPECT_THAT(new_map->parents(), ElementsAre(src1));
  EXPECT_THAT(new_map->Children(), ElementsAre(map1));

  // original map relation and children shouldn't have changed.
  EXPECT_EQ(map1->relation(), existing_map_relation);
  EXPECT_EQ(1, map1->col_exprs().size());
  EXPECT_EQ("kelvin", map1->col_exprs()[0].name);
  EXPECT_MATCH(map1->col_exprs()[0].node,
               FuncNameAllArgsMatch("kelvin_only", ColumnNode("pem_only_0")));
  EXPECT_THAT(map1->parents(), ElementsAre(new_map));
  EXPECT_THAT(map1->Children(), ElementsAre(sink));
}

TEST_F(SplitPEMandKelvinOnlyUDFOperatorRuleTest, name_collision) {
  // Kelvin-only plan
  MemorySourceIR* src1 = MakeMemSource("http_events");
  ASSERT_OK(src1->SetRelation(Relation({types::STRING}, {"pem_only_0"})));
  auto input1 = MakeColumn("pem_only_0", 0);
  input1->ResolveColumnType(types::DataType::STRING);
  auto func1 = MakeFunc("pem_only", {input1});
  auto func2 = MakeFunc("pem_only", {input1});
  func1->SetOutputDataType(types::DataType::STRING);
  func2->SetOutputDataType(types::DataType::STRING);
  MapIR* map1 = MakeMap(src1, {{"pem1", func1}, {"pem2", func2}, {"pem_only_0", input1}});
  ASSERT_OK(map1->SetRelationFromExprs());
  MemorySinkIR* sink = MakeMemSink(map1, "foo", {});

  Relation existing_map_relation({types::STRING, types::STRING, types::STRING},
                                 {"pem1", "pem2", "pem_only_0"});
  EXPECT_EQ(map1->relation(), existing_map_relation);

  SplitPEMandKelvinOnlyUDFOperatorRule rule(compiler_state_.get());
  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_TRUE(rule_or_s.ConsumeValueOrDie());

  ASSERT_EQ(1, src1->Children().size());
  EXPECT_NE(src1->Children()[0], map1);
  EXPECT_MATCH(src1->Children()[0], Map());
  auto new_map = static_cast<MapIR*>(src1->Children()[0]);
  Relation expected_map_relation({types::STRING, types::STRING, types::STRING},
                                 {"pem_only_1", "pem_only_2", "pem_only_0"});
  EXPECT_EQ(new_map->relation(), expected_map_relation);
  EXPECT_THAT(new_map->parents(), ElementsAre(src1));
  EXPECT_THAT(new_map->Children(), ElementsAre(map1));

  // original map relation and children shouldn't have changed.
  // pem_only func should now be a column projection.
  EXPECT_EQ(map1->relation(), existing_map_relation);
  EXPECT_EQ(3, map1->col_exprs().size());
  EXPECT_EQ("pem1", map1->col_exprs()[0].name);
  EXPECT_MATCH(map1->col_exprs()[0].node, ColumnNode("pem_only_1"));
  EXPECT_EQ("pem2", map1->col_exprs()[1].name);
  EXPECT_MATCH(map1->col_exprs()[1].node, ColumnNode("pem_only_2"));
  EXPECT_EQ("pem_only_0", map1->col_exprs()[2].name);
  EXPECT_MATCH(map1->col_exprs()[2].node, ColumnNode("pem_only_0"));
  EXPECT_THAT(map1->parents(), ElementsAre(new_map));
  EXPECT_THAT(map1->Children(), ElementsAre(sink));
}

TEST_F(SplitPEMandKelvinOnlyUDFOperatorRuleTest, filter) {
  // Kelvin-only plan
  MemorySourceIR* src1 = MakeMemSource("http_events");
  ASSERT_OK(
      src1->SetRelation(Relation({types::STRING, types::STRING}, {"remote_addr", "req_path"})));
  auto input1 = MakeColumn("remote_addr", 0);
  auto input2 = MakeColumn("req_path", 0);
  input1->ResolveColumnType(types::DataType::STRING);
  input2->ResolveColumnType(types::DataType::STRING);
  auto func1 = MakeFunc("pem_only", {input1});
  auto func2 = MakeFunc("kelvin_only", {input2});
  func1->SetOutputDataType(types::DataType::STRING);
  func2->SetOutputDataType(types::DataType::STRING);
  auto func3 = MakeEqualsFunc(func1, func2);
  func3->SetOutputDataType(types::DataType::BOOLEAN);
  FilterIR* filter = MakeFilter(src1, func3);
  ASSERT_OK(filter->SetRelation(src1->relation()));
  MemorySinkIR* sink = MakeMemSink(filter, "foo", {});

  Relation existing_filter_relation({types::STRING, types::STRING}, {"remote_addr", "req_path"});
  EXPECT_EQ(filter->relation(), existing_filter_relation);

  SplitPEMandKelvinOnlyUDFOperatorRule rule(compiler_state_.get());
  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_TRUE(rule_or_s.ConsumeValueOrDie());

  ASSERT_EQ(1, src1->Children().size());
  EXPECT_NE(src1->Children()[0], filter);
  EXPECT_MATCH(src1->Children()[0], Map());
  auto new_map = static_cast<MapIR*>(src1->Children()[0]);
  Relation expected_map_relation({types::STRING, types::STRING, types::STRING},
                                 {"pem_only_0", "remote_addr", "req_path"});
  EXPECT_THAT(new_map->relation(), UnorderedRelationMatches(expected_map_relation));
  EXPECT_THAT(new_map->parents(), ElementsAre(src1));
  EXPECT_THAT(new_map->Children(), ElementsAre(filter));

  // original map relation and children shouldn't have changed.
  // pem_only func should now be a column projection.
  EXPECT_EQ(filter->relation(), existing_filter_relation);
  EXPECT_MATCH(filter->filter_expr(), Func());
  auto func = static_cast<FuncIR*>(filter->filter_expr());
  EXPECT_EQ("equal", func->func_name());
  EXPECT_EQ(2, func->args().size());
  EXPECT_MATCH(func->args()[0], ColumnNode("pem_only_0"));
  EXPECT_MATCH(func->args()[1], Func("kelvin_only"));
  EXPECT_THAT(filter->parents(), ElementsAre(new_map));
  EXPECT_THAT(filter->Children(), ElementsAre(sink));
}

using LimitPushdownRuleTest = DistributedRulesTest;
TEST_F(LimitPushdownRuleTest, simple_no_op) {
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource(relation);
  FilterIR* filter = MakeFilter(src, MakeEqualsFunc(MakeColumn("abc", 0), MakeColumn("xyz", 0)));
  LimitIR* limit = MakeLimit(filter, 10);
  MakeMemSink(limit, "foo", {});

  LimitPushdownRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
}

TEST_F(LimitPushdownRuleTest, simple) {
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});

  MemorySourceIR* src = MakeMemSource(relation);
  MapIR* map1 = MakeMap(src, {{"abc", MakeColumn("abc", 0)}}, false);
  MapIR* map2 = MakeMap(map1, {{"def", MakeColumn("abc", 0)}}, false);
  LimitIR* limit = MakeLimit(map2, 10);
  MemorySinkIR* sink = MakeMemSink(limit, "foo", {});

  LimitPushdownRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_THAT(sink->parents(), ElementsAre(map2));
  EXPECT_THAT(map2->parents(), ElementsAre(map1));
  EXPECT_EQ(1, map1->parents().size());
  auto new_limit = map1->parents()[0];
  EXPECT_MATCH(new_limit, Limit());
  EXPECT_EQ(10, static_cast<LimitIR*>(new_limit)->limit_value());
  EXPECT_THAT(new_limit->parents(), ElementsAre(src));

  // Ensure the limit inherits the relation from its parent, not the previous location.
  EXPECT_EQ(new_limit->relation(), relation);
}

TEST_F(LimitPushdownRuleTest, pem_only) {
  Relation relation1({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  Relation relation2({types::DataType::INT64}, {"abc"});

  MemorySourceIR* src = MakeMemSource(relation1);
  MapIR* map1 = MakeMap(src, {{"abc", MakeFunc("pem_only", {})}}, false);
  ASSERT_OK(map1->SetRelation(relation2));
  MapIR* map2 = MakeMap(map1, {{"def", MakeColumn("abc", 0)}}, false);
  LimitIR* limit = MakeLimit(map2, 10);
  MemorySinkIR* sink = MakeMemSink(limit, "foo", {});

  LimitPushdownRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_THAT(sink->parents(), ElementsAre(map2));
  EXPECT_EQ(1, map2->parents().size());
  auto new_limit = map2->parents()[0];
  EXPECT_MATCH(new_limit, Limit());
  EXPECT_EQ(10, static_cast<LimitIR*>(new_limit)->limit_value());
  EXPECT_THAT(new_limit->parents(), ElementsAre(map1));
  EXPECT_THAT(map1->parents(), ElementsAre(src));

  // Ensure the limit inherits the relation from its parent, not the previous location.
  EXPECT_EQ(new_limit->relation(), relation2);
}

TEST_F(LimitPushdownRuleTest, multi_branch_union) {
  Relation relation1({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  Relation relation2({types::DataType::INT64}, {"abc"});

  MemorySourceIR* src1 = MakeMemSource(relation1);
  MapIR* map1 = MakeMap(src1, {{"abc", MakeColumn("abc", 0)}}, false);

  MemorySourceIR* src2 = MakeMemSource(relation2);
  MapIR* map2 = MakeMap(src2, {{"abc", MakeColumn("abc", 0)}}, false);

  UnionIR* union_node = MakeUnion({map1, map2});
  ASSERT_OK(union_node->SetRelation(relation2));
  MapIR* map3 = MakeMap(union_node, {{"abc", MakeColumn("abc", 0)}}, false);

  LimitIR* limit = MakeLimit(map3, 10);
  MemorySinkIR* sink = MakeMemSink(limit, "foo", {});

  LimitPushdownRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_THAT(sink->parents(), ElementsAre(map3));

  // There should be 3 copies of the limit -- one before of each branch of the
  // union, and one after the union.
  EXPECT_EQ(1, map3->parents().size());
  auto new_limit = map3->parents()[0];
  EXPECT_EQ(10, static_cast<LimitIR*>(new_limit)->limit_value());
  EXPECT_THAT(new_limit->parents(), ElementsAre(union_node));
  EXPECT_EQ(new_limit->relation(), relation2);

  EXPECT_THAT(union_node->parents(), ElementsAre(map1, map2));

  EXPECT_EQ(1, map1->parents().size());
  auto new_limit1 = map1->parents()[0];
  EXPECT_MATCH(new_limit1, Limit());
  EXPECT_EQ(10, static_cast<LimitIR*>(new_limit1)->limit_value());
  EXPECT_THAT(new_limit1->parents(), ElementsAre(src1));
  // Ensure the limit inherits the relation from its parent, not the previous location.
  EXPECT_EQ(new_limit1->relation(), relation1);

  EXPECT_EQ(1, map2->parents().size());
  auto new_limit2 = map2->parents()[0];
  EXPECT_MATCH(new_limit2, Limit());
  EXPECT_EQ(10, static_cast<LimitIR*>(new_limit2)->limit_value());
  EXPECT_THAT(new_limit2->parents(), ElementsAre(src2));
  // Ensure the limit inherits the relation from its parent, not the previous location.
  EXPECT_EQ(new_limit2->relation(), relation2);
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
