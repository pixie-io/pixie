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

#include "src/carnot/exec/exec_graph.h"

#include <arrow/array.h>
#include <arrow/memory_pool.h>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <sole.hpp>

#include "src/carnot/exec/grpc_source_node.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/plan/plan_fragment.h"
#include "src/carnot/plan/plan_state.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf/base.h"
#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/types.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace exec {

using google::protobuf::TextFormat;
using table_store::Table;
using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;

class AddUDF : public udf::ScalarUDF {
 public:
  types::Float64Value Exec(udf::FunctionContext*, types::Int64Value v1, types::Float64Value v2) {
    return v1.val + v2.val;
  }
};

class MultiplyUDF : public udf::ScalarUDF {
 public:
  types::Float64Value Exec(udf::FunctionContext*, types::Float64Value v1, types::Int64Value v2) {
    return v1.val * v2.val;
  }
};

class BaseExecGraphTest : public ::testing::Test {
 protected:
  void SetUpExecState() {
    func_registry_ = std::make_unique<udf::Registry>("test_registry");
    func_registry_->RegisterOrDie<AddUDF>("add");
    func_registry_->RegisterOrDie<MultiplyUDF>("multiply");

    auto table_store = std::make_shared<table_store::TableStore>();
    exec_state_ = std::make_unique<ExecState>(func_registry_.get(), table_store,
                                              MockResultSinkStubGenerator, MockMetricsStubGenerator,
                                              MockTraceStubGenerator, sole::uuid4(), nullptr);
  }

  std::unique_ptr<udf::Registry> func_registry_;
  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
  std::unique_ptr<ExecState> exec_state_ = nullptr;
};

class ExecGraphTest : public BaseExecGraphTest {
 protected:
  void SetUp() override {
    SetUpPlanFragment();
    SetUpExecState();
  }

  void SetUpPlanFragment() {
    planpb::PlanFragment pf_pb;
    ASSERT_TRUE(TextFormat::MergeFromString(planpb::testutils::kPlanFragmentWithFourNodes, &pf_pb));
    ASSERT_OK(plan_fragment_->Init(pf_pb));
  }
};

TEST_F(ExecGraphTest, basic) {
  ExecutionGraph e;
  auto plan_state = std::make_unique<plan::PlanState>(func_registry_.get());

  auto schema = std::make_shared<table_store::schema::Schema>();
  table_store::schema::Relation relation(std::vector<types::DataType>({types::DataType::INT64}),
                                         std::vector<std::string>({"test"}));
  schema->AddRelation(1, relation);

  auto s = e.Init(schema.get(), plan_state.get(), exec_state_.get(), plan_fragment_.get(),
                  /* collect_exec_node_stats */ false);

  // Check that the structure of the exec graph is correct.
  auto sources = e.sources();
  EXPECT_EQ(1, sources.size());
  auto root = e.node(sources[0]).ConsumeValueOrDie();
  EXPECT_TRUE(root->IsSource());
  auto root_children = root->children();
  EXPECT_EQ(2, root_children.size());
  EXPECT_TRUE(root_children[0]->IsProcessing());
  EXPECT_TRUE(root_children[1]->IsProcessing());
  EXPECT_EQ(1, root_children[0]->children().size());
  EXPECT_EQ(1, root_children[1]->children().size());
  EXPECT_TRUE(root_children[0]->children()[0]->IsSink());
  EXPECT_EQ(root_children[1]->children()[0], root_children[0]->children()[0]);
  EXPECT_EQ(0, root_children[1]->children()[0]->children().size());
}

class ExecGraphExecuteTest : public ExecGraphTest,
                             public ::testing::WithParamInterface<std::tuple<int32_t>> {};

TEST_P(ExecGraphExecuteTest, execute) {
  int32_t calls_to_generate;
  std::tie(calls_to_generate) = GetParam();

  planpb::PlanFragment pf_pb;
  ASSERT_TRUE(TextFormat::MergeFromString(planpb::testutils::kLinearPlanFragment, &pf_pb));
  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
  ASSERT_OK(plan_fragment_->Init(pf_pb));

  auto plan_state = std::make_unique<plan::PlanState>(func_registry_.get());

  auto schema = std::make_shared<table_store::schema::Schema>();
  schema->AddRelation(
      1, table_store::schema::Relation(
             std::vector<types::DataType>(
                 {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64}),
             std::vector<std::string>({"a", "b", "c"})));

  table_store::schema::Relation rel(
      {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64},
      {"col1", "col2", "col3"});
  auto table = Table::Create("test", rel);

  auto rb1 = RowBatch(RowDescriptor(rel.col_types()), 3);
  std::vector<types::Int64Value> col1_in1 = {1, 2, 3};
  std::vector<types::BoolValue> col2_in1 = {true, false, true};
  std::vector<types::Float64Value> col3_in1 = {1.4, 6.2, 10.2};
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col2_in1, arrow::default_memory_pool())));
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col3_in1, arrow::default_memory_pool())));
  EXPECT_OK(table->WriteRowBatch(rb1));

  auto rb2 = RowBatch(RowDescriptor(rel.col_types()), 2);
  std::vector<types::Int64Value> col1_in2 = {4, 5};
  std::vector<types::BoolValue> col2_in2 = {false, false};
  std::vector<types::Float64Value> col3_in2 = {3.4, 1.2};
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col2_in2, arrow::default_memory_pool())));
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col3_in2, arrow::default_memory_pool())));
  EXPECT_OK(table->WriteRowBatch(rb2));

  auto table_store = std::make_shared<table_store::TableStore>();
  table_store->AddTable("numbers", table);
  auto exec_state_ = std::make_unique<ExecState>(
      func_registry_.get(), table_store, MockResultSinkStubGenerator, MockMetricsStubGenerator,
      MockTraceStubGenerator, sole::uuid4(), nullptr);

  EXPECT_OK(exec_state_->AddScalarUDF(
      0, "add", std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64})));
  EXPECT_OK(exec_state_->AddScalarUDF(
      1, "multiply",
      std::vector<types::DataType>({types::DataType::FLOAT64, types::DataType::INT64})));

  ExecutionGraph e;
  auto s = e.Init(schema.get(), plan_state.get(), exec_state_.get(), plan_fragment_.get(),
                  /* collect_exec_node_stats */ false, calls_to_generate);

  EXPECT_OK(e.Execute());

  auto output_table = exec_state_->table_store()->GetTable("output");
  std::vector<types::Float64Value> out_in1 = {4.8, 16.4, 26.4};
  std::vector<types::Float64Value> out_in2 = {14.8, 12.4};
  table_store::Table::Cursor cursor(output_table);
  EXPECT_TRUE(cursor.GetNextRowBatch({0}).ConsumeValueOrDie()->ColumnAt(0)->Equals(
      types::ToArrow(out_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(cursor.GetNextRowBatch({0}).ConsumeValueOrDie()->ColumnAt(0)->Equals(
      types::ToArrow(out_in2, arrow::default_memory_pool())));
}

std::vector<std::tuple<int32_t>> calls_to_execute = {
    {1},
    {2},
    {3},
    {4},
};

INSTANTIATE_TEST_SUITE_P(ExecGraphExecuteTestSuite, ExecGraphExecuteTest,
                         ::testing::ValuesIn(calls_to_execute));

TEST_F(ExecGraphTest, execute_time) {
  planpb::PlanFragment pf_pb;
  ASSERT_TRUE(TextFormat::MergeFromString(planpb::testutils::kLinearPlanFragment, &pf_pb));
  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
  ASSERT_OK(plan_fragment_->Init(pf_pb));

  auto func_registry = std::make_unique<udf::Registry>("testUDF");
  EXPECT_OK(func_registry->Register<AddUDF>("add"));
  EXPECT_OK(func_registry->Register<MultiplyUDF>("multiply"));

  auto plan_state = std::make_unique<plan::PlanState>(func_registry.get());
  auto schema = std::make_shared<table_store::schema::Schema>();
  schema->AddRelation(
      1, table_store::schema::Relation(
             std::vector<types::DataType>(
                 {types::DataType::TIME64NS, types::DataType::BOOLEAN, types::DataType::FLOAT64}),
             std::vector<std::string>({"a", "b", "c"})));

  table_store::schema::Relation rel(
      {types::DataType::TIME64NS, types::DataType::BOOLEAN, types::DataType::FLOAT64},
      {"col1", "col2", "col3"});
  auto table = Table::Create("test", rel);

  auto rb1 = RowBatch(RowDescriptor(rel.col_types()), 3);
  std::vector<types::Time64NSValue> col1_in1 = {types::Time64NSValue(1), types::Time64NSValue(2),
                                                types::Time64NSValue(3)};
  std::vector<types::BoolValue> col2_in1 = {true, false, true};
  std::vector<types::Float64Value> col3_in1 = {1.4, 6.2, 10.2};
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col2_in1, arrow::default_memory_pool())));
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col3_in1, arrow::default_memory_pool())));
  EXPECT_OK(table->WriteRowBatch(rb1));

  auto rb2 = RowBatch(RowDescriptor(rel.col_types()), 2);
  std::vector<types::Time64NSValue> col1_in2 = {types::Time64NSValue(4), types::Time64NSValue(5)};
  std::vector<types::BoolValue> col2_in2 = {false, false};
  std::vector<types::Float64Value> col3_in2 = {3.4, 1.2};
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col2_in2, arrow::default_memory_pool())));
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col3_in2, arrow::default_memory_pool())));
  EXPECT_OK(table->WriteRowBatch(rb2));

  auto table_store = std::make_shared<table_store::TableStore>();
  table_store->AddTable("numbers", table);

  auto exec_state_ = std::make_unique<ExecState>(
      func_registry.get(), table_store, MockResultSinkStubGenerator, MockMetricsStubGenerator,
      MockTraceStubGenerator, sole::uuid4(), nullptr);

  EXPECT_OK(exec_state_->AddScalarUDF(
      0, "add", std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64})));
  EXPECT_OK(exec_state_->AddScalarUDF(
      1, "multiply",
      std::vector<types::DataType>({types::DataType::FLOAT64, types::DataType::INT64})));

  ExecutionGraph e;
  auto s = e.Init(schema.get(), plan_state.get(), exec_state_.get(), plan_fragment_.get(),
                  /* collect_exec_node_stats */ false);

  EXPECT_OK(e.Execute());

  auto output_table = exec_state_->table_store()->GetTable("output");
  std::vector<types::Float64Value> out_in1 = {4.8, 16.4, 26.4};
  std::vector<types::Float64Value> out_in2 = {14.8, 12.4};
  table_store::Table::Cursor cursor(output_table);
  EXPECT_TRUE(cursor.GetNextRowBatch({0}).ConsumeValueOrDie()->ColumnAt(0)->Equals(
      types::ToArrow(out_in1, arrow::default_memory_pool())));
  EXPECT_TRUE(cursor.GetNextRowBatch({0}).ConsumeValueOrDie()->ColumnAt(0)->Equals(
      types::ToArrow(out_in2, arrow::default_memory_pool())));
}

TEST_F(ExecGraphTest, two_limits_dont_interfere) {
  planpb::PlanFragment pf_pb;
  ASSERT_TRUE(
      TextFormat::MergeFromString(planpb::testutils::kPlanWithTwoSourcesWithLimits, &pf_pb));
  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
  ASSERT_OK(plan_fragment_->Init(pf_pb));

  auto plan_state = std::make_unique<plan::PlanState>(func_registry_.get());

  auto schema = std::make_shared<table_store::schema::Schema>();
  schema->AddRelation(
      1, table_store::schema::Relation(
             std::vector<types::DataType>(
                 {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64}),
             std::vector<std::string>({"a", "b", "c"})));

  table_store::schema::Relation rel(
      {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64},
      {"col1", "col2", "col3"});
  auto table = Table::Create("test", rel);

  auto rb1 = RowBatch(RowDescriptor(rel.col_types()), 3);
  std::vector<types::Int64Value> col1_in1 = {1, 2, 3};
  std::vector<types::BoolValue> col2_in1 = {true, false, true};
  std::vector<types::Float64Value> col3_in1 = {1.4, 6.2, 10.2};
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col2_in1, arrow::default_memory_pool())));
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col3_in1, arrow::default_memory_pool())));
  EXPECT_OK(table->WriteRowBatch(rb1));

  auto rb2 = RowBatch(RowDescriptor(rel.col_types()), 2);
  std::vector<types::Int64Value> col1_in2 = {4, 5};
  std::vector<types::BoolValue> col2_in2 = {false, false};
  std::vector<types::Float64Value> col3_in2 = {3.4, 1.2};
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col2_in2, arrow::default_memory_pool())));
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col3_in2, arrow::default_memory_pool())));
  EXPECT_OK(table->WriteRowBatch(rb2));

  auto table_store = std::make_shared<table_store::TableStore>();
  table_store->AddTable("numbers", table);
  auto exec_state_ = std::make_unique<ExecState>(
      func_registry_.get(), table_store, MockResultSinkStubGenerator, MockMetricsStubGenerator,
      MockTraceStubGenerator, sole::uuid4(), nullptr);

  ExecutionGraph e;
  auto s = e.Init(schema.get(), plan_state.get(), exec_state_.get(), plan_fragment_.get(),
                  /* collect_exec_node_stats */ false);

  EXPECT_OK(e.Execute());

  auto output_table1 = exec_state_->table_store()->GetTable("output1");
  auto output_table2 = exec_state_->table_store()->GetTable("output2");
  std::vector<types::Int64Value> out_col1 = {1, 2};
  std::vector<types::BoolValue> out_col2 = {true, false};
  std::vector<types::Float64Value> out_col3 = {1.4, 6.2};
  table_store::Table::Cursor cursor1(output_table1);
  table_store::Table::Cursor cursor2(output_table2);

  auto out_rb1 = cursor1.GetNextRowBatch(std::vector<int64_t>({0, 1, 2})).ConsumeValueOrDie();
  auto out_rb2 = cursor2.GetNextRowBatch(std::vector<int64_t>({0, 1, 2})).ConsumeValueOrDie();
  EXPECT_TRUE(out_rb1->ColumnAt(0)->Equals(types::ToArrow(out_col1, arrow::default_memory_pool())));
  EXPECT_TRUE(out_rb1->ColumnAt(1)->Equals(types::ToArrow(out_col2, arrow::default_memory_pool())));
  EXPECT_TRUE(out_rb1->ColumnAt(2)->Equals(types::ToArrow(out_col3, arrow::default_memory_pool())));
  EXPECT_TRUE(out_rb2->ColumnAt(0)->Equals(types::ToArrow(out_col1, arrow::default_memory_pool())));
  EXPECT_TRUE(out_rb2->ColumnAt(1)->Equals(types::ToArrow(out_col2, arrow::default_memory_pool())));
  EXPECT_TRUE(out_rb2->ColumnAt(2)->Equals(types::ToArrow(out_col3, arrow::default_memory_pool())));
}

TEST_F(ExecGraphTest, limit_w_multiple_srcs) {
  planpb::PlanFragment pf_pb;
  ASSERT_TRUE(TextFormat::MergeFromString(planpb::testutils::kOneLimit3Sources, &pf_pb));
  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
  ASSERT_OK(plan_fragment_->Init(pf_pb));

  auto plan_state = std::make_unique<plan::PlanState>(func_registry_.get());

  auto schema = std::make_shared<table_store::schema::Schema>();
  schema->AddRelation(
      1, table_store::schema::Relation(
             std::vector<types::DataType>(
                 {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64}),
             std::vector<std::string>({"a", "b", "c"})));

  table_store::schema::Relation rel(
      {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64},
      {"col1", "col2", "col3"});
  auto table = Table::Create("test", rel);

  auto rb1 = RowBatch(RowDescriptor(rel.col_types()), 3);
  std::vector<types::Int64Value> col1_in1 = {1, 2, 3};
  std::vector<types::BoolValue> col2_in1 = {true, false, true};
  std::vector<types::Float64Value> col3_in1 = {1.4, 6.2, 10.2};
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col2_in1, arrow::default_memory_pool())));
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col3_in1, arrow::default_memory_pool())));
  EXPECT_OK(table->WriteRowBatch(rb1));

  auto rb2 = RowBatch(RowDescriptor(rel.col_types()), 2);
  std::vector<types::Int64Value> col1_in2 = {4, 5};
  std::vector<types::BoolValue> col2_in2 = {false, false};
  std::vector<types::Float64Value> col3_in2 = {3.4, 1.2};
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col2_in2, arrow::default_memory_pool())));
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col3_in2, arrow::default_memory_pool())));
  EXPECT_OK(table->WriteRowBatch(rb2));

  auto table_store = std::make_shared<table_store::TableStore>();
  table_store->AddTable("numbers", table);
  auto exec_state_ = std::make_unique<ExecState>(
      func_registry_.get(), table_store, MockResultSinkStubGenerator, MockMetricsStubGenerator,
      MockTraceStubGenerator, sole::uuid4(), nullptr);

  ExecutionGraph e;
  auto s = e.Init(schema.get(), plan_state.get(), exec_state_.get(), plan_fragment_.get(),
                  /* collect_exec_node_stats */ false);

  EXPECT_OK(e.Execute());

  auto output_table = exec_state_->table_store()->GetTable("output");
  std::vector<types::Int64Value> out_col1 = {1, 2};
  std::vector<types::BoolValue> out_col2 = {true, false};
  std::vector<types::Float64Value> out_col3 = {1.4, 6.2};
  table_store::Table::Cursor cursor(output_table);
  auto out_rb = cursor.GetNextRowBatch(std::vector<int64_t>({0, 1, 2})).ConsumeValueOrDie();
  EXPECT_TRUE(out_rb->ColumnAt(0)->Equals(types::ToArrow(out_col1, arrow::default_memory_pool())));
  EXPECT_TRUE(out_rb->ColumnAt(1)->Equals(types::ToArrow(out_col2, arrow::default_memory_pool())));
  EXPECT_TRUE(out_rb->ColumnAt(2)->Equals(types::ToArrow(out_col3, arrow::default_memory_pool())));
}

TEST_F(ExecGraphTest, two_sequential_limits) {
  planpb::PlanFragment pf_pb;
  ASSERT_TRUE(TextFormat::MergeFromString(planpb::testutils::kTwoSequentialLimits, &pf_pb));
  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
  ASSERT_OK(plan_fragment_->Init(pf_pb));

  auto plan_state = std::make_unique<plan::PlanState>(func_registry_.get());

  auto schema = std::make_shared<table_store::schema::Schema>();
  schema->AddRelation(
      1, table_store::schema::Relation(
             std::vector<types::DataType>(
                 {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64}),
             std::vector<std::string>({"a", "b", "c"})));

  table_store::schema::Relation rel(
      {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64},
      {"col1", "col2", "col3"});
  auto table = Table::Create("test", rel);

  auto rb1 = RowBatch(RowDescriptor(rel.col_types()), 3);
  std::vector<types::Int64Value> col1_in1 = {1, 2, 3};
  std::vector<types::BoolValue> col2_in1 = {true, false, true};
  std::vector<types::Float64Value> col3_in1 = {1.4, 6.2, 10.2};

  EXPECT_OK(rb1.AddColumn(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col2_in1, arrow::default_memory_pool())));
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col3_in1, arrow::default_memory_pool())));
  EXPECT_OK(table->WriteRowBatch(rb1));

  auto rb2 = RowBatch(RowDescriptor(rel.col_types()), 2);
  std::vector<types::Int64Value> col1_in2 = {4, 5};
  std::vector<types::BoolValue> col2_in2 = {false, false};
  std::vector<types::Float64Value> col3_in2 = {3.4, 1.2};
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col2_in2, arrow::default_memory_pool())));
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col3_in2, arrow::default_memory_pool())));
  EXPECT_OK(table->WriteRowBatch(rb2));

  auto table_store = std::make_shared<table_store::TableStore>();
  table_store->AddTable("numbers", table);
  auto exec_state_ = std::make_unique<ExecState>(
      func_registry_.get(), table_store, MockResultSinkStubGenerator, MockMetricsStubGenerator,
      MockTraceStubGenerator, sole::uuid4(), nullptr);

  ExecutionGraph e;
  auto s = e.Init(schema.get(), plan_state.get(), exec_state_.get(), plan_fragment_.get(),
                  /* collect_exec_node_stats */ false);

  EXPECT_OK(e.Execute());

  auto output_table = exec_state_->table_store()->GetTable("output");
  std::vector<types::Int64Value> out_col1 = {1, 2};
  std::vector<types::BoolValue> out_col2 = {true, false};
  std::vector<types::Float64Value> out_col3 = {1.4, 6.2};
  table_store::Table::Cursor cursor(output_table);
  auto out_rb = cursor.GetNextRowBatch({0, 1, 2}).ConsumeValueOrDie();
  EXPECT_TRUE(out_rb->ColumnAt(0)->Equals(types::ToArrow(out_col1, arrow::default_memory_pool())));
  EXPECT_TRUE(out_rb->ColumnAt(1)->Equals(types::ToArrow(out_col2, arrow::default_memory_pool())));
  EXPECT_TRUE(out_rb->ColumnAt(2)->Equals(types::ToArrow(out_col3, arrow::default_memory_pool())));
}

TEST_F(ExecGraphTest, execute_with_two_limits) {
  planpb::PlanFragment pf_pb;
  ASSERT_TRUE(
      TextFormat::MergeFromString(planpb::testutils::kPlanWithTwoSourcesWithLimits, &pf_pb));
  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
  ASSERT_OK(plan_fragment_->Init(pf_pb));

  auto plan_state = std::make_unique<plan::PlanState>(func_registry_.get());

  auto schema = std::make_shared<table_store::schema::Schema>();
  schema->AddRelation(
      1, table_store::schema::Relation(
             std::vector<types::DataType>(
                 {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64}),
             std::vector<std::string>({"a", "b", "c"})));

  table_store::schema::Relation rel(
      {types::DataType::INT64, types::DataType::BOOLEAN, types::DataType::FLOAT64},
      {"col1", "col2", "col3"});
  auto table = Table::Create("test", rel);

  auto rb1 = RowBatch(RowDescriptor(rel.col_types()), 3);
  std::vector<types::Int64Value> col1_in1 = {1, 2, 3};
  std::vector<types::BoolValue> col2_in1 = {true, false, true};
  std::vector<types::Float64Value> col3_in1 = {1.4, 6.2, 10.2};

  EXPECT_OK(rb1.AddColumn(types::ToArrow(col1_in1, arrow::default_memory_pool())));
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col2_in1, arrow::default_memory_pool())));
  EXPECT_OK(rb1.AddColumn(types::ToArrow(col3_in1, arrow::default_memory_pool())));
  EXPECT_OK(table->WriteRowBatch(rb1));

  auto rb2 = RowBatch(RowDescriptor(rel.col_types()), 2);
  std::vector<types::Int64Value> col1_in2 = {4, 5};
  std::vector<types::BoolValue> col2_in2 = {false, false};
  std::vector<types::Float64Value> col3_in2 = {3.4, 1.2};
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col1_in2, arrow::default_memory_pool())));
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col2_in2, arrow::default_memory_pool())));
  EXPECT_OK(rb2.AddColumn(types::ToArrow(col3_in2, arrow::default_memory_pool())));
  EXPECT_OK(table->WriteRowBatch(rb2));

  auto table_store = std::make_shared<table_store::TableStore>();
  table_store->AddTable("numbers", table);
  auto exec_state_ = std::make_unique<ExecState>(
      func_registry_.get(), table_store, MockResultSinkStubGenerator, MockMetricsStubGenerator,
      MockTraceStubGenerator, sole::uuid4(), nullptr);

  ExecutionGraph e;
  auto s = e.Init(schema.get(), plan_state.get(), exec_state_.get(), plan_fragment_.get(),
                  /* collect_exec_node_stats */ false);

  EXPECT_OK(e.Execute());

  auto output_table_1 = exec_state_->table_store()->GetTable("output1");
  auto output_table_2 = exec_state_->table_store()->GetTable("output2");
  std::vector<types::Float64Value> out_in1 = {1.4, 6.2};
  table_store::Table::Cursor cursor1(output_table_1);
  EXPECT_TRUE(cursor1.GetNextRowBatch({2}).ConsumeValueOrDie()->ColumnAt(0)->Equals(
      types::ToArrow(out_in1, arrow::default_memory_pool())));
  table_store::Table::Cursor cursor2(output_table_2);
  EXPECT_TRUE(cursor2.GetNextRowBatch({2}).ConsumeValueOrDie()->ColumnAt(0)->Equals(
      types::ToArrow(out_in1, arrow::default_memory_pool())));
}

class YieldingExecGraphTest : public BaseExecGraphTest {
 protected:
  void SetUp() { SetUpExecState(); }
};

TEST_F(YieldingExecGraphTest, yield) {
  ExecutionGraph e{std::chrono::milliseconds(1), std::chrono::milliseconds(1)};
  e.testing_set_exec_state(exec_state_.get());

  RowDescriptor output_rd({types::DataType::INT64});
  MockSourceNode yielding_source(output_rd);
  MockSourceNode non_yielding_source(output_rd);

  FakePlanNode yielding_plan_node(1);
  FakePlanNode non_yielding_plan_node(2);

  EXPECT_CALL(yielding_source, InitImpl(::testing::_));
  EXPECT_CALL(non_yielding_source, InitImpl(::testing::_));

  ASSERT_OK(yielding_source.Init(yielding_plan_node, output_rd, {}));
  ASSERT_OK(non_yielding_source.Init(non_yielding_plan_node, output_rd, {}));

  e.AddNode(1, &yielding_source);
  e.AddNode(2, &non_yielding_source);

  auto set_non_yielding_eos = [&](ExecState*) { non_yielding_source.SendEOS(); };
  auto set_yielding_eos = [&](ExecState*) { yielding_source.SendEOS(); };

  // Setup
  EXPECT_CALL(non_yielding_source, PrepareImpl(::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(Status::OK()));
  EXPECT_CALL(non_yielding_source, OpenImpl(::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(Status::OK()));
  EXPECT_CALL(yielding_source, PrepareImpl(::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(Status::OK()));
  EXPECT_CALL(yielding_source, OpenImpl(::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(Status::OK()));

  // Non-yielding
  EXPECT_CALL(non_yielding_source, NextBatchReady())
      .Times(3)
      .WillOnce(::testing::Return(true))
      .WillOnce(::testing::Return(true))
      .WillOnce(::testing::Return(false));

  EXPECT_CALL(non_yielding_source, GenerateNextImpl(::testing::_))
      .Times(2)
      .WillOnce(::testing::Return(Status::OK()))
      .WillOnce(::testing::DoAll(::testing::Invoke(set_non_yielding_eos),
                                 ::testing::Return(Status::OK())));

  // YieldingSource will retrigger execution once it returns true for NextBatchReady.
  EXPECT_CALL(yielding_source, NextBatchReady())
      .Times(7)
      .WillOnce(::testing::Return(false))
      .WillOnce(::testing::Return(false))
      .WillOnce(::testing::Return(false))
      .WillOnce(::testing::Return(true))    // Break out of the wait loop
      .WillOnce(::testing::Return(true))    // Generate first batch
      .WillOnce(::testing::Return(true))    // Generate EOS batch.
      .WillOnce(::testing::Return(false));  // No more batches

  EXPECT_CALL(yielding_source, GenerateNextImpl(::testing::_))
      .Times(2)
      .WillOnce(::testing::Return(Status::OK()))
      .WillOnce(
          ::testing::DoAll(::testing::Invoke(set_yielding_eos), ::testing::Return(Status::OK())));

  // Cleanup
  EXPECT_CALL(non_yielding_source, CloseImpl(::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(Status::OK()));

  EXPECT_CALL(yielding_source, CloseImpl(::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(Status::OK()));

  ASSERT_OK(e.Execute());
}

TEST_F(YieldingExecGraphTest, continue_yield) {
// This test appears to generate a false TSAN error.
// Possibly related to https://github.com/google/sanitizers/issues/1259
#if !__has_feature(thread_sanitizer)
  ExecutionGraph e;
  e.testing_set_exec_state(exec_state_.get());

  std::thread exec_thread([&] {
    bool timed_out = e.YieldWithTimeout();
    ASSERT_FALSE(timed_out);
  });

  e.Continue();
  exec_thread.join();
#endif
}

TEST_F(YieldingExecGraphTest, yield_timeout) {
  ExecutionGraph e{std::chrono::milliseconds(1), std::chrono::milliseconds(1)};
  e.testing_set_exec_state(exec_state_.get());

  std::thread exec_thread([&] {
    bool timed_out = e.YieldWithTimeout();
    ASSERT_TRUE(timed_out);
  });
  exec_thread.join();
}

constexpr char kGRPCSourcePlanFragment[] = R"(
  id: 1,
  dag {
    nodes {
      id: 1
      sorted_children: 2
    }
    nodes {
      id: 2
      sorted_parents: 1
    }
  }
  nodes {
    id: 1
    op {
      op_type: GRPC_SOURCE_OPERATOR
      grpc_source_op {
        column_types: INT64
        column_names: "test"
      }
    }
  }
  nodes {
    id: 2
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "mem_sink"
        column_types: INT64
        column_names: "test"
      }
    }
  }
)";

class GRPCExecGraphTest : public ::testing::Test {
 protected:
  void SetUp() override {
    SetUpPlanFragment();
    SetUpExecStateWithGRPCRouter();
    schema_ = std::make_shared<table_store::schema::Schema>();
    table_store::schema::Relation relation(std::vector<types::DataType>({types::DataType::INT64}),
                                           std::vector<std::string>({"test"}));
    schema_->AddRelation(1, relation);
    plan_state_ = std::make_unique<plan::PlanState>(func_registry_.get());
  }

  void SetUpExecStateWithGRPCRouter() {
    grpc_router_ = std::make_unique<GRPCRouter>();
    func_registry_ = std::make_unique<udf::Registry>("test_registry");
    func_registry_->RegisterOrDie<AddUDF>("add");
    func_registry_->RegisterOrDie<MultiplyUDF>("multiply");

    auto table_store = std::make_shared<table_store::TableStore>();
    exec_state_ = std::make_unique<ExecState>(
        func_registry_.get(), table_store, MockResultSinkStubGenerator, MockMetricsStubGenerator,
        MockTraceStubGenerator, sole::uuid4(), nullptr, grpc_router_.get());
  }

  void SetUpPlanFragment() {
    planpb::PlanFragment pf_pb;
    ASSERT_TRUE(TextFormat::MergeFromString(kGRPCSourcePlanFragment, &pf_pb));
    ASSERT_OK(plan_fragment_->Init(pf_pb));
  }

  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
  std::shared_ptr<table_store::schema::Schema> schema_;
  std::unique_ptr<plan::PlanState> plan_state_;
  std::unique_ptr<GRPCRouter> grpc_router_;
  std::unique_ptr<udf::Registry> func_registry_;
  std::unique_ptr<ExecState> exec_state_;
};

TEST_F(GRPCExecGraphTest, pending_row_batches_closed_connection_eos_success) {
  // Make a GRPC source, but don't initiate the connection.
  // Set timeouts to be 0 so that we definitely time out on connections being established.
  ExecutionGraph e{std::chrono::milliseconds(0), std::chrono::milliseconds(0)};
  auto s = e.Init(schema_.get(), plan_state_.get(), exec_state_.get(), plan_fragment_.get(),
                  /* collect_exec_node_stats */ false);
  ASSERT_OK(s);

  auto grpc_sources = e.grpc_sources();
  ASSERT_EQ(1, grpc_sources.size());
  auto src_id = *(grpc_sources.begin());
  auto grpc_src = static_cast<GRPCSourceNode*>(e.node(src_id).ConsumeValueOrDie());
  grpc_src->set_upstream_initiated_connection();
  auto req1 = std::make_unique<carnotpb::TransferResultChunkRequest>();
  auto req2 = std::make_unique<carnotpb::TransferResultChunkRequest>();

  RowDescriptor output_rd({types::DataType::INT64});
  auto rb1 = RowBatchBuilder(output_rd, 2, /*eow*/ false, /*eos*/ false)
                 .AddColumn<types::Int64Value>({2, 3})
                 .get();
  auto rb2 = RowBatchBuilder(output_rd, 3, /*eow*/ true, /*eos*/ true)
                 .AddColumn<types::Int64Value>({4, 5, 6})
                 .get();

  ASSERT_OK(rb1.ToProto(req1->mutable_query_result()->mutable_row_batch()));
  ASSERT_OK(rb2.ToProto(req2->mutable_query_result()->mutable_row_batch()));

  ASSERT_OK(grpc_src->EnqueueRowBatch(std::move(req1)));
  ASSERT_OK(grpc_src->EnqueueRowBatch(std::move(req2)));
  grpc_src->set_upstream_closed_connection();

  // This should be a successful case.
  s = e.Execute();
  EXPECT_OK(s);
}

TEST_F(GRPCExecGraphTest, pending_row_batches_closed_connection_no_eos) {
  // Make a GRPC source, but don't initiate the connection.
  // Set timeouts to be 0 so that we definitely time out on connections being established.
  ExecutionGraph e{std::chrono::milliseconds(0), std::chrono::milliseconds(0)};
  auto s = e.Init(schema_.get(), plan_state_.get(), exec_state_.get(), plan_fragment_.get(),
                  /* collect_exec_node_stats */ false);
  ASSERT_OK(s);

  auto grpc_sources = e.grpc_sources();
  ASSERT_EQ(1, grpc_sources.size());
  auto src_id = *(grpc_sources.begin());
  auto grpc_src = static_cast<GRPCSourceNode*>(e.node(src_id).ConsumeValueOrDie());
  grpc_src->set_upstream_initiated_connection();
  auto req1 = std::make_unique<carnotpb::TransferResultChunkRequest>();
  auto req2 = std::make_unique<carnotpb::TransferResultChunkRequest>();

  RowDescriptor output_rd({types::DataType::INT64});
  auto rb1 = RowBatchBuilder(output_rd, 2, /*eow*/ false, /*eos*/ false)
                 .AddColumn<types::Int64Value>({2, 3})
                 .get();
  auto rb2 = RowBatchBuilder(output_rd, 3, /*eow*/ false, /*eos*/ false)
                 .AddColumn<types::Int64Value>({4, 5, 6})
                 .get();

  ASSERT_OK(rb1.ToProto(req1->mutable_query_result()->mutable_row_batch()));
  ASSERT_OK(rb2.ToProto(req2->mutable_query_result()->mutable_row_batch()));

  ASSERT_OK(grpc_src->EnqueueRowBatch(std::move(req1)));
  ASSERT_OK(grpc_src->EnqueueRowBatch(std::move(req2)));
  grpc_src->set_upstream_closed_connection();

  // This shouldn't hang or error out.
  s = e.Execute();
  EXPECT_OK(s);
}

TEST_F(GRPCExecGraphTest, upstream_never_connected_no_active_sources) {
  // Make a GRPC source, but don't initiate the connection.
  // Set timeouts to be 0 so that we definitely time out on connections being established.
  ExecutionGraph e{std::chrono::milliseconds(0), std::chrono::milliseconds(0)};
  auto s = e.Init(schema_.get(), plan_state_.get(), exec_state_.get(), plan_fragment_.get(),
                  /* collect_exec_node_stats */ false);
  ASSERT_OK(s);

  // This shouldn't hang or error out.
  s = e.Execute();
  EXPECT_OK(s);
}

TEST_F(GRPCExecGraphTest, infinite_source_and_error_source) {
  ExecutionGraph e{std::chrono::milliseconds(1), std::chrono::milliseconds(1)};
  e.testing_set_exec_state(exec_state_.get());

  RowDescriptor output_rd({types::DataType::INT64});

  MockSourceNode data_producing_source(output_rd);
  FakePlanNode data_producing_plan_node(1);

  MockSourceNode error_source(output_rd);
  FakePlanNode error_plan_node(2);

  // Setup a source that will continuously produce data, and another that will error
  // and cause the rest of the query to be cancelled.

  EXPECT_CALL(data_producing_source, InitImpl(::testing::_));
  ASSERT_OK(data_producing_source.Init(data_producing_plan_node, output_rd, {}));

  EXPECT_CALL(error_source, InitImpl(::testing::_));
  ASSERT_OK(error_source.Init(error_plan_node, output_rd, {}));

  EXPECT_CALL(data_producing_source, PrepareImpl(::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(Status::OK()));
  EXPECT_CALL(error_source, PrepareImpl(::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(Status::OK()));

  EXPECT_CALL(data_producing_source, OpenImpl(::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(Status::OK()));
  EXPECT_CALL(error_source, OpenImpl(::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(Status::OK()));

  EXPECT_CALL(data_producing_source, NextBatchReady()).WillRepeatedly(::testing::Return(true));
  EXPECT_CALL(error_source, NextBatchReady()).WillRepeatedly(::testing::Return(true));

  EXPECT_CALL(data_producing_source, GenerateNextImpl(::testing::_))
      .WillRepeatedly(::testing::Return(Status::OK()));

  EXPECT_CALL(error_source, GenerateNextImpl(::testing::_))
      .Times(2)
      .WillOnce(::testing::Return(Status::OK()))
      .WillOnce(::testing::Return(error::Internal("o no an error")));

  // Even though the execute query errors out, the query should still call
  // Close() on all of the nodes.
  EXPECT_CALL(data_producing_source, CloseImpl(::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(Status::OK()));
  EXPECT_CALL(error_source, CloseImpl(::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(Status::OK()));

  e.AddNode(10, &data_producing_source);
  e.AddNode(20, &error_source);

  // This should return an error, even though the first source is fully healthy.
  auto s = e.Execute();
  EXPECT_NOT_OK(s);
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
