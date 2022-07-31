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

#include <memory>
#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/rules/rule_executor.h"
#include "src/carnot/planner/rules/rule_mock.h"
#include "src/carnot/udf_exporter/udf_exporter.h"

namespace px {
namespace carnot {
namespace planner {

using ::testing::_;

class RuleExecutorTest : public OperatorTests {
 protected:
  void SetUp() override {
    info_ = udfexporter::ExportUDFInfo().ConsumeValueOrDie();

    auto rel_map = std::make_unique<RelationMap>();
    auto cpu_relation = table_store::schema::Relation(
        std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64,
                                      types::DataType::FLOAT64, types::DataType::FLOAT64}),
        std::vector<std::string>({"count", "cpu0", "cpu1", "cpu2"}));
    rel_map->emplace("cpu", cpu_relation);

    compiler_state_ = std::make_unique<CompilerState>(
        std::move(rel_map), SensitiveColumnMap{}, info_.get(), time_now, 0, "result_addr",
        "result_ssl_targetname", RedactionOptions{}, nullptr, nullptr, planner::DebugInfo{});

    ast = MakeTestAstPtr();
    graph = std::make_shared<IR>();
    mem_src =
        graph->CreateNode<MemorySourceIR>(ast, "source", std::vector<std::string>{}).ValueOrDie();
    SetupGraph();
  }
  void SetupGraph() {
    int_constant = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
    int_constant2 = graph->CreateNode<IntIR>(ast, 12).ValueOrDie();
    col = graph->CreateNode<ColumnIR>(ast, "count", /* parent_op_idx */ 0).ValueOrDie();
    func = graph
               ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::add, "+", "add"},
                                    std::vector<ExpressionIR*>({int_constant, col}))
               .ValueOrDie();
    func2 = graph
                ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::add, "+", "add"},
                                     std::vector<ExpressionIR*>({int_constant2, func}))
                .ValueOrDie();
    map = graph
              ->CreateNode<MapIR>(ast, mem_src, ColExpressionVector{{"func", func2}},
                                  /* keep_input_columns */ false)
              .ValueOrDie();
  }

  std::unique_ptr<CompilerState> compiler_state_;
  std::unique_ptr<RegistryInfo> info_;
  int64_t time_now = 1552607213931245000;
  pypa::AstPtr ast;
  std::shared_ptr<IR> graph;
  MemorySourceIR* mem_src;
  FuncIR* func;
  FuncIR* func2;
  MapIR* map;
  IntIR* int_constant;
  IntIR* int_constant2;
  ColumnIR* col;
};

class TestExecutor : public RuleExecutor<IR> {
 public:
  static StatusOr<std::unique_ptr<TestExecutor>> Create() {
    std::unique_ptr<TestExecutor> executor(new TestExecutor());
    return executor;
  }
};

TEST(StrategyTest, fail_on_max) {
  int64_t num_iterations = 10;
  auto s = std::make_unique<FailOnMax>("test", num_iterations);
  Strategy* s_down_cast = s.get();
  EXPECT_EQ(s_down_cast->max_iterations(), num_iterations);
  Status status = s_down_cast->MaxIterationsHandler();
  EXPECT_NOT_OK(status);
  EXPECT_EQ(status.msg(), "Reached max iterations (10) for rule batch 'test'");
}

TEST(StrategyTest, try_until_max) {
  int64_t num_iterations = 10;
  auto s = std::make_unique<TryUntilMax>("", num_iterations);
  Strategy* s_down_cast = s.get();
  EXPECT_EQ(s_down_cast->max_iterations(), num_iterations);
  Status status = s_down_cast->MaxIterationsHandler();
  EXPECT_OK(status);
}

TEST(StrategyTest, do_once) {
  auto s = std::make_unique<DoOnce>("");
  Strategy* s_down_cast = s.get();
  EXPECT_EQ(s_down_cast->max_iterations(), 1);
  Status status = s_down_cast->MaxIterationsHandler();
  EXPECT_OK(status);
}

using ::testing::Return;

// Tests that rule execution works as expected in simple 1 batch case.
TEST_F(RuleExecutorTest, rule_executor_test) {
  std::unique_ptr<TestExecutor> executor = std::move(TestExecutor::Create().ValueOrDie());
  RuleBatch* rule_batch = executor->CreateRuleBatch<FailOnMax>("resolve", 10);
  MockRule* rule1 = rule_batch->AddRule<MockRule>(compiler_state_.get());
  EXPECT_CALL(*rule1, Execute(_)).Times(2).WillOnce(Return(true)).WillRepeatedly(Return(false));
  ASSERT_OK(executor->Execute(graph.get()));
}

// // Tests to see that rules in different batches can run.
TEST_F(RuleExecutorTest, multiple_rule_batches) {
  std::unique_ptr<TestExecutor> executor = std::move(TestExecutor::Create().ValueOrDie());
  RuleBatch* rule_batch1 = executor->CreateRuleBatch<FailOnMax>("resolve", 10);
  MockRule* rule1_1 = rule_batch1->AddRule<MockRule>(compiler_state_.get());
  EXPECT_CALL(*rule1_1, Execute(_)).Times(2).WillRepeatedly(Return(false));
  MockRule* rule1_2 = rule_batch1->AddRule<MockRule>(compiler_state_.get());
  EXPECT_CALL(*rule1_2, Execute(_)).Times(2).WillOnce(Return(true)).WillRepeatedly(Return(false));

  RuleBatch* rule_batch2 = executor->CreateRuleBatch<FailOnMax>("resolve", 10);
  MockRule* rule2_1 = rule_batch2->AddRule<MockRule>(compiler_state_.get());
  EXPECT_CALL(*rule2_1, Execute(_)).Times(2).WillOnce(Return(true)).WillRepeatedly(Return(false));
  EXPECT_OK(executor->Execute(graph.get()));
}

// Tests to see that within a rule batch, rules will run if their sibling rule changes the graph.
TEST_F(RuleExecutorTest, rules_in_batch_correspond) {
  std::unique_ptr<TestExecutor> executor = std::move(TestExecutor::Create().ValueOrDie());
  RuleBatch* rule_batch1 = executor->CreateRuleBatch<FailOnMax>("resolve", 10);
  MockRule* rule1_1 = rule_batch1->AddRule<MockRule>(compiler_state_.get());
  EXPECT_CALL(*rule1_1, Execute(_))
      .Times(3)
      .WillOnce(Return(false))
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  MockRule* rule1_2 = rule_batch1->AddRule<MockRule>(compiler_state_.get());
  EXPECT_CALL(*rule1_2, Execute(_)).Times(3).WillOnce(Return(true)).WillRepeatedly(Return(false));

  EXPECT_OK(executor->Execute(graph.get()));
}

// Test to see that if the strategy exits, then following batches don't run.
TEST_F(RuleExecutorTest, exit_early) {
  std::unique_ptr<TestExecutor> executor = std::move(TestExecutor::Create().ValueOrDie());
  RuleBatch* rule_batch1 = executor->CreateRuleBatch<FailOnMax>("resolve", 10);
  MockRule* rule1_1 = rule_batch1->AddRule<MockRule>(compiler_state_.get());
  EXPECT_CALL(*rule1_1, Execute(_)).Times(10).WillRepeatedly(Return(true));

  RuleBatch* rule_batch2 = executor->CreateRuleBatch<FailOnMax>("resolve", 10);
  MockRule* rule2_1 = rule_batch2->AddRule<MockRule>(compiler_state_.get());
  EXPECT_CALL(*rule2_1, Execute(_)).Times(0);

  EXPECT_NOT_OK(executor->Execute(graph.get()));
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
