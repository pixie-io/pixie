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

#include <gtest/gtest.h>

#include "src/carnot/planner/compiler/analyzer/analyzer.h"
#include "src/carnot/planner/compiler/optimizer/prune_unused_contains_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using table_store::schema::Relation;

using PruneUnusedContainsRuleTest = RulesTest;

TEST_F(PruneUnusedContainsRuleTest, Basic) {
  MemorySourceIR* mem_src = MakeMemSource(MakeRelation());
  compiler_state_->relation_map()->emplace("table", MakeRelation());

  auto constant1 = graph->CreateNode<StringIR>(ast, "testing 1").ValueOrDie();
  auto constant2 = graph->CreateNode<StringIR>(ast, "testing 2").ValueOrDie();
  auto empty_str = graph->CreateNode<StringIR>(ast, "").ValueOrDie();

  auto filter_func_1 =
      graph
          ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "contains"},
                               std::vector<ExpressionIR*>{constant1, empty_str})
          .ValueOrDie();

  auto filter_func_2 =
      graph
          ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "contains"},
                               std::vector<ExpressionIR*>{constant2, empty_str})
          .ValueOrDie();

  auto filter_ir_1 = graph->CreateNode<FilterIR>(ast, mem_src, filter_func_1).ValueOrDie();
  auto filter_ir_2 = graph->CreateNode<FilterIR>(ast, filter_ir_1, filter_func_2).ValueOrDie();

  auto filter_ir_1_id = filter_ir_1->id();
  auto filter_ir_2_id = filter_ir_2->id();
  auto filter_func_ir_1_id = filter_func_1->id();
  auto filter_func_ir_2_id = filter_func_2->id();

  auto limit = MakeLimit(static_cast<OperatorIR*>(filter_ir_2), 1000);

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  PruneUnusedContainsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_TRUE(result.ConsumeValueOrDie());

  EXPECT_FALSE(graph->HasNode(filter_ir_1_id));
  EXPECT_FALSE(graph->HasNode(filter_ir_2_id));
  EXPECT_FALSE(graph->HasNode(filter_func_ir_1_id));
  EXPECT_FALSE(graph->HasNode(filter_func_ir_2_id));
  EXPECT_TRUE(graph->HasNode(limit->id()));
}

TEST_F(PruneUnusedContainsRuleTest, IgnoresNonEmptyStringContains) {
  MemorySourceIR* mem_src = MakeMemSource(MakeRelation());
  compiler_state_->relation_map()->emplace("table", MakeRelation());

  auto constant1 = graph->CreateNode<StringIR>(ast, "testing").ValueOrDie();
  auto constant2 = graph->CreateNode<StringIR>(ast, "existing value").ValueOrDie();

  auto filter_func =
      graph
          ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "contains"},
                               std::vector<ExpressionIR*>{constant1, constant2})
          .ValueOrDie();
  auto filter_func_id = filter_func->id();

  auto filter_ir = graph->CreateNode<FilterIR>(ast, mem_src, filter_func).ValueOrDie();
  auto filter_ir_id = filter_ir->id();

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  PruneUnusedContainsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_FALSE(result.ConsumeValueOrDie());

  EXPECT_TRUE(graph->HasNode(filter_ir_id));
  EXPECT_TRUE(graph->HasNode(filter_func_id));
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
