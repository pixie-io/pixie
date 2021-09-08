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
#include <gtest/gtest.h>

#include "src/carnot/planner/compiler/analyzer/resolve_types_rule.h"
#include "src/carnot/planner/distributed/splitter/presplit_optimizer/presplit_optimizer.h"
#include "src/carnot/planner/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

using compiler::ResolveTypesRule;
using table_store::schema::Relation;
using table_store::schemapb::Schema;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Return;
using testutils::DistributedRulesTest;

using PreSplitOptimizerTest = DistributedRulesTest;
TEST_F(PreSplitOptimizerTest, limit_pushdown) {
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});

  MemorySourceIR* src = MakeMemSource("source", relation);
  compiler_state_->relation_map()->emplace("source", relation);
  MapIR* map1 = MakeMap(src, {{"abc", MakeColumn("abc", 0)}}, false);
  MapIR* map2 = MakeMap(map1, {{"def", MakeColumn("abc", 0)}}, false);
  LimitIR* limit = MakeLimit(map2, 10);
  MemorySinkIR* sink = MakeMemSink(limit, "foo", {});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto optimizer = PreSplitOptimizer::Create(compiler_state_.get()).ConsumeValueOrDie();
  ASSERT_OK(optimizer->Execute(graph.get()));

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

TEST_F(PreSplitOptimizerTest, filter_pushdown) {
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource("source", relation);
  compiler_state_->relation_map()->emplace("source", relation);
  MapIR* map =
      MakeMap(src, {{"abc_1", MakeColumn("abc", 0)}, {"abc", MakeColumn("abc", 0)}}, false);
  auto col = MakeColumn("abc", 0);
  auto eq_func = MakeEqualsFunc(col, MakeInt(2));
  FilterIR* filter = MakeFilter(map, eq_func);
  MemorySinkIR* sink = MakeMemSink(filter, "foo", {});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto optimizer = PreSplitOptimizer::Create(compiler_state_.get()).ConsumeValueOrDie();
  ASSERT_OK(optimizer->Execute(graph.get()));

  EXPECT_THAT(sink->parents(), ElementsAre(map));
  EXPECT_THAT(map->parents(), ElementsAre(filter));
  EXPECT_THAT(filter->parents(), ElementsAre(src));
  EXPECT_MATCH(filter->filter_expr(), Equals(ColumnNode("abc"), Int(2)));
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
