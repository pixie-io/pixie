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

#include "src/carnot/planner/compiler/analyzer/resolve_types_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/splitter/presplit_optimizer/limit_push_down_rule.h"
#include "src/carnot/planner/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

using compiler::ResolveTypesRule;
using ::testing::ElementsAre;

using LimitPushdownRuleTest = testutils::DistributedRulesTest;
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

  MemorySourceIR* src = MakeMemSource("source", relation);
  compiler_state_->relation_map()->emplace("source", relation);
  MapIR* map1 = MakeMap(src, {{"abc", MakeColumn("abc", 0)}}, false);
  MapIR* map2 = MakeMap(map1, {{"def", MakeColumn("abc", 0)}}, false);
  LimitIR* limit = MakeLimit(map2, 10);
  MemorySinkIR* sink = MakeMemSink(limit, "foo", {});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

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
  EXPECT_THAT(*new_limit->resolved_table_type(), IsTableType(relation));
}

TEST_F(LimitPushdownRuleTest, pem_only) {
  Relation relation1({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  Relation relation2({types::DataType::INT64}, {"abc"});

  MemorySourceIR* src = MakeMemSource("source", relation1);
  compiler_state_->relation_map()->emplace("source", relation1);
  MapIR* map1 = MakeMap(src, {{"abc", MakeFunc("pem_only", {})}}, false);
  MapIR* map2 = MakeMap(map1, {{"def", MakeColumn("abc", 0)}}, false);
  LimitIR* limit = MakeLimit(map2, 10);
  MemorySinkIR* sink = MakeMemSink(limit, "foo", {});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

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
  EXPECT_TRUE(new_limit->resolved_table_type()->Equals(map1->resolved_table_type()));
}

TEST_F(LimitPushdownRuleTest, multi_branch_union) {
  Relation relation1({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  Relation relation2({types::DataType::INT64}, {"abc"});

  MemorySourceIR* src1 = MakeMemSource("source1", relation1);
  compiler_state_->relation_map()->emplace("source1", relation1);
  MapIR* map1 = MakeMap(src1, {{"abc", MakeColumn("abc", 0)}}, false);

  MemorySourceIR* src2 = MakeMemSource("source2", relation2);
  compiler_state_->relation_map()->emplace("source2", relation2);
  MapIR* map2 = MakeMap(src2, {{"abc", MakeColumn("abc", 0)}}, false);

  UnionIR* union_node = MakeUnion({map1, map2});
  MapIR* map3 = MakeMap(union_node, {{"abc", MakeColumn("abc", 0)}}, false);

  LimitIR* limit = MakeLimit(map3, 10);
  MemorySinkIR* sink = MakeMemSink(limit, "foo", {});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

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
  EXPECT_THAT(*new_limit->resolved_table_type(), IsTableType(relation2));

  EXPECT_THAT(union_node->parents(), ElementsAre(map1, map2));

  EXPECT_EQ(1, map1->parents().size());
  auto new_limit1 = map1->parents()[0];
  EXPECT_MATCH(new_limit1, Limit());
  EXPECT_EQ(10, static_cast<LimitIR*>(new_limit1)->limit_value());
  EXPECT_THAT(new_limit1->parents(), ElementsAre(src1));
  // Ensure the limit inherits the relation from its parent, not the previous location.
  EXPECT_THAT(*new_limit1->resolved_table_type(), IsTableType(relation1));

  EXPECT_EQ(1, map2->parents().size());
  auto new_limit2 = map2->parents()[0];
  EXPECT_MATCH(new_limit2, Limit());
  EXPECT_EQ(10, static_cast<LimitIR*>(new_limit2)->limit_value());
  EXPECT_THAT(new_limit2->parents(), ElementsAre(src2));
  // Ensure the limit inherits the relation from its parent, not the previous location.
  EXPECT_THAT(*new_limit2->resolved_table_type(), IsTableType(relation2));
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
