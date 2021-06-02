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

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "src/carnot/planner/compiler/analyzer/source_relation_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

class SourceRelationTest : public RulesTest {
 protected:
  void SetUp() override { RulesTest::SetUp(); }
};

// Simple check with select all.
TEST_F(SourceRelationTest, set_source_select_all) {
  MemorySourceIR* mem_src =
      graph->CreateNode<MemorySourceIR>(ast, "cpu", std::vector<std::string>{}).ValueOrDie();
  EXPECT_FALSE(mem_src->IsRelationInit());

  SourceRelationRule source_relation_rule(compiler_state_.get());
  auto result = source_relation_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  EXPECT_TRUE(mem_src->IsRelationInit());
  // Make sure the relations are the same after processing.
  table_store::schema::Relation relation = mem_src->relation();
  EXPECT_TRUE(relation.col_types() == cpu_relation.col_types());
  EXPECT_TRUE(relation.col_names() == cpu_relation.col_names());
}

TEST_F(SourceRelationTest, set_source_variable_columns) {
  std::vector<std::string> str_columns = {"cpu1", "cpu2"};
  MemorySourceIR* mem_src = graph->CreateNode<MemorySourceIR>(ast, "cpu", str_columns).ValueOrDie();

  EXPECT_FALSE(mem_src->IsRelationInit());

  SourceRelationRule source_relation_rule(compiler_state_.get());
  auto result = source_relation_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  EXPECT_TRUE(mem_src->IsRelationInit());
  // Make sure the relations are the same after processing.
  table_store::schema::Relation relation = mem_src->relation();
}

TEST_F(SourceRelationTest, missing_table_name) {
  auto table_name = "tablename_22";
  MemorySourceIR* mem_src =
      graph->CreateNode<MemorySourceIR>(ast, table_name, std::vector<std::string>{}).ValueOrDie();
  EXPECT_FALSE(mem_src->IsRelationInit());

  EXPECT_FALSE(mem_src->IsRelationInit());

  SourceRelationRule source_relation_rule(compiler_state_.get());
  auto result = source_relation_rule.Execute(graph.get());
  EXPECT_NOT_OK(result);
  EXPECT_THAT(result.status(), HasCompilerError("Table '$0' not found.", table_name));
}

TEST_F(SourceRelationTest, missing_columns) {
  std::string missing_column = "blah_column";
  std::vector<std::string> str_columns = {"cpu1", "cpu2", missing_column};
  MemorySourceIR* mem_src = graph->CreateNode<MemorySourceIR>(ast, "cpu", str_columns).ValueOrDie();

  EXPECT_FALSE(mem_src->IsRelationInit());

  SourceRelationRule source_relation_rule(compiler_state_.get());
  auto result = source_relation_rule.Execute(graph.get());
  EXPECT_NOT_OK(result);
  VLOG(1) << result.status().ToString();

  EXPECT_THAT(result.status(),
              HasCompilerError("Columns \\{$0\\} are missing in table.", missing_column));
}

TEST_F(SourceRelationTest, UDTFDoesNothing) {
  udfspb::UDTFSourceSpec udtf_spec;
  Relation relation{{types::INT64, types::STRING}, {"fd", "name"}};
  ASSERT_OK(relation.ToProto(udtf_spec.mutable_relation()));

  auto udtf =
      graph
          ->CreateNode<UDTFSourceIR>(ast, "GetOpenNetworkConnections",
                                     absl::flat_hash_map<std::string, ExpressionIR*>{}, udtf_spec)
          .ConsumeValueOrDie();

  EXPECT_TRUE(udtf->IsRelationInit());

  SourceRelationRule source_relation_rule(compiler_state_.get());
  auto did_change_or_s = source_relation_rule.Execute(graph.get());
  ASSERT_OK(did_change_or_s);
  // Should not change.
  EXPECT_FALSE(did_change_or_s.ConsumeValueOrDie());
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
