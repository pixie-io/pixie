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

#include "src/carnot/planner/compiler/analyzer/data_type_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using table_store::schema::Relation;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Not;
using ::testing::Return;
using ::testing::UnorderedElementsAreArray;

class DataTypeRuleTest : public RulesTest {
 protected:
  void SetUp() override {
    RulesTest::SetUp();
    mem_src =
        graph->CreateNode<MemorySourceIR>(ast, "source", std::vector<std::string>{}).ValueOrDie();
    PL_CHECK_OK(mem_src->SetRelation(cpu_relation));
  }
  MemorySourceIR* mem_src;
};

// Simple map function.
TEST_F(DataTypeRuleTest, map_function) {
  auto constant = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
  auto col = MakeColumn("count", /* parent_op_idx */ 0);
  auto func = graph
                  ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::add, "+", "add"},
                                       std::vector<ExpressionIR*>({constant, col}))
                  .ValueOrDie();
  EXPECT_OK(graph->CreateNode<MapIR>(ast, mem_src, ColExpressionVector{{"func", func}},
                                     /* keep_input_columns */ false));
  // No rule has been run, don't expect any of these to be evaluated.
  EXPECT_FALSE(func->IsDataTypeEvaluated());
  EXPECT_FALSE(col->IsDataTypeEvaluated());

  EXPECT_NOT_MATCH(func, UnresolvedRTFuncMatchAllArgs(ResolvedExpression()));

  // Expect the data_rule to change something.
  DataTypeRule data_rule(compiler_state_.get());
  bool did_change;
  do {
    auto result = data_rule.Execute(graph.get());
    ASSERT_OK(result.status());
    did_change = result.ConsumeValueOrDie();
  } while (did_change);

  // The function should now be evaluated, the column should stay evaluated.
  EXPECT_TRUE(func->IsDataTypeEvaluated());
  EXPECT_TRUE(col->IsDataTypeEvaluated());

  // Both should be integers.
  EXPECT_EQ(col->EvaluatedDataType(), types::DataType::INT64);
  EXPECT_EQ(func->EvaluatedDataType(), types::DataType::INT64);

  // Expect the data_rule to do nothing, no more work left.
  auto result = data_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());

  // Both should stay evaluated.
  EXPECT_TRUE(func->IsDataTypeEvaluated());
  EXPECT_TRUE(col->IsDataTypeEvaluated());
}

// The DataType shouldn't be resolved for a function without a name.
TEST_F(DataTypeRuleTest, missing_udf_name) {
  auto constant = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
  auto col = MakeColumn("count", /* parent_op_idx */ 0);
  auto func = graph
                  ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::add, "+", "gobeldy"},
                                       std::vector<ExpressionIR*>({constant, col}))
                  .ValueOrDie();
  EXPECT_OK(graph->CreateNode<MapIR>(ast, mem_src, ColExpressionVector{{"func", func}},
                                     /* keep_input_columns */ false));
  // Expect the data_rule to successfully change columnir.
  DataTypeRule data_rule(compiler_state_.get());
  Status s;
  do {
    auto result = data_rule.Execute(graph.get());
    s = result.status();
  } while (s.ok());

  EXPECT_NOT_OK(s);

  // The function should not be evaluated, the function was not matched.
  EXPECT_FALSE(func->IsDataTypeEvaluated());
  auto result_or_s = data_rule.Execute(graph.get());
  ASSERT_NOT_OK(result_or_s);
  EXPECT_THAT(result_or_s.status(), HasCompilerError("Could not find function 'gobeldy'."));
}

// Checks to make sure that agg functions work properly.
TEST_F(DataTypeRuleTest, function_in_agg) {
  auto col = MakeColumn("count", /* parent_op_idx */ 0);
  auto func = graph
                  ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "mean"},
                                       std::vector<ExpressionIR*>{col})
                  .ValueOrDie();
  EXPECT_OK(graph->CreateNode<BlockingAggIR>(ast, mem_src, std::vector<ColumnIR*>{},
                                             ColExpressionVector{{"func", func}}));

  // Expect the data_rule to successfully evaluate the column.
  DataTypeRule data_rule(compiler_state_.get());
  bool did_change;
  do {
    auto result = data_rule.Execute(graph.get());
    ASSERT_OK(result.status());
    did_change = result.ConsumeValueOrDie();
  } while (did_change);

  // The function should be evaluated.
  EXPECT_TRUE(col->IsDataTypeEvaluated());
  EXPECT_TRUE(func->IsDataTypeEvaluated());
  EXPECT_EQ(func->EvaluatedDataType(), types::DataType::FLOAT64);
}

// Checks to make sure that nested functions are evaluated as expected.
TEST_F(DataTypeRuleTest, nested_functions) {
  auto constant = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
  auto constant2 = graph->CreateNode<IntIR>(ast, 12).ValueOrDie();
  auto col = MakeColumn("count", /* parent_op_idx */ 0);
  auto func = graph
                  ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::add, "+", "add"},
                                       std::vector<ExpressionIR*>({constant, col}))
                  .ValueOrDie();
  auto func2 = graph
                   ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::add, "-", "subtract"},
                                        std::vector<ExpressionIR*>({constant2, func}))
                   .ValueOrDie();
  EXPECT_OK(graph->CreateNode<MapIR>(ast, mem_src, ColExpressionVector{{"col_name", func2}},
                                     /* keep_input_columns */ false));
  // No rule has been run, don't expect any of these to be evaluated.
  EXPECT_FALSE(func->IsDataTypeEvaluated());
  EXPECT_FALSE(func2->IsDataTypeEvaluated());
  EXPECT_FALSE(col->IsDataTypeEvaluated());

  // Expect the data_rule to change something.
  DataTypeRule data_rule(compiler_state_.get());

  bool did_change;
  int64_t number_changes = 0;
  do {
    auto result = data_rule.Execute(graph.get());
    ASSERT_OK(result.status());
    did_change = result.ConsumeValueOrDie();
    ++number_changes;
  } while (did_change);

  EXPECT_LE(number_changes, 4);

  // All should be evaluated.
  EXPECT_TRUE(func->IsDataTypeEvaluated());
  EXPECT_TRUE(func2->IsDataTypeEvaluated());
  EXPECT_TRUE(col->IsDataTypeEvaluated());

  // All should be integers.
  EXPECT_EQ(col->EvaluatedDataType(), types::DataType::INT64);
  EXPECT_EQ(func->EvaluatedDataType(), types::DataType::INT64);
  EXPECT_EQ(func2->EvaluatedDataType(), types::DataType::INT64);

  // Expect the data_rule to do nothing, no more work left.
  auto result = data_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
}

TEST_F(DataTypeRuleTest, metadata_column) {
  std::string metadata_name = "pod_name";
  MetadataProperty* property = md_handler->GetProperty(metadata_name).ValueOrDie();

  MetadataIR* metadata_ir = MakeMetadataIR(metadata_name, /* parent_op_idx */ 0);
  metadata_ir->set_property(property);
  MakeFilter(MakeMemSource(), metadata_ir);
  EXPECT_FALSE(metadata_ir->IsDataTypeEvaluated());

  DataTypeRule data_rule(compiler_state_.get());
  auto result = data_rule.Execute(graph.get());
  LOG(INFO) << graph->DebugString();
  ASSERT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());
  EXPECT_TRUE(metadata_ir->IsDataTypeEvaluated());
  EXPECT_EQ(metadata_ir->EvaluatedDataType(), types::DataType::STRING);
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
