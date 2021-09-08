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
#include "src/carnot/planner/distributed/splitter/presplit_analyzer/presplit_analyzer.h"
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

using PreSplitAnalyzerTest = DistributedRulesTest;
TEST_F(PreSplitAnalyzerTest, split_pem_udf) {
  // Kelvin-only plan
  MemorySourceIR* src1 = MakeMemSource("http_events");
  auto input1 = MakeColumn("remote_addr", 0);
  auto input2 = MakeColumn("req_path", 0);
  auto func1 = MakeFunc("pem_only", {input1});
  auto func2 = MakeFunc("kelvin_only", {input2});
  MapIR* map1 = MakeMap(src1, {{"pem", func1}, {"kelvin", func2}});
  MemorySinkIR* sink = MakeMemSink(map1, "foo", {});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  Relation existing_map_relation({types::STRING, types::STRING}, {"pem", "kelvin"});
  EXPECT_THAT(*map1->resolved_table_type(), IsTableType(existing_map_relation));

  auto analyzer = PreSplitAnalyzer::Create(compiler_state_.get()).ConsumeValueOrDie();
  ASSERT_OK(analyzer->Execute(graph.get()));

  ASSERT_EQ(1, src1->Children().size());
  EXPECT_NE(src1->Children()[0], map1);
  EXPECT_MATCH(src1->Children()[0], Map());
  auto new_map = static_cast<MapIR*>(src1->Children()[0]);
  Relation expected_map_relation({types::STRING, types::STRING}, {"pem_only_0", "req_path"});
  EXPECT_THAT(*new_map->resolved_table_type(), IsTableType(expected_map_relation));
  EXPECT_THAT(new_map->parents(), ElementsAre(src1));
  EXPECT_THAT(new_map->Children(), ElementsAre(map1));

  // original map relation and children shouldn't have changed.
  // pem_only func should now be a column projection.
  EXPECT_THAT(*map1->resolved_table_type(), IsTableType(existing_map_relation));
  EXPECT_EQ(2, map1->col_exprs().size());
  EXPECT_EQ("pem", map1->col_exprs()[0].name);
  EXPECT_MATCH(map1->col_exprs()[0].node, ColumnNode("pem_only_0"));
  EXPECT_EQ("kelvin", map1->col_exprs()[1].name);
  EXPECT_MATCH(map1->col_exprs()[1].node,
               FuncNameAllArgsMatch("kelvin_only", ColumnNode("req_path")));
  EXPECT_THAT(map1->parents(), ElementsAre(new_map));
  EXPECT_THAT(map1->Children(), ElementsAre(sink));
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
