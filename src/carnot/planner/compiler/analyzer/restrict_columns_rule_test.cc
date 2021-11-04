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

#include "src/carnot/planner/compiler/analyzer/resolve_types_rule.h"
#include "src/carnot/planner/compiler/analyzer/restrict_columns_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using table_store::schema::Relation;
using ::testing::_;
using ::testing::ElementsAre;

TEST_F(RulesTest, redact_column_using_map) {
  MemorySourceIR* mem_src =
      graph->CreateNode<MemorySourceIR>(ast, "sensitive_table", std::vector<std::string>{})
          .ConsumeValueOrDie();

  MemorySinkIR* sink = MakeMemSink(mem_src, "sink");
  auto sensitive_table = table_store::schema::Relation(
      std::vector<types::DataType>({types::DataType::INT64, types::DataType::STRING}),
      std::vector<std::string>({"id", "sensitive_data"}));
  compiler_state_->relation_map()->emplace("sensitive_table", sensitive_table);
  compiler_state_->table_names_to_sensitive_columns()->emplace(
      "sensitive_table", absl::flat_hash_set<std::string>{"sensitive_data"});
  RedactionOptions options;
  options.use_full_redaction = true;
  compiler_state_->set_redaction_options(options);
  EXPECT_THAT(graph->dag().TopologicalSort(), ElementsAre(0, 1));

  // ResolveTypes first.
  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  // Now apply the rule.
  RestrictColumnsRule rule(compiler_state_.get());
  auto status = rule.Execute(graph.get());
  ASSERT_OK(status);
  EXPECT_TRUE(status.ValueOrDie());

  ASSERT_MATCH(sink->parents()[0], Map());

  auto map = static_cast<MapIR*>(sink->parents()[0]);
  EXPECT_MATCH(map->col_exprs()[0].node, ColumnNode("id"));
  EXPECT_MATCH(map->col_exprs()[1].node, String("REDACTED"));

  EXPECT_TRUE(map->is_type_resolved());
}

TEST_F(RulesTest, dont_add_map_if_not_necessary) {
  // This time we don't add the sensitive_data to the set of sensitive tables.
  MemorySourceIR* mem_src =
      graph->CreateNode<MemorySourceIR>(ast, "sensitive_table", std::vector<std::string>{})
          .ConsumeValueOrDie();
  auto sensitive_table = table_store::schema::Relation(
      std::vector<types::DataType>({types::DataType::INT64, types::DataType::STRING}),
      std::vector<std::string>({"id", "sensitive_data"}));
  compiler_state_->relation_map()->emplace("sensitive_table", sensitive_table);
  RedactionOptions options;
  options.use_full_redaction = true;
  compiler_state_->set_redaction_options(options);
  MemorySinkIR* sink = MakeMemSink(mem_src, "sink");
  EXPECT_THAT(graph->dag().TopologicalSort(), ElementsAre(0, 1));

  // ResolveTypes first.
  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  // Now apply the rule.
  RestrictColumnsRule rule(compiler_state_.get());
  auto status = rule.Execute(graph.get());
  ASSERT_OK(status);
  EXPECT_FALSE(status.ValueOrDie());

  ASSERT_MATCH(sink->parents()[0], MemorySource());
}

TEST_F(RulesTest, redact_column_using_px_redact_pii_best_effort) {
  MemorySourceIR* mem_src =
      graph->CreateNode<MemorySourceIR>(ast, "sensitive_table", std::vector<std::string>{})
          .ConsumeValueOrDie();

  MemorySinkIR* sink = MakeMemSink(mem_src, "sink");
  auto sensitive_table = table_store::schema::Relation(
      std::vector<types::DataType>({types::DataType::INT64, types::DataType::STRING}),
      std::vector<std::string>({"id", "sensitive_data"}));
  compiler_state_->relation_map()->emplace("sensitive_table", sensitive_table);
  compiler_state_->table_names_to_sensitive_columns()->emplace(
      "sensitive_table", absl::flat_hash_set<std::string>{"sensitive_data"});
  RedactionOptions options;
  options.use_px_redact_pii_best_effort = true;
  compiler_state_->set_redaction_options(options);
  EXPECT_THAT(graph->dag().TopologicalSort(), ElementsAre(0, 1));

  // ResolveTypes first.
  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  // Now apply the rule.
  RestrictColumnsRule rule(compiler_state_.get());
  auto status = rule.Execute(graph.get());
  ASSERT_OK(status);
  EXPECT_TRUE(status.ValueOrDie());

  ASSERT_MATCH(sink->parents()[0], Map());

  auto map = static_cast<MapIR*>(sink->parents()[0]);
  EXPECT_MATCH(map->col_exprs()[0].node, ColumnNode("id"));
  EXPECT_MATCH(map->col_exprs()[1].node,
               Func("redact_pii_best_effort", ColumnNode("sensitive_data")));

  EXPECT_TRUE(map->is_type_resolved());
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
