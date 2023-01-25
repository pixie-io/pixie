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

#include "src/carnot/planner/compiler/analyzer/verify_filter_expression_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

class VerifyFilterExpressionTest : public RulesTest {
 protected:
  void SetUp() override {
    RulesTest::SetUp();
    mem_src =
        graph->CreateNode<MemorySourceIR>(ast, "source", std::vector<std::string>{}).ValueOrDie();
  }
  FuncIR* MakeFilter() {
    auto constant1 = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
    auto constant2 = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();

    filter_func = graph
                      ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::eq, "==", "equals"},
                                           std::vector<ExpressionIR*>{constant1, constant2})
                      .ValueOrDie();
    PX_CHECK_OK(graph->CreateNode<FilterIR>(ast, mem_src, filter_func));
    return filter_func;
  }
  MemorySourceIR* mem_src;
  FuncIR* filter_func;
};

TEST_F(VerifyFilterExpressionTest, basic_test) {
  FuncIR* filter_func = MakeFilter();
  EXPECT_OK(
      filter_func->SetResolvedType(ValueType::Create(types::DataType::BOOLEAN, types::ST_NONE)));
  VerifyFilterExpressionRule rule(compiler_state_.get());
  auto status_or = rule.Execute(graph.get());
  EXPECT_OK(status_or);
  EXPECT_FALSE(status_or.ValueOrDie());
}

TEST_F(VerifyFilterExpressionTest, wrong_filter_func_type) {
  FuncIR* filter_func = MakeFilter();
  EXPECT_OK(
      filter_func->SetResolvedType(ValueType::Create(types::DataType::INT64, types::ST_NONE)));
  VerifyFilterExpressionRule rule(compiler_state_.get());
  auto status_or = rule.Execute(graph.get());
  EXPECT_NOT_OK(status_or);
}

TEST_F(VerifyFilterExpressionTest, filter_func_not_set) {
  FuncIR* filter_func = MakeFilter();
  EXPECT_EQ(filter_func->EvaluatedDataType(), types::DataType::DATA_TYPE_UNKNOWN);
  VerifyFilterExpressionRule rule(compiler_state_.get());
  auto status_or = rule.Execute(graph.get());
  EXPECT_NOT_OK(status_or);
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
