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

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/coordinator/plan_clusters.h"
#include "src/carnot/planner/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {
using PlanClustersTest = testutils::DistributedRulesTest;
using ::testing::UnorderedElementsAre;

TEST_F(PlanClustersTest, create_plan) {
  // Setup the logical plan.
  auto logical_plan = CompileSingleNodePlan(testutils::kDependentRemovableOpsQuery);
  auto split_plan = SplitPlan(logical_plan.get());
  auto pem_plan = split_plan->before_blocking.get();

  auto mem_srcs = pem_plan->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(mem_srcs.size(), 1);
  MemorySourceIR* mem_src = static_cast<MemorySourceIR*>(mem_srcs[0]);

  auto filters = pem_plan->FindNodesThatMatch(Filter());
  ASSERT_EQ(filters.size(), 1);
  FilterIR* filter = static_cast<FilterIR*>(filters[0]);

  // Delete 1.
  PlanCluster filter_cluster({1, 2, 4}, {filter});
  ASSERT_OK_AND_ASSIGN(auto filter_only_plan, filter_cluster.CreatePlan(pem_plan));
  EXPECT_TRUE(filter_only_plan->FindNodesThatMatch(Operator()).empty());

  // Delete both.
  PlanCluster both_cluster({4}, {mem_src, filter});
  ASSERT_OK_AND_ASSIGN(auto both_plan, both_cluster.CreatePlan(pem_plan));
  EXPECT_TRUE(both_plan->FindNodesThatMatch(Operator()).empty());

  // Delete none.
  PlanCluster none_cluster({3}, {});
  ASSERT_OK_AND_ASSIGN(auto none_plan, none_cluster.CreatePlan(pem_plan));
  // Should not delete any operators.
  EXPECT_EQ(none_plan->FindNodesThatMatch(Operator()).size(),
            pem_plan->FindNodesThatMatch(Operator()).size());
}

TEST_F(PlanClustersTest, cluster_operators) {
  auto mem_src = MakeMemSource();
  auto filter = MakeFilter(MakeMemSource(), MakeEqualsFunc(MakeColumn("cpu", 0), MakeInt(10)));

  OperatorToAgentSet op_to_agent_set;
  op_to_agent_set[filter] = {1, 2, 4};
  op_to_agent_set[mem_src] = {4};

  std::vector<PlanCluster> plan_clusters = ClusterOperators(op_to_agent_set);
  ASSERT_EQ(plan_clusters.size(), 2);

  PlanCluster* mem_src_and_filter_cluster = &plan_clusters[0];
  PlanCluster* filter_only_cluster = &plan_clusters[1];
  if (filter_only_cluster->ops_to_remove.contains(mem_src)) {
    auto tmp = mem_src_and_filter_cluster;
    mem_src_and_filter_cluster = filter_only_cluster;
    filter_only_cluster = tmp;
  }

  EXPECT_THAT(mem_src_and_filter_cluster->ops_to_remove, UnorderedElementsAre(filter, mem_src));
  EXPECT_THAT(filter_only_cluster->ops_to_remove, UnorderedElementsAre(filter));
  EXPECT_THAT(mem_src_and_filter_cluster->agent_set, UnorderedElementsAre(4));
  EXPECT_THAT(filter_only_cluster->agent_set, UnorderedElementsAre(1, 2));

  EXPECT_THAT(RemainingAgents(op_to_agent_set, {1, 2, 3, 4, 5}), UnorderedElementsAre(3, 5));
}

TEST_F(PlanClustersTest, cluster_operators_no_overlapping_sets) {
  auto mem_src = MakeMemSource();
  auto filter = MakeFilter(MakeMemSource(), MakeEqualsFunc(MakeColumn("cpu", 0), MakeInt(10)));

  OperatorToAgentSet op_to_agent_set;
  op_to_agent_set[filter] = {1, 2, 4};
  op_to_agent_set[mem_src] = {5, 6, 7};

  std::vector<PlanCluster> plan_clusters = ClusterOperators(op_to_agent_set);
  ASSERT_EQ(plan_clusters.size(), 2);

  PlanCluster* mem_src_cluster = &plan_clusters[0];
  PlanCluster* filter_cluster = &plan_clusters[1];
  if (filter_cluster->ops_to_remove.contains(mem_src)) {
    auto tmp = mem_src_cluster;
    mem_src_cluster = filter_cluster;
    filter_cluster = tmp;
  }

  EXPECT_THAT(mem_src_cluster->ops_to_remove, UnorderedElementsAre(mem_src));
  EXPECT_THAT(filter_cluster->ops_to_remove, UnorderedElementsAre(filter));
  EXPECT_THAT(mem_src_cluster->agent_set, UnorderedElementsAre(5, 6, 7));
  EXPECT_THAT(filter_cluster->agent_set, UnorderedElementsAre(1, 2, 4));

  EXPECT_THAT(RemainingAgents(op_to_agent_set, {1, 2, 3, 4, 5, 6, 7, 8}),
              UnorderedElementsAre(3, 8));
}

TEST_F(PlanClustersTest, cluster_operator_intersecting_set) {
  auto mem_src = MakeMemSource();
  auto filter = MakeFilter(MakeMemSource(), MakeEqualsFunc(MakeColumn("cpu", 0), MakeInt(10)));

  OperatorToAgentSet op_to_agent_set;
  op_to_agent_set[filter] = {1, 2, 4};
  op_to_agent_set[mem_src] = {4, 5};

  std::vector<PlanCluster> plan_clusters = ClusterOperators(op_to_agent_set);
  ASSERT_EQ(plan_clusters.size(), 3);

  const PlanCluster* mem_src_and_filter_cluster = &plan_clusters[0];
  const PlanCluster* filter_only_cluster = &plan_clusters[1];
  const PlanCluster* mem_src_only_cluster = &plan_clusters[2];
  for (const auto& cluster : plan_clusters) {
    if (cluster.ops_to_remove.size() == 2) {
      mem_src_and_filter_cluster = &cluster;
      continue;
    }
    if (cluster.ops_to_remove.contains(filter)) {
      filter_only_cluster = &cluster;
      continue;
    }
    if (cluster.ops_to_remove.contains(mem_src)) {
      mem_src_only_cluster = &cluster;
      continue;
    }
  }

  EXPECT_THAT(mem_src_and_filter_cluster->ops_to_remove, UnorderedElementsAre(filter, mem_src));
  EXPECT_THAT(filter_only_cluster->ops_to_remove, UnorderedElementsAre(filter));
  EXPECT_THAT(mem_src_only_cluster->ops_to_remove, UnorderedElementsAre(mem_src));

  EXPECT_THAT(mem_src_and_filter_cluster->agent_set, UnorderedElementsAre(4));
  EXPECT_THAT(filter_only_cluster->agent_set, UnorderedElementsAre(1, 2));
  EXPECT_THAT(mem_src_only_cluster->agent_set, UnorderedElementsAre(5));

  EXPECT_THAT(RemainingAgents(op_to_agent_set, {1, 2, 3, 4, 5}), UnorderedElementsAre(3));
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
