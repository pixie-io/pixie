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
#include "src/carnot/planner/distributed/distributed_planner.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/rules/rules.h"
#include "src/carnot/planner/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/testing/protobuf.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {
using compiler::ResolveTypesRule;
using px::testing::proto::EqualsProto;
using ::testing::ContainsRegex;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAreArray;
using testutils::DistributedRulesTest;
using testutils::kThreePEMsOneKelvinDistributedState;

constexpr char kOnePEMOneKelvinDistributedState[] = R"proto(
carnot_info {
  query_broker_address: "pem"
  agent_id {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000001
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
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000002
  }
  grpc_address: "1111"
  has_grpc_server: true
  has_data_store: false
  processes_data: true
  accepts_remote_sources: true
  ssl_targetname: "kelvin.pl.svc"
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
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000001
  }
  agent_list {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000002
  }
}
)proto";

class DistributedPlannerTest : public ASTVisitorTest {
 protected:
  distributedpb::DistributedState LoadDistributedStatePb(const std::string& physical_state_txt) {
    distributedpb::DistributedState physical_state_pb;
    CHECK(google::protobuf::TextFormat::MergeFromString(physical_state_txt, &physical_state_pb));
    return physical_state_pb;
  }
};

TEST_F(DistributedPlannerTest, one_pem_one_kelvin) {
  auto mem_src = MakeMemSource(MakeRelation());
  compiler_state_->relation_map()->emplace("table", MakeRelation());
  MakeMemSink(mem_src, "out");

  ResolveTypesRule rule(compiler_state_.get());
  ASSERT_OK(rule.Execute(graph.get()));

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
  ASSERT_EQ(grpc_sources.size(), 1);
  ASSERT_EQ(grpc_sources[0]->type(), IRNodeType::kGRPCSource);

  auto grpc_source = static_cast<GRPCSourceIR*>(grpc_sources[0]);
  // Make sure that the destinations are setup properly.
  for (const auto& [agent_id, destination_id] : grpc_sink->agent_id_to_destination_id()) {
    EXPECT_THAT(destination_id, grpc_source->id());
  }
}

TEST_F(DistributedPlannerTest, three_agents_one_kelvin) {
  auto mem_src = MakeMemSource(MakeRelation());
  compiler_state_->relation_map()->emplace("table", MakeRelation());
  MakeMemSink(mem_src, "out");

  ResolveTypesRule rule(compiler_state_.get());
  ASSERT_OK(rule.Execute(graph.get()));

  distributedpb::DistributedState ps_pb =
      LoadDistributedStatePb(kThreePEMsOneKelvinDistributedState);
  std::unique_ptr<DistributedPlanner> physical_planner =
      DistributedPlanner::Create().ConsumeValueOrDie();
  std::unique_ptr<DistributedPlan> physical_plan =
      physical_planner->Plan(ps_pb, compiler_state_.get(), graph.get()).ConsumeValueOrDie();

  auto topo_sort = physical_plan->dag().TopologicalSort();
  // Last item should be kelvin, id 0.
  ASSERT_EQ(topo_sort.size(), 4);
  ASSERT_EQ(topo_sort[3], 0);

  std::vector<int64_t> grpc_sink_destinations;
  absl::flat_hash_set<IR*> seen_plans;
  for (int64_t i = 1; i <= 3; ++i) {
    SCOPED_TRACE(absl::Substitute("agent id = $0", i));
    auto agent_instance = physical_plan->Get(i);
    EXPECT_THAT(agent_instance->carnot_info().query_broker_address(), ContainsRegex("pem"));

    if (seen_plans.contains(agent_instance->plan())) {
      continue;
    }

    seen_plans.insert(agent_instance->plan());
    std::vector<IRNode*> grpc_sinks =
        agent_instance->plan()->FindNodesOfType(IRNodeType::kGRPCSink);
    ASSERT_EQ(grpc_sinks.size(), 1);
    auto grpc_sink = static_cast<GRPCSinkIR*>(grpc_sinks[0]);
    for (const auto& [agent_id, dest_id] : grpc_sink->agent_id_to_destination_id()) {
      grpc_sink_destinations.push_back(dest_id);
    }
  }

  auto kelvin_instance = physical_plan->Get(0);
  EXPECT_THAT(kelvin_instance->carnot_info().query_broker_address(), ContainsRegex("kelvin"));

  std::vector<IRNode*> unions = kelvin_instance->plan()->FindNodesOfType(IRNodeType::kUnion);
  ASSERT_EQ(unions.size(), 1);
  UnionIR* kelvin_union = static_cast<UnionIR*>(unions[0]);
  ASSERT_EQ(kelvin_union->parents().size(), 3);

  std::vector<int64_t> grpc_source_ids;
  for (OperatorIR* union_parent : kelvin_union->parents()) {
    ASSERT_EQ(union_parent->type(), IRNodeType::kGRPCSource);
    auto grpc_source = static_cast<GRPCSourceIR*>(union_parent);
    grpc_source_ids.push_back(grpc_source->id());
  }

  // Make sure that the destinations are setup properly.
  EXPECT_THAT(grpc_sink_destinations, UnorderedElementsAreArray(grpc_source_ids));
}

using DistributedPlannerUDTFTests = DistributedRulesTest;
TEST_F(DistributedPlannerUDTFTests, UDTFOnlyOnPEMsDoesntRunOnKelvin) {
  uint32_t asid = 123;
  md::UPID upid(asid, 456, 3420030816657ULL);
  std::string upid_str =
      sole::rebuild(absl::Uint128High64(upid.value()), absl::Uint128Low64(upid.value())).str();
  auto physical_plan = PlanQuery(
      absl::Substitute("import px\npx.display(px.OpenNetworkConnections('$0'))", upid_str));

  // // Find the appropriate agents.
  CarnotInstance* pem_instance = nullptr;
  CarnotInstance* kelvin_instance = nullptr;
  for (int64_t plan_id : physical_plan->dag().nodes()) {
    auto instance = physical_plan->Get(plan_id);
    if (IsPEM(instance->carnot_info())) {
      if (instance->carnot_info().query_broker_address() == "pem1") {
        pem_instance = instance;
      }
    } else {
      kelvin_instance = instance;
    }
  }
  ASSERT_NE(kelvin_instance, nullptr);
  ASSERT_NE(pem_instance, nullptr);
  // Make sure that the asid of this matches the UDTF specified one before we run it.
  ASSERT_EQ(pem_instance->carnot_info().query_broker_address(), "pem1");
  ASSERT_EQ(pem_instance->carnot_info().asid(), asid);

  ASSERT_EQ(kelvin_instance->carnot_info().query_broker_address(), "kelvin");
  auto pem_plan = pem_instance->plan();
  auto kelvin_plan = kelvin_instance->plan();

  // Planner should:
  // 1. Remove one agent entirely (pem2) and keep pem1 and kelvin.
  // 2. Remove UDTFSources on Kelvin but keep the rest of the Kelvin plan.
  ASSERT_EQ(physical_plan->dag().nodes().size(), 2UL);
  // Has pem.
  ASSERT_TRUE(physical_plan->HasNode(pem_instance->id()));
  // Has Kelvin.
  ASSERT_TRUE(physical_plan->HasNode(kelvin_instance->id()));

  // PEMs should have 2 nodes, one of which is a UDTF.
  EXPECT_EQ(pem_plan->FindNodesThatMatch(Operator()).size(), 2);
  auto pem_udtfs = pem_plan->FindNodesOfType(IRNodeType::kUDTFSource);
  ASSERT_EQ(pem_udtfs.size(), 1);
  auto new_udtf = static_cast<UDTFSourceIR*>(pem_udtfs[0]);
  ASSERT_EQ(new_udtf->Children().size(), 1UL);
  EXPECT_MATCH(new_udtf->Children()[0], InternalGRPCSink());

  // Kelvin should not have UDTFs.
  auto kelvin_udtfs = kelvin_plan->FindNodesOfType(IRNodeType::kUDTFSource);
  ASSERT_EQ(kelvin_udtfs.size(), 0);
  // Kelvin should have two Operators: Memory Sink and GRPCSource
  EXPECT_EQ(kelvin_plan->FindNodesThatMatch(Operator()).size(), 2);
  auto kelvin_sinks = kelvin_plan->FindNodesThatMatch(ExternalGRPCSink());
  auto new_sink = static_cast<GRPCSinkIR*>(kelvin_sinks[0]);
  ASSERT_EQ(new_sink->parents().size(), 1UL);
  EXPECT_EQ(new_sink->parents()[0]->type(), IRNodeType::kGRPCSource);
}

TEST_F(DistributedPlannerUDTFTests, UDTFOnKelvinOnlyOnKelvin) {
  auto physical_plan = PlanQuery("import px\npx.display(px.ServiceUpTime())");

  // Planner should:
  // 1. Remove all pems but keep kelvin.
  // 2. Remove UDTFSources on Kelvin but keep the rest of the Kelvin plan.
  ASSERT_EQ(physical_plan->dag().nodes().size(), 1UL);
  int64_t kelvin_instance_id = *(physical_plan->dag().nodes().begin());
  CarnotInstance* kelvin_instance = physical_plan->Get(kelvin_instance_id);
  ASSERT_FALSE(IsPEM(kelvin_instance->carnot_info()));
  ASSERT_EQ(kelvin_instance->carnot_info().query_broker_address(), "kelvin");
  auto kelvin_plan = kelvin_instance->plan();

  // We combine GRPCSink and GRPCSource on the same plan.
  // Kelvin should have two Operators that form one subgraphs:
  // UDTFSource -> MemSink
  EXPECT_EQ(kelvin_plan->FindNodesThatMatch(Operator()).size(), 2);

  // Kelvin should have UDTFs.
  auto kelvin_udtfs = kelvin_plan->FindNodesOfType(IRNodeType::kUDTFSource);
  ASSERT_EQ(kelvin_udtfs.size(), 1);
  UDTFSourceIR* udtf = static_cast<UDTFSourceIR*>(kelvin_udtfs[0]);
  ASSERT_EQ(udtf->Children().size(), 1);
  EXPECT_MATCH(udtf->Children()[0], ExternalGRPCSink());
}

constexpr char kQueryJoinKelvinOnlyUDTFWithPEMOnlyUDTF[] = R"pxl(
import px

kelvin_df = px.ServiceUpTime()
pem_df =  px.OpenNetworkConnections('$0')
pem_df.service = 'blah_service'

px.display(kelvin_df.merge(pem_df, how='inner',left_on=['service'], right_on=['service'], suffixes=['', '_x']))
)pxl";
TEST_F(DistributedPlannerUDTFTests, UDTFOnKelvinJoinWithUDTFOnPEM) {
  md::UPID upid(123, 456, 789);
  std::string upid_as_str =
      sole::rebuild(absl::Uint128High64(upid.value()), absl::Uint128Low64(upid.value())).str();
  auto physical_plan =
      PlanQuery(absl::Substitute(kQueryJoinKelvinOnlyUDTFWithPEMOnlyUDTF, upid_as_str));

  // Find the appropriate agents.
  CarnotInstance* pem_instance = nullptr;
  CarnotInstance* kelvin_instance = nullptr;
  for (int64_t plan_id : physical_plan->dag().nodes()) {
    auto instance = physical_plan->Get(plan_id);
    if (IsPEM(instance->carnot_info())) {
      if (instance->carnot_info().query_broker_address() == "pem1") {
        pem_instance = instance;
      }
    } else {
      kelvin_instance = instance;
    }
  }
  ASSERT_NE(kelvin_instance, nullptr);
  ASSERT_NE(pem_instance, nullptr);

  // Make sure that the asid of this matches the UDTF specified one before we run it.
  ASSERT_EQ(pem_instance->carnot_info().query_broker_address(), "pem1");
  ASSERT_EQ(pem_instance->carnot_info().asid(), upid.asid());
  ASSERT_EQ(kelvin_instance->carnot_info().query_broker_address(), "kelvin");

  auto kelvin_plan = kelvin_instance->plan();
  auto pem_plan = pem_instance->plan();

  // Planner should:
  // 1. Remove the PEMS_SUBSET UDTFSource from Kelvin
  // 2. Remove the PEMS_SUBSET UDTFSource from Agent
  ASSERT_EQ(physical_plan->dag().nodes().size(), 2UL);

  // Kelvin should have UDTF.
  auto kelvin_udtfs = kelvin_plan->FindNodesOfType(IRNodeType::kUDTFSource);
  ASSERT_EQ(kelvin_udtfs.size(), 1);
  UDTFSourceIR* kelvin_udtf = static_cast<UDTFSourceIR*>(kelvin_udtfs[0]);
  EXPECT_EQ(kelvin_udtf->udtf_spec().name(), "ServiceUpTime");
  ASSERT_EQ(kelvin_udtf->Children().size(), 1);
  EXPECT_MATCH(kelvin_udtf->Children()[0], Join());

  // PEM should have UDTF.
  auto pem_udtfs = pem_plan->FindNodesOfType(IRNodeType::kUDTFSource);
  ASSERT_EQ(pem_udtfs.size(), 1);
  UDTFSourceIR* pem_udtf = static_cast<UDTFSourceIR*>(pem_udtfs[0]);
  EXPECT_EQ(pem_udtf->udtf_spec().name(), "OpenNetworkConnections");
  ASSERT_EQ(pem_udtf->Children().size(), 1);
  auto pem_udtf_child = pem_udtf->Children()[0];
  EXPECT_EQ(pem_udtf_child->type(), IRNodeType::kMap);
  // Now check the children's children
  ASSERT_EQ(pem_udtf_child->Children().size(), 1);
  auto pem_udtf_child_child = pem_udtf_child->Children()[0];
  EXPECT_EQ(pem_udtf_child_child->type(), IRNodeType::kGRPCSink);
  auto pem_grpc_sink = static_cast<GRPCSinkIR*>(pem_udtf_child_child);

  auto kelvin_result_sinks = kelvin_plan->FindNodesThatMatch(ExternalGRPCSink());
  auto new_result_sink = static_cast<GRPCSinkIR*>(kelvin_result_sinks[0]);
  ASSERT_EQ(new_result_sink->parents().size(), 1UL);
  EXPECT_EQ(new_result_sink->parents()[0]->type(), IRNodeType::kJoin);
  auto join = static_cast<JoinIR*>(new_result_sink->parents()[0]);
  ASSERT_EQ(join->parents().size(), 2);
  ASSERT_MATCH(join->parents()[0], UDTFSource());
  ASSERT_MATCH(join->parents()[1], GRPCSource());
  auto pem_grpc_source = static_cast<GRPCSourceIR*>(join->parents()[1]);
  EXPECT_EQ(pem_grpc_source->id(),
            pem_grpc_sink->agent_id_to_destination_id().find(pem_instance->id())->second);
}

constexpr char kInitArgQuery[] = R"pxl(
import px
df = px.DataFrame('http_events')
df.match = px.regex_match('regex_pattern', df.req_body)
px.display(df)
)pxl";

TEST_F(DistributedRulesTest, init_args) {
  compiler::Compiler compiler;
  auto plan_or_s = compiler.CompileToIR(kInitArgQuery, compiler_state_.get());
  ASSERT_OK(plan_or_s);
  auto single_node_plan = plan_or_s.ConsumeValueOrDie();

  auto distributed_planner = distributed::DistributedPlanner::Create().ConsumeValueOrDie();
  auto distributed_plan_or_s = distributed_planner->Plan(
      logical_state_.distributed_state(), compiler_state_.get(), single_node_plan.get());
  EXPECT_OK(distributed_plan_or_s);
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
