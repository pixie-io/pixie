#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/removable_ops_rule.h"
#include "src/carnot/planner/test_utils.h"

namespace pl {
namespace carnot {
namespace planner {
namespace distributed {
using md::AgentMetadataFilter;
using ::pl::testing::proto::EqualsProto;
using ::pl::testing::proto::Partially;
using ::testing::Contains;
using ::testing::ContainsRegex;
using ::testing::ElementsAre;
using ::testing::Key;
using ::testing::UnorderedElementsAre;
using testutils::kOnePEMOneKelvinDistributedState;
using testutils::kOnePEMThreeKelvinsDistributedState;
using testutils::kThreePEMsOneKelvinDistributedState;

using RemovableOpsRuleTest = testutils::DistributedRulesTest;

constexpr char kExtraPEM[] = R"carnotinfo(
query_broker_address: "pem5"
agent_id {
  data: "00000001-0000-0000-0000-000000000005"
}
has_grpc_server: false
has_data_store: true
processes_data: true
accepts_remote_sources: false
asid: 1111111
)carnotinfo";

TEST_F(RemovableOpsRuleTest, delete_dependent_nodes) {
  auto distributed_state = ThreeAgentOneKelvinStateWithMetadataInfo();
  // We add an extra PEM that doesn't have an entry in the schema table.
  EXPECT_TRUE(google::protobuf::TextFormat::MergeFromString(kExtraPEM,
                                                            distributed_state.add_carnot_info()));
  auto logical_plan = CompileSingleNodePlan(testutils::kDependentRemovableOpsQuery);
  auto distributed_plan = AssembleDistributedPlan(distributed_state);
  auto split_plan = SplitPlan(logical_plan.get());

  absl::flat_hash_set<int64_t> source_node_ids = SourceNodeIds(distributed_plan.get());

  ASSERT_OK_AND_ASSIGN(auto agent_schema_map,
                       LoadSchemaMap(distributed_state, distributed_plan->uuid_to_id_map()));

  ASSERT_OK_AND_ASSIGN(OperatorToAgentSet removable_ops_to_agents,
                       MapRemovableOperatorsRule::GetRemovableOperators(
                           distributed_plan.get(), agent_schema_map, source_node_ids,
                           split_plan->before_blocking.get()));

  EXPECT_EQ(removable_ops_to_agents.size(), 2);
  MemorySourceIR* mem_src = nullptr;
  FilterIR* filter = nullptr;
  for (const auto& [op, agents] : removable_ops_to_agents) {
    if (Match(op, MemorySource())) {
      mem_src = static_cast<MemorySourceIR*>(op);
    }
    if (Match(op, Filter())) {
      filter = static_cast<FilterIR*>(op);
    }
  }
  ASSERT_NE(mem_src, nullptr);
  // All except the last agent have the mem src.
  EXPECT_THAT(removable_ops_to_agents[mem_src], UnorderedElementsAre(4));

  ASSERT_NE(filter, nullptr);
  // Only one agent has the filter.
  EXPECT_THAT(removable_ops_to_agents[filter], UnorderedElementsAre(1, 2, 4));
}

constexpr char kKelvinOnlyUDTF[] = R"pxl(
import px

px.display(px.GetSchemas(), 't1')
)pxl";

TEST_F(RemovableOpsRuleTest, udtf_removed) {
  auto distributed_state = ThreeAgentOneKelvinStateWithMetadataInfo();
  auto logical_plan = CompileSingleNodePlan(kKelvinOnlyUDTF);
  auto distributed_plan = AssembleDistributedPlan(distributed_state);
  auto split_plan = SplitPlan(logical_plan.get());

  absl::flat_hash_set<int64_t> source_node_ids = SourceNodeIds(distributed_plan.get());

  ASSERT_OK_AND_ASSIGN(auto agent_schema_map,
                       LoadSchemaMap(distributed_state, distributed_plan->uuid_to_id_map()));

  ASSERT_OK_AND_ASSIGN(OperatorToAgentSet removable_ops_to_agents,
                       MapRemovableOperatorsRule::GetRemovableOperators(
                           distributed_plan.get(), agent_schema_map, source_node_ids,
                           split_plan->before_blocking.get()));

  EXPECT_EQ(removable_ops_to_agents.size(), 1);
  UDTFSourceIR* udtf_src = nullptr;
  for (const auto& [op, agents] : removable_ops_to_agents) {
    if (Match(op, UDTFSource())) {
      udtf_src = static_cast<UDTFSourceIR*>(op);
    }
  }
  ASSERT_NE(udtf_src, nullptr);
  // All except the last agent have the mem src.
  EXPECT_THAT(removable_ops_to_agents[udtf_src], UnorderedElementsAre(0, 1, 2));
}

constexpr char kRemoveFilterOrTwoAgents[] = R"pxl(
import px

# and (only agent 1)
t1 = px.DataFrame(table='http_events')
t1 = t1[t1.ctx['pod_id'] == 'agent1_pod' or t1.ctx['service_id'] == 'agent2_service']
px.display(t1, 't1')
)pxl";

constexpr char kRemoveFilterOrWithNonexistant[] = R"pxl(
import px

# and (only agent 1)
t1 = px.DataFrame(table='http_events')
t1 = t1[t1.ctx['pod_id'] == 'agent1_pod' or t1.ctx['pod_id'] == 'does_not_exist']
px.display(t1, 't1')
)pxl";
constexpr char kRemoveFilterHeavilyNested[] = R"pxl(
import px


# mixed (only agent 2)
t2 = px.DataFrame(table='http_events')
t2 = t2['agent2_service' == t2.ctx['service_id'] and ('agent2_service' == t2.ctx['service_id'] or 3 == 3)]
px.display(t2, 't2')

)pxl";

constexpr char kRemoveFilterConjunctionMatchNone[] = R"pxl(
import px

# mixed (passes none)
t3 = px.DataFrame(table='http_events')
t3 = t3['agent3_service' == t3.ctx['service_id'] and t3.ctx['pod_id'] == 'agent3_pod']
px.display(t3, 't3')

)pxl";

TEST_F(RemovableOpsRuleTest, filter_conjunctions_match_none) {
  auto distributed_state = ThreeAgentOneKelvinStateWithMetadataInfo();
  auto logical_plan = CompileSingleNodePlan(kRemoveFilterConjunctionMatchNone);
  auto distributed_plan = AssembleDistributedPlan(distributed_state);
  auto split_plan = SplitPlan(logical_plan.get());

  absl::flat_hash_set<int64_t> source_node_ids = SourceNodeIds(distributed_plan.get());

  ASSERT_OK_AND_ASSIGN(auto agent_schema_map,
                       LoadSchemaMap(distributed_state, distributed_plan->uuid_to_id_map()));

  ASSERT_OK_AND_ASSIGN(OperatorToAgentSet removable_ops_to_agents,
                       MapRemovableOperatorsRule::GetRemovableOperators(
                           distributed_plan.get(), agent_schema_map, source_node_ids,
                           split_plan->before_blocking.get()));

  EXPECT_EQ(removable_ops_to_agents.size(), 1);

  FilterIR* filter = nullptr;
  for (const auto& [op, agents] : removable_ops_to_agents) {
    ASSERT_MATCH(op, Filter());
    filter = static_cast<FilterIR*>(op);
  }
  ASSERT_NE(filter, nullptr);
  // All agents delete.
  EXPECT_THAT(removable_ops_to_agents[filter], UnorderedElementsAre(0, 1, 2));
}

TEST_F(RemovableOpsRuleTest, filter_conjunctions_nested) {
  auto distributed_state = ThreeAgentOneKelvinStateWithMetadataInfo();
  auto logical_plan = CompileSingleNodePlan(kRemoveFilterHeavilyNested);
  auto distributed_plan = AssembleDistributedPlan(distributed_state);
  auto split_plan = SplitPlan(logical_plan.get());

  absl::flat_hash_set<int64_t> source_node_ids = SourceNodeIds(distributed_plan.get());

  ASSERT_OK_AND_ASSIGN(auto agent_schema_map,
                       LoadSchemaMap(distributed_state, distributed_plan->uuid_to_id_map()));

  ASSERT_OK_AND_ASSIGN(OperatorToAgentSet removable_ops_to_agents,
                       MapRemovableOperatorsRule::GetRemovableOperators(
                           distributed_plan.get(), agent_schema_map, source_node_ids,
                           split_plan->before_blocking.get()));

  EXPECT_EQ(removable_ops_to_agents.size(), 1);

  FilterIR* filter = nullptr;
  for (const auto& [op, agents] : removable_ops_to_agents) {
    ASSERT_MATCH(op, Filter());
    filter = static_cast<FilterIR*>(op);
  }
  ASSERT_NE(filter, nullptr);
  // All agents delete.
  EXPECT_THAT(removable_ops_to_agents[filter], UnorderedElementsAre(0, 2));
}

TEST_F(RemovableOpsRuleTest, filter_conjunctions_across_two_agents) {
  auto distributed_state = ThreeAgentOneKelvinStateWithMetadataInfo();
  auto logical_plan = CompileSingleNodePlan(kRemoveFilterOrTwoAgents);
  auto distributed_plan = AssembleDistributedPlan(distributed_state);
  auto split_plan = SplitPlan(logical_plan.get());

  absl::flat_hash_set<int64_t> source_node_ids = SourceNodeIds(distributed_plan.get());

  ASSERT_OK_AND_ASSIGN(auto agent_schema_map,
                       LoadSchemaMap(distributed_state, distributed_plan->uuid_to_id_map()));

  ASSERT_OK_AND_ASSIGN(OperatorToAgentSet removable_ops_to_agents,
                       MapRemovableOperatorsRule::GetRemovableOperators(
                           distributed_plan.get(), agent_schema_map, source_node_ids,
                           split_plan->before_blocking.get()));

  EXPECT_EQ(removable_ops_to_agents.size(), 1);

  FilterIR* filter = nullptr;
  for (const auto& [op, agents] : removable_ops_to_agents) {
    ASSERT_MATCH(op, Filter());
    filter = static_cast<FilterIR*>(op);
  }
  ASSERT_NE(filter, nullptr);
  // All agents delete.
  EXPECT_THAT(removable_ops_to_agents[filter], UnorderedElementsAre(2));
}

TEST_F(RemovableOpsRuleTest, filter_or_with_nonexistant) {
  auto distributed_state = ThreeAgentOneKelvinStateWithMetadataInfo();
  auto logical_plan = CompileSingleNodePlan(kRemoveFilterOrWithNonexistant);
  auto distributed_plan = AssembleDistributedPlan(distributed_state);
  auto split_plan = SplitPlan(logical_plan.get());

  absl::flat_hash_set<int64_t> source_node_ids = SourceNodeIds(distributed_plan.get());

  ASSERT_OK_AND_ASSIGN(auto agent_schema_map,
                       LoadSchemaMap(distributed_state, distributed_plan->uuid_to_id_map()));

  ASSERT_OK_AND_ASSIGN(OperatorToAgentSet removable_ops_to_agents,
                       MapRemovableOperatorsRule::GetRemovableOperators(
                           distributed_plan.get(), agent_schema_map, source_node_ids,
                           split_plan->before_blocking.get()));

  EXPECT_EQ(removable_ops_to_agents.size(), 1);

  FilterIR* filter = nullptr;
  for (const auto& [op, agents] : removable_ops_to_agents) {
    ASSERT_MATCH(op, Filter());
    filter = static_cast<FilterIR*>(op);
  }
  ASSERT_NE(filter, nullptr);
  // All agents delete.
  EXPECT_THAT(removable_ops_to_agents[filter], UnorderedElementsAre(1, 2));
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
