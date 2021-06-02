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

#include <gtest/gtest.h>

#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/annotate_abortable_sources_for_limits_rule.h"
#include "src/carnot/planner/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

using AnnotateAbortableSourcesForLimitsRuleTest = testutils::DistributedRulesTest;
TEST_F(AnnotateAbortableSourcesForLimitsRuleTest, SourceLimitSink) {
  auto mem_src = MakeMemSource("http_events");
  auto limit = MakeLimit(mem_src, 10);
  MakeMemSink(limit, "output");

  AnnotateAbortableSourcesForLimitsRule rule;

  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_TRUE(rule_or_s.ConsumeValueOrDie());

  // Limit should have mem_src as abortable source.
  EXPECT_THAT(limit->abortable_srcs(), ::testing::UnorderedElementsAre(mem_src->id()));
}

TEST_F(AnnotateAbortableSourcesForLimitsRuleTest, MultipleSourcesUnioned) {
  auto mem_src1 = MakeMemSource("http_events");
  auto mem_src2 = MakeMemSource("http_events");
  auto mem_src3 = MakeMemSource("http_events");
  auto union_node = MakeUnion({mem_src1, mem_src2, mem_src3});
  auto limit = MakeLimit(union_node, 10);
  MakeMemSink(limit, "output");

  AnnotateAbortableSourcesForLimitsRule rule;

  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_TRUE(rule_or_s.ConsumeValueOrDie());

  // Limit should have all mem_src as abortable sources.
  EXPECT_THAT(limit->abortable_srcs(),
              ::testing::UnorderedElementsAre(mem_src1->id(), mem_src2->id(), mem_src3->id()));
}

TEST_F(AnnotateAbortableSourcesForLimitsRuleTest, DisjointGraphs) {
  auto mem_src1 = MakeMemSource("http_events");
  auto limit1 = MakeLimit(mem_src1, 10);
  MakeMemSink(limit1, "output1");

  auto mem_src2 = MakeMemSource("http_events");
  auto limit2 = MakeLimit(mem_src2, 10);
  MakeMemSink(limit2, "output2");

  AnnotateAbortableSourcesForLimitsRule rule;

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
}  // namespace px
