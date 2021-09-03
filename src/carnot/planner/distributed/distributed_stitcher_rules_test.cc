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

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/analyzer/resolve_types_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/coordinator/coordinator.h"
#include "src/carnot/planner/distributed/distributed_plan/distributed_plan.h"
#include "src/carnot/planner/distributed/distributed_planner.h"
#include "src/carnot/planner/distributed/distributed_stitcher_rules.h"
#include "src/carnot/planner/distributed/grpc_source_conversion.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/rules/rules.h"
#include "src/carnot/planner/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/uuid/uuid_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {
using ::testing::_;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;
using testutils::DistributedRulesTest;
using testutils::kOnePEMOneKelvinDistributedState;
using testutils::kOnePEMThreeKelvinsDistributedState;
using testutils::kThreePEMsOneKelvinDistributedState;

class StitcherTest : public DistributedRulesTest {
 protected:
  distributedpb::DistributedState LoadDistributedStatePb(const std::string& physical_state_txt) {
    distributedpb::DistributedState physical_state_pb;
    CHECK(google::protobuf::TextFormat::MergeFromString(physical_state_txt, &physical_state_pb));
    return physical_state_pb;
  }

  void MakeSourceSinkGraph() {
    auto mem_source = MakeMemSource(MakeRelation());
    compiler_state_->relation_map()->emplace("table", MakeRelation());
    MakeMemSink(mem_source, "out");

    compiler::ResolveTypesRule rule(compiler_state_.get());
    ASSERT_OK(rule.Execute(graph.get()));
  }

  std::unique_ptr<DistributedPlan> MakeDistributedPlan(const distributedpb::DistributedState& ps) {
    auto coordinator = Coordinator::Create(compiler_state_.get(), ps).ConsumeValueOrDie();

    MakeSourceSinkGraph();

    auto physical_plan = coordinator->Coordinate(graph.get()).ConsumeValueOrDie();
    return physical_plan;
  }

  void TestBeforeSetSourceGroupGRPCAddress(std::vector<CarnotInstance*> data_stores,
                                           std::vector<CarnotInstance*> mergers) {
    for (auto data_store_agent : data_stores) {
      // Make sure that the grpc_sink has not yet been set to avoid a false positive.
      for (auto ir_node : data_store_agent->plan()->FindNodesThatMatch(InternalGRPCSink())) {
        SCOPED_TRACE(data_store_agent->carnot_info().query_broker_address());
        auto grpc_sink = static_cast<GRPCSinkIR*>(ir_node);
        EXPECT_FALSE(grpc_sink->DestinationAddressSet());
      }
    }

    for (auto merger_agent : mergers) {
      // Make sure that the grpc_source_group addresses have not yet been set to avoid a false
      // positive.
      for (auto ir_node : merger_agent->plan()->FindNodesOfType(IRNodeType::kGRPCSourceGroup)) {
        SCOPED_TRACE(absl::Substitute("agent : $0, node :$1",
                                      merger_agent->carnot_info().query_broker_address(),
                                      ir_node->DebugString()));
        auto grpc_source_group = static_cast<GRPCSourceGroupIR*>(ir_node);
        EXPECT_FALSE(grpc_source_group->GRPCAddressSet());
        // EXPECT_FALSE(grpc_source_group->GRPCAddressSet());
        EXPECT_EQ(grpc_source_group->dependent_sinks().size(), 0);
      }
    }
  }

  // Test to make sure that GRPCAddresses are set properly and that nothing else is set in the grpc
  // source group.
  void TestGRPCAddressSet(std::vector<CarnotInstance*> mergers) {
    for (auto merger_agent : mergers) {
      // Make sure GRPCAddress is now set.
      for (auto ir_node : merger_agent->plan()->FindNodesOfType(IRNodeType::kGRPCSourceGroup)) {
        SCOPED_TRACE(absl::Substitute("agent : $0, node :$1",
                                      merger_agent->carnot_info().query_broker_address(),
                                      ir_node->DebugString()));
        auto grpc_source_group = static_cast<GRPCSourceGroupIR*>(ir_node);
        EXPECT_TRUE(grpc_source_group->GRPCAddressSet());
        // We have yet to set the dependent sinks.
        EXPECT_EQ(grpc_source_group->dependent_sinks().size(), 0);
        EXPECT_EQ(grpc_source_group->grpc_address(), merger_agent->carnot_info().grpc_address());
      }
    }
  }

  // Test to make sure that GRPC sinks are all connected to GRPC sources in some plan in the graph.
  void TestGRPCBridgesWiring(std::vector<CarnotInstance*> data_stores,
                             std::vector<CarnotInstance*> mergers) {
    absl::flat_hash_set<GRPCSinkIR*> sinks_referenced_by_sources;
    absl::flat_hash_set<GRPCSinkIR*> sinks_found_in_plans;
    for (auto merger_agent : mergers) {
      // Load the sinks refernenced by the GRPC source group and makes sure they are in the agent
      // plan.
      for (auto ir_node : merger_agent->plan()->FindNodesOfType(IRNodeType::kGRPCSourceGroup)) {
        auto grpc_source_group = static_cast<GRPCSourceGroupIR*>(ir_node);
        for (auto sink : grpc_source_group->dependent_sinks()) {
          sinks_referenced_by_sources.insert(sink.first);
        }
      }
    }

    for (auto data_store_agent : data_stores) {
      // Now check that the sink exists for the grpc sources.
      for (auto ir_node : data_store_agent->plan()->FindNodesThatMatch(InternalGRPCSink())) {
        SCOPED_TRACE(absl::Substitute("agent : $0, node :$1",
                                      data_store_agent->carnot_info().query_broker_address(),
                                      ir_node->DebugString()));
        auto grpc_sink = static_cast<GRPCSinkIR*>(ir_node);
        EXPECT_TRUE(sinks_referenced_by_sources.contains(grpc_sink));
        sinks_found_in_plans.insert(grpc_sink);
      }
    }
    EXPECT_THAT(sinks_found_in_plans, UnorderedElementsAreArray(sinks_referenced_by_sources));
  }

  // Test to make sure that GRPCSourceGroups become GRPCSources that are referenced by all sinks.
  void TestGRPCBridgesExpandedCorrectly(std::vector<CarnotInstance*> data_stores,
                                        std::vector<CarnotInstance*> mergers) {
    // Kelvin plan should no longer have grpc source groups.
    std::vector<int64_t> source_ids;
    std::vector<int64_t> destination_ids;

    for (auto merger_agent : mergers) {
      SCOPED_TRACE(
          absl::Substitute("agent : $0", merger_agent->carnot_info().query_broker_address()));
      EXPECT_EQ(merger_agent->plan()->FindNodesOfType(IRNodeType::kGRPCSourceGroup).size(), 0);
      // Node ids of GRPCSources should be the same as the destination ids.
      for (auto ir_node : merger_agent->plan()->FindNodesOfType(IRNodeType::kGRPCSource)) {
        auto grpc_source = static_cast<GRPCSourceIR*>(ir_node);
        source_ids.push_back(grpc_source->id());
      }
    }

    for (auto data_store_agent : data_stores) {
      // Now check that the sink exists for the grpc sources.
      for (auto ir_node : data_store_agent->plan()->FindNodesThatMatch(InternalGRPCSink())) {
        SCOPED_TRACE(absl::Substitute("agent : $0, node :$1",
                                      data_store_agent->carnot_info().query_broker_address(),
                                      ir_node->DebugString()));
        auto sink = static_cast<GRPCSinkIR*>(ir_node);
        // Test GRPCSinks for expected GRPC destination address, as well as the proper physical id
        // being set, as well as being set to the correct value.
        EXPECT_TRUE(sink->DestinationAddressSet());
        auto grpc_sink_destination =
            sink->agent_id_to_destination_id().find(data_store_agent->id())->second;
        destination_ids.emplace_back(grpc_sink_destination);
      }
    }

    EXPECT_THAT(source_ids, UnorderedElementsAreArray(destination_ids));
  }
};

TEST_F(StitcherTest, one_pem_one_kelvin) {
  auto ps = LoadDistributedStatePb(kOnePEMOneKelvinDistributedState);
  auto physical_plan = MakeDistributedPlan(ps);

  EXPECT_THAT(physical_plan->dag().TopologicalSort(), ElementsAre(1, 0));
  CarnotInstance* kelvin = physical_plan->Get(0);

  std::string kelvin_qb_address = "kelvin";
  ASSERT_EQ(kelvin->carnot_info().query_broker_address(), kelvin_qb_address);

  CarnotInstance* pem = physical_plan->Get(1);
  {
    SCOPED_TRACE("one_pem_one_kelvin");
    TestBeforeSetSourceGroupGRPCAddress({kelvin, pem}, {kelvin});
  }

  // Execute the address rule.
  DistributedSetSourceGroupGRPCAddressRule rule;
  auto node_changed_or_s = rule.Execute(physical_plan.get());
  ASSERT_OK(node_changed_or_s);
  ASSERT_TRUE(node_changed_or_s.ConsumeValueOrDie());

  {
    SCOPED_TRACE("one_pem_one_kelvin");
    TestGRPCAddressSet({kelvin});
  }

  // Associate the edges of the graph.
  AssociateDistributedPlanEdgesRule distributed_edges_rule;
  node_changed_or_s = distributed_edges_rule.Execute(physical_plan.get());
  ASSERT_OK(node_changed_or_s);
  ASSERT_TRUE(node_changed_or_s.ConsumeValueOrDie());

  {
    SCOPED_TRACE("one_pem_one_kelvin");
    TestGRPCBridgesWiring({kelvin, pem}, {kelvin});
  }

  DistributedIRRule<GRPCSourceGroupConversionRule> distributed_grpc_source_conv_rule;
  node_changed_or_s = distributed_grpc_source_conv_rule.Execute(physical_plan.get());
  ASSERT_OK(node_changed_or_s);
  ASSERT_TRUE(node_changed_or_s.ConsumeValueOrDie());

  {
    SCOPED_TRACE("one_pem_one_kelvin");
    TestGRPCBridgesExpandedCorrectly({kelvin, pem}, {kelvin});
  }
}

TEST_F(StitcherTest, three_pems_one_kelvin) {
  auto ps = LoadDistributedStatePb(kThreePEMsOneKelvinDistributedState);
  auto physical_plan = MakeDistributedPlan(ps);
  auto topo_sort = physical_plan->dag().TopologicalSort();
  ASSERT_EQ(topo_sort.size(), 4);
  ASSERT_EQ(topo_sort[3], 0);

  CarnotInstance* kelvin = physical_plan->Get(0);
  std::string kelvin_qb_address = "kelvin";
  ASSERT_EQ(kelvin->carnot_info().query_broker_address(), kelvin_qb_address);

  std::vector<CarnotInstance*> data_sources;
  for (int64_t agent_id = 1; agent_id <= 3; ++agent_id) {
    CarnotInstance* agent = physical_plan->Get(agent_id);
    // Quick check to make sure agents are valid.
    ASSERT_THAT(agent->carnot_info().query_broker_address(), HasSubstr("pem"));
    data_sources.push_back(agent);
  }
  // Kelvin can be a data source sometimes.
  data_sources.push_back(kelvin);
  {
    SCOPED_TRACE("three_pems_one_kelvin");
    TestBeforeSetSourceGroupGRPCAddress(data_sources, {kelvin});
  }

  // Execute the address rule.
  DistributedSetSourceGroupGRPCAddressRule rule;
  auto node_changed_or_s = rule.Execute(physical_plan.get());
  ASSERT_OK(node_changed_or_s);
  ASSERT_TRUE(node_changed_or_s.ConsumeValueOrDie());

  {
    SCOPED_TRACE("three_pems_one_kelvin");
    TestGRPCAddressSet({kelvin});
  }

  // Associate the edges of the graph.
  AssociateDistributedPlanEdgesRule distributed_edges_rule;
  node_changed_or_s = distributed_edges_rule.Execute(physical_plan.get());
  ASSERT_OK(node_changed_or_s);
  ASSERT_TRUE(node_changed_or_s.ConsumeValueOrDie());

  {
    SCOPED_TRACE("three_pems_one_kelvin");
    TestGRPCBridgesWiring(data_sources, {kelvin});
  }

  DistributedIRRule<GRPCSourceGroupConversionRule> distributed_grpc_source_conv_rule;
  node_changed_or_s = distributed_grpc_source_conv_rule.Execute(physical_plan.get());
  ASSERT_OK(node_changed_or_s);
  ASSERT_TRUE(node_changed_or_s.ConsumeValueOrDie());

  {
    SCOPED_TRACE("three_pems_one_kelvin");
    TestGRPCBridgesExpandedCorrectly(data_sources, {kelvin});
  }
}

// Test to see whether we can stitch a graph to itself.
TEST_F(StitcherTest, stitch_self_together_with_udtf) {
  auto ps = LoadDistributedStatePb(kOnePEMOneKelvinDistributedState);
  // px.ServiceUpTime() is a Kelvin-Only UDTF, so it should only run on Kelvin.
  auto physical_plan = CoordinateQuery("import px\npx.display(px.ServiceUpTime())", ps);

  EXPECT_THAT(physical_plan->dag().TopologicalSort(), ElementsAre(0));
  CarnotInstance* kelvin = physical_plan->Get(0);

  std::string kelvin_qb_address = "kelvin";
  ASSERT_EQ(kelvin->carnot_info().query_broker_address(), kelvin_qb_address);

  {
    SCOPED_TRACE("stitch_kelvin_to_self");
    TestBeforeSetSourceGroupGRPCAddress({kelvin}, {kelvin});
  }

  // Execute the address rule.
  DistributedSetSourceGroupGRPCAddressRule rule;
  auto node_changed_or_s = rule.Execute(physical_plan.get());
  ASSERT_OK(node_changed_or_s);
  ASSERT_TRUE(node_changed_or_s.ConsumeValueOrDie());

  {
    SCOPED_TRACE("stitch_kelvin_to_self");
    TestGRPCAddressSet({kelvin});
  }

  // Associate the edges of the graph.
  AssociateDistributedPlanEdgesRule distributed_edges_rule;
  node_changed_or_s = distributed_edges_rule.Execute(physical_plan.get());
  ASSERT_OK(node_changed_or_s);
  ASSERT_TRUE(node_changed_or_s.ConsumeValueOrDie());

  {
    SCOPED_TRACE("stitch_kelvin_to_self");
    TestGRPCBridgesWiring({kelvin}, {kelvin});
  }
}

// Test to see whether we can stitch a graph to itself.
TEST_F(StitcherTest, stitch_all_togther_with_udtf) {
  auto ps = LoadDistributedStatePb(kOnePEMOneKelvinDistributedState);
  // px._Test_MDState() is an all agent so it should run on every pem and kelvin.
  auto physical_plan = CoordinateQuery("import px\npx.display(px._Test_MD_State())", ps);

  EXPECT_THAT(physical_plan->dag().TopologicalSort(), ElementsAre(1, 0));
  CarnotInstance* kelvin = physical_plan->Get(0);

  std::string kelvin_id = "00000001-0000-0000-0000-000000000002";
  ASSERT_EQ(ParseUUID(kelvin->carnot_info().agent_id()).ConsumeValueOrDie().str(), kelvin_id);

  CarnotInstance* pem = physical_plan->Get(1);
  ASSERT_EQ(pem->carnot_info().query_broker_address(), "pem");

  std::string pem_id = "00000001-0000-0000-0000-000000000001";
  ASSERT_EQ(ParseUUID(pem->carnot_info().agent_id()).ConsumeValueOrDie().str(), pem_id);
  {
    SCOPED_TRACE("stitch_all_together");
    TestBeforeSetSourceGroupGRPCAddress({kelvin, pem}, {kelvin});
  }

  // Execute the address rule.
  DistributedSetSourceGroupGRPCAddressRule rule;
  auto node_changed_or_s = rule.Execute(physical_plan.get());
  ASSERT_OK(node_changed_or_s);
  ASSERT_TRUE(node_changed_or_s.ConsumeValueOrDie());

  {
    SCOPED_TRACE("stitch_all_together");
    TestGRPCAddressSet({kelvin});
  }

  // Associate the edges of the graph.
  AssociateDistributedPlanEdgesRule distributed_edges_rule;
  node_changed_or_s = distributed_edges_rule.Execute(physical_plan.get());
  ASSERT_OK(node_changed_or_s);
  ASSERT_TRUE(node_changed_or_s.ConsumeValueOrDie());

  // Manually test to make sure kelvin has grpc sinks as well as pem to make sure they are all
  // connected.
  auto kelvin_plan = kelvin->plan();
  auto pem_plan = pem->plan();

  auto kelvin_grpc_sinks = kelvin_plan->FindNodesThatMatch(InternalGRPCSink());
  ASSERT_EQ(kelvin_grpc_sinks.size(), 1);
  auto kelvin_grpc_sink = static_cast<GRPCSinkIR*>(kelvin_grpc_sinks[0]);

  auto pem_grpc_sinks = pem_plan->FindNodesThatMatch(InternalGRPCSink());
  ASSERT_EQ(pem_grpc_sinks.size(), 1);
  auto pem_grpc_sink = static_cast<GRPCSinkIR*>(pem_grpc_sinks[0]);

  auto grpc_sources = kelvin_plan->FindNodesOfType(IRNodeType::kGRPCSourceGroup);
  ASSERT_EQ(grpc_sources.size(), 1);
  auto kelvin_grpc_source = static_cast<GRPCSourceGroupIR*>(grpc_sources[0]);
  std::vector<GRPCSinkIR*> dependent_sinks;
  for (const auto& dep_sinks : kelvin_grpc_source->dependent_sinks()) {
    dependent_sinks.push_back(dep_sinks.first);
  }
  EXPECT_THAT(dependent_sinks, UnorderedElementsAre(pem_grpc_sink, kelvin_grpc_sink));
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
