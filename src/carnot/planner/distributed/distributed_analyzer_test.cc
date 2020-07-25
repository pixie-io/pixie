#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/analyzer.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/distributed_analyzer.h"
#include "src/carnot/planner/logical_planner.h"
#include "src/carnot/planner/parser/parser.h"
#include "src/carnot/planner/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"

namespace pl {
namespace carnot {
namespace planner {
namespace distributed {

using table_store::schema::Relation;
using testutils::CreateTwoPEMsOneKelvinPlannerState;
using testutils::DistributedRulesTest;
using testutils::kHttpEventsSchema;

using DistributedAnalyzerTest = DistributedRulesTest;
TEST_F(DistributedAnalyzerTest, UDTFOnlyOnPEMsDoesntRunOnKelvin) {
  uint32_t asid = 123;
  md::UPID upid(asid, 456, 3420030816657ULL);
  std::string upid_str =
      sole::rebuild(absl::Uint128High64(upid.value()), absl::Uint128Low64(upid.value())).str();
  auto physical_plan = PlanQuery(
      absl::Substitute("import px\npx.display(px.OpenNetworkConnections('$0'))", upid_str));
  // The plan starts with 3 agents -> 2 pems and 1 kelvin
  ASSERT_EQ(physical_plan->dag().nodes().size(), 3UL);
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
  ASSERT_EQ(pem_instance->carnot_info().asid(), asid);

  ASSERT_EQ(kelvin_instance->carnot_info().query_broker_address(), "kelvin");
  auto pem_plan = pem_instance->plan();
  auto kelvin_plan = kelvin_instance->plan();

  // Verify plan values before the analyzer.
  // Both PEM and Kelvin should have UDTF before.
  ASSERT_EQ(pem_plan->FindNodesOfType(IRNodeType::kUDTFSource).size(), 1);
  ASSERT_EQ(kelvin_plan->FindNodesOfType(IRNodeType::kUDTFSource).size(), 1);

  // Run the analyzer (what we are actually testing.)
  auto distributed_analyzer =
      DistributedAnalyzer::Create(logical_state_.distributed_state()).ConsumeValueOrDie();
  ASSERT_OK(distributed_analyzer->Execute(physical_plan.get()));

  // Analyzer should:
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
  EXPECT_EQ(new_udtf->Children()[0]->type(), IRNodeType::kGRPCSink);

  // Kelvin should not have UDTFs.
  auto kelvin_udtfs = kelvin_plan->FindNodesOfType(IRNodeType::kUDTFSource);
  ASSERT_EQ(kelvin_udtfs.size(), 0);
  // Kelvin should have two Operators: Memory Sink and GRPCSource
  EXPECT_EQ(kelvin_plan->FindNodesThatMatch(Operator()).size(), 2);
  auto kelvin_mem_sinks = kelvin_plan->FindNodesOfType(IRNodeType::kMemorySink);
  auto new_mem_sink = static_cast<MemorySinkIR*>(kelvin_mem_sinks[0]);
  ASSERT_EQ(new_mem_sink->parents().size(), 1UL);
  EXPECT_EQ(new_mem_sink->parents()[0]->type(), IRNodeType::kGRPCSource);
}

TEST_F(DistributedAnalyzerTest, UDTFOnKelvinOnlyOnKelvin) {
  auto physical_plan = PlanQuery("import px\npx.display(px.ServiceUpTime())");
  // The plan starts with 3 agents -> 2 pems and 1 kelvin
  ASSERT_EQ(physical_plan->dag().nodes().size(), 3UL);
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

  ASSERT_EQ(kelvin_instance->carnot_info().query_broker_address(), "kelvin");
  auto kelvin_plan = kelvin_instance->plan();
  auto pem_plan = pem_instance->plan();
  auto kelvin_instance_id = kelvin_instance->id();
  auto pem_instance_id = pem_instance->id();

  // Verify plan values before the analyzer.
  // Both PEM and Kelvin should have UDTF before.
  ASSERT_EQ(pem_plan->FindNodesOfType(IRNodeType::kUDTFSource).size(), 1);
  ASSERT_EQ(kelvin_plan->FindNodesOfType(IRNodeType::kUDTFSource).size(), 1);

  // Run the analyzer (what we are actually testing).
  auto distributed_analyzer =
      DistributedAnalyzer::Create(logical_state_.distributed_state()).ConsumeValueOrDie();
  ASSERT_OK(distributed_analyzer->Execute(physical_plan.get()));

  // Analyzer should:
  // 1. Remove one agent entirely (pem2) and keep pem1 and kelvin.
  // 2. Remove UDTFSources on Kelvin but keep the rest of the Kelvin plan.
  ASSERT_EQ(physical_plan->dag().nodes().size(), 1UL);
  // Does not have pem.
  ASSERT_FALSE(physical_plan->HasNode(pem_instance_id));
  // Has Kelvin.
  ASSERT_TRUE(physical_plan->HasNode(kelvin_instance_id));

  // TODO(philkuz) (PL-1468) update this when we combine GRPCSink and GRPCSource on the same plan.
  // Kelvin should have four Operators that form two subgraphs:
  // UDTFSource -> GRPCSink && GRPCSource -> MemSink
  EXPECT_EQ(kelvin_plan->FindNodesThatMatch(Operator()).size(), 4);

  // Kelvin should have UDTFs.
  auto kelvin_udtfs = kelvin_plan->FindNodesOfType(IRNodeType::kUDTFSource);
  ASSERT_EQ(kelvin_udtfs.size(), 1);
  UDTFSourceIR* udtf = static_cast<UDTFSourceIR*>(kelvin_udtfs[0]);
  ASSERT_EQ(udtf->Children().size(), 1);
  EXPECT_EQ(udtf->Children()[0]->type(), IRNodeType::kGRPCSink);

  auto kelvin_mem_sinks = kelvin_plan->FindNodesOfType(IRNodeType::kMemorySink);
  auto new_mem_sink = static_cast<MemorySinkIR*>(kelvin_mem_sinks[0]);
  ASSERT_EQ(new_mem_sink->parents().size(), 1UL);
  EXPECT_EQ(new_mem_sink->parents()[0]->type(), IRNodeType::kGRPCSource);
}

constexpr char kQueryJoinKelvinOnlyUDTFWithPEMOnlyUDTF[] = R"pxl(
import px

kelvin_df = px.ServiceUpTime()
pem_df =  px.OpenNetworkConnections('$0')
pem_df.service = 'blah_service'

px.display(kelvin_df.merge(pem_df, how='inner',left_on=['service'], right_on=['service'], suffixes=['', '_x']))
)pxl";
TEST_F(DistributedAnalyzerTest, UDTFOnKelvinJoinWithUDTFOnPEM) {
  md::UPID upid(123, 456, 789);
  std::string upid_as_str =
      sole::rebuild(absl::Uint128High64(upid.value()), absl::Uint128Low64(upid.value())).str();
  auto physical_plan =
      PlanQuery(absl::Substitute(kQueryJoinKelvinOnlyUDTFWithPEMOnlyUDTF, upid_as_str));
  // The plan starts with 3 agents -> 2 pems and 1 kelvin.
  ASSERT_EQ(physical_plan->dag().nodes().size(), 3UL);

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
  auto kelvin_instance_id = kelvin_instance->id();
  auto pem_instance_id = pem_instance->id();

  // Verify plan values before the analyzer.
  // Both PEM and Kelvin should have UDTF before.
  ASSERT_EQ(pem_plan->FindNodesOfType(IRNodeType::kUDTFSource).size(), 2);
  ASSERT_EQ(kelvin_plan->FindNodesOfType(IRNodeType::kUDTFSource).size(), 2);
  auto pem_grpc_sinks = pem_plan->FindNodesOfType(IRNodeType::kGRPCSink);
  auto kelvin_grpc_sinks = kelvin_plan->FindNodesOfType(IRNodeType::kGRPCSink);
  ASSERT_EQ(pem_grpc_sinks.size(), 2);
  ASSERT_EQ(kelvin_grpc_sinks.size(), 2);

  // Run the analyzer (what we are actually testing).
  auto distributed_analyzer =
      DistributedAnalyzer::Create(logical_state_.distributed_state()).ConsumeValueOrDie();
  ASSERT_OK(distributed_analyzer->Execute(physical_plan.get()));

  // Analyzer should:
  // 1. Remove the PEMS_SUBSET UDTFSource from Kelvin
  // 2. Remove the PEMS_SUBSET UDTFSource from Agent
  ASSERT_EQ(physical_plan->dag().nodes().size(), 2UL);
  // Has the PEM.
  ASSERT_TRUE(physical_plan->HasNode(pem_instance_id));
  // Has Kelvin.
  ASSERT_TRUE(physical_plan->HasNode(kelvin_instance_id));

  // Kelvin should have UDTF.
  auto kelvin_udtfs = kelvin_plan->FindNodesOfType(IRNodeType::kUDTFSource);
  ASSERT_EQ(kelvin_udtfs.size(), 1);
  UDTFSourceIR* kelvin_udtf = static_cast<UDTFSourceIR*>(kelvin_udtfs[0]);
  EXPECT_EQ(kelvin_udtf->udtf_spec().name(), "ServiceUpTime");
  ASSERT_EQ(kelvin_udtf->Children().size(), 1);
  EXPECT_EQ(kelvin_udtf->Children()[0]->type(), IRNodeType::kGRPCSink);
  auto kelvin_grpc_sink = static_cast<GRPCSinkIR*>(kelvin_udtf->Children()[0]);

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

  auto kelvin_mem_sinks = kelvin_plan->FindNodesOfType(IRNodeType::kMemorySink);
  auto new_mem_sink = static_cast<MemorySinkIR*>(kelvin_mem_sinks[0]);
  ASSERT_EQ(new_mem_sink->parents().size(), 1UL);
  EXPECT_EQ(new_mem_sink->parents()[0]->type(), IRNodeType::kJoin);
  auto join = static_cast<JoinIR*>(new_mem_sink->parents()[0]);
  ASSERT_EQ(join->parents().size(), 2);
  ASSERT_EQ(join->parents()[0]->type(), IRNodeType::kGRPCSource);
  ASSERT_EQ(join->parents()[1]->type(), IRNodeType::kGRPCSource);
  auto kelvin_grpc_source = static_cast<GRPCSourceIR*>(join->parents()[0]);
  auto pem_grpc_source = static_cast<GRPCSourceIR*>(join->parents()[1]);
  EXPECT_EQ(kelvin_grpc_source->id(), kelvin_grpc_sink->destination_id());
  EXPECT_EQ(pem_grpc_source->id(), pem_grpc_sink->destination_id());
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
