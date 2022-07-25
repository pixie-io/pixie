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

#include <memory>

#include <gtest/gtest.h>
#include <vector>

#include "src/carnot/planner/compiler/analyzer/add_limit_to_batch_result_sink_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using ::testing::ElementsAre;

using AddLimitToBatchResultSinkRuleTest = RulesTest;

TEST_F(AddLimitToBatchResultSinkRuleTest, basic) {
  MemorySourceIR* src = MakeMemSource(MakeRelation());
  GRPCSinkIR* sink = MakeGRPCSink(src, "foo", {});

  auto compiler_state = std::make_unique<CompilerState>(
      std::make_unique<RelationMap>(), SensitiveColumnMap{}, info_.get(), time_now, 1000,
      "result_addr", "result_ssl_targetname", RedactionOptions{}, nullptr, nullptr,
      planner::DebugInfo{});

  AddLimitToBatchResultSinkRule rule(compiler_state.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_EQ(3, graph->FindNodesThatMatch(Operator()).size());
  auto limit_nodes = graph->FindNodesOfType(IRNodeType::kLimit);
  EXPECT_EQ(1, limit_nodes.size());

  auto limit = static_cast<LimitIR*>(limit_nodes[0]);
  EXPECT_TRUE(limit->limit_value_set());
  EXPECT_EQ(1000, limit->limit_value());
  EXPECT_THAT(sink->parents(), ElementsAre(limit));
  EXPECT_THAT(limit->parents(), ElementsAre(src));
}

TEST_F(AddLimitToBatchResultSinkRuleTest, overwrite_higher) {
  MemorySourceIR* src = MakeMemSource(MakeRelation());
  auto limit = graph->CreateNode<LimitIR>(ast, src, 1001).ValueOrDie();
  MakeMemSink(limit, "foo", {});

  auto compiler_state = std::make_unique<CompilerState>(
      std::make_unique<RelationMap>(), SensitiveColumnMap{}, info_.get(), time_now, 1000,
      "result_addr", "result_ssl_targetname", RedactionOptions{}, nullptr, nullptr,
      planner::DebugInfo{});

  AddLimitToBatchResultSinkRule rule(compiler_state.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_EQ(3, graph->FindNodesThatMatch(Operator()).size());
  auto limit_nodes = graph->FindNodesOfType(IRNodeType::kLimit);
  EXPECT_EQ(1, limit_nodes.size());
  EXPECT_EQ(1000, limit->limit_value());
}

TEST_F(AddLimitToBatchResultSinkRuleTest, dont_overwrite_lower) {
  MemorySourceIR* src = MakeMemSource(MakeRelation());
  auto limit = graph->CreateNode<LimitIR>(ast, src, 999).ValueOrDie();
  MakeMemSink(limit, "foo", {});

  auto compiler_state = std::make_unique<CompilerState>(
      std::make_unique<RelationMap>(), SensitiveColumnMap{}, info_.get(), time_now, 1000,
      "result_addr", "result_ssl_targetname", RedactionOptions{}, nullptr, nullptr,
      planner::DebugInfo{});

  AddLimitToBatchResultSinkRule rule(compiler_state.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
}

TEST_F(AddLimitToBatchResultSinkRuleTest, skip_if_no_limit) {
  MemorySourceIR* src = MakeMemSource(MakeRelation());
  MakeMemSink(src, "foo", {});

  auto compiler_state = std::make_unique<CompilerState>(
      std::make_unique<RelationMap>(), SensitiveColumnMap{}, info_.get(), time_now, 0,
      "result_addr", "result_ssl_targetname", RedactionOptions{}, nullptr, nullptr,
      planner::DebugInfo{});

  AddLimitToBatchResultSinkRule rule(compiler_state.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
}

TEST_F(AddLimitToBatchResultSinkRuleTest, skip_if_streaming) {
  MemorySourceIR* src = MakeMemSource(MakeRelation());
  src->set_streaming(true);
  FilterIR* filter = MakeFilter(src);
  MakeMemSink(filter, "foo", {});

  auto compiler_state = std::make_unique<CompilerState>(
      std::make_unique<RelationMap>(), SensitiveColumnMap{}, info_.get(), time_now, 1000,
      "result_addr", "result_ssl_targetname", RedactionOptions{}, nullptr, nullptr,
      planner::DebugInfo{});

  AddLimitToBatchResultSinkRule rule(compiler_state.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
}
}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
