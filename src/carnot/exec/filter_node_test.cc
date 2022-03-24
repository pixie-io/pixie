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

#include "src/carnot/exec/filter_node.h"

#include <sole.hpp>

#include "src/carnot/exec/test_utils.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;
using ::testing::_;
using types::Int64Value;
using udf::FunctionContext;

// TOOD(zasgar): refactor these into test udfs.
class EqUDF : public udf::ScalarUDF {
 public:
  types::BoolValue Exec(FunctionContext*, types::Int64Value v1, types::Int64Value v2) {
    return v1.val == v2.val;
  }
};

class StrEqUDF : public udf::ScalarUDF {
 public:
  types::BoolValue Exec(FunctionContext*, types::StringValue v1, types::StringValue v2) {
    return v1 == v2;
  }
};

class FilterNodeTest : public ::testing::Test {
 public:
  FilterNodeTest() {
    func_registry_ = std::make_unique<udf::Registry>("test_registry");
    EXPECT_OK(func_registry_->Register<EqUDF>("eq"));
    EXPECT_OK(func_registry_->Register<StrEqUDF>("eq"));
    auto table_store = std::make_shared<table_store::TableStore>();

    exec_state_ = std::make_unique<ExecState>(func_registry_.get(), table_store,
                                              MockResultSinkStubGenerator, MockMetricsStubGenerator,
                                              MockTraceStubGenerator, sole::uuid4(), nullptr);
    EXPECT_OK(exec_state_->AddScalarUDF(
        0, "eq", std::vector<types::DataType>({types::DataType::INT64, types::DataType::INT64})));
    EXPECT_OK(exec_state_->AddScalarUDF(
        1, "eq", std::vector<types::DataType>({types::DataType::STRING, types::DataType::STRING})));
  }

 protected:
  std::unique_ptr<plan::Operator> plan_node_;
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::Registry> func_registry_;
};

TEST_F(FilterNodeTest, basic) {
  auto op_proto = planpb::testutils::CreateTestFilterTwoCols();
  plan_node_ = plan::FilterOperator::FromProto(op_proto, /*id*/ 1);

  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64, types::DataType::STRING});
  RowDescriptor output_rd(
      {types::DataType::INT64, types::DataType::INT64, types::DataType::STRING});

  auto tester = exec::ExecNodeTester<FilterNode, plan::FilterOperator>(
      *plan_node_, output_rd, {input_rd}, exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 1, 3, 4})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9})
                       .AddColumn<types::StringValue>({"ABC", "DEF", "HELLO", "WORLD"})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 2, false, false)
                          .AddColumn<types::Int64Value>({1, 1})
                          .AddColumn<types::Int64Value>({1, 3})
                          .AddColumn<types::StringValue>({"ABC", "DEF"})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd, 3, true, true)
                       .AddColumn<types::Int64Value>({1, 2, 3})
                       .AddColumn<types::Int64Value>({1, 4, 6})
                       .AddColumn<types::StringValue>({"Hello", "world", "now"})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 1, true, true)
                          .AddColumn<types::Int64Value>({1})
                          .AddColumn<types::Int64Value>({1})
                          .AddColumn<types::StringValue>({"Hello"})
                          .get())
      .Close();
}

TEST_F(FilterNodeTest, column_selection) {
  auto op_proto = planpb::testutils::CreateTestFilterTwoColsColumnSelection();
  plan_node_ = plan::FilterOperator::FromProto(op_proto, /*id*/ 1);

  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64, types::DataType::STRING});
  RowDescriptor output_rd({types::DataType::INT64});

  auto tester = exec::ExecNodeTester<FilterNode, plan::FilterOperator>(
      *plan_node_, output_rd, {input_rd}, exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 1, 3, 4})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9})
                       .AddColumn<types::StringValue>({"ABC", "DEF", "HELLO", "WORLD"})
                       .get(),
                   0)
      .ExpectRowBatch(
          RowBatchBuilder(output_rd, 2, false, false).AddColumn<types::Int64Value>({1, 3}).get())
      .ConsumeNext(RowBatchBuilder(input_rd, 3, true, true)
                       .AddColumn<types::Int64Value>({1, 2, 3})
                       .AddColumn<types::Int64Value>({1, 4, 6})
                       .AddColumn<types::StringValue>({"Hello", "world", "now"})
                       .get(),
                   0)
      .ExpectRowBatch(
          RowBatchBuilder(output_rd, 1, true, true).AddColumn<types::Int64Value>({1}).get())
      .Close();
}

TEST_F(FilterNodeTest, zero_row_row_batch) {
  auto op_proto = planpb::testutils::CreateTestFilterTwoCols();
  plan_node_ = plan::FilterOperator::FromProto(op_proto, /*id*/ 1);

  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64, types::DataType::STRING});
  RowDescriptor output_rd(
      {types::DataType::INT64, types::DataType::INT64, types::DataType::STRING});

  auto tester = exec::ExecNodeTester<FilterNode, plan::FilterOperator>(
      *plan_node_, output_rd, {input_rd}, exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 1, 3, 4})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9})
                       .AddColumn<types::StringValue>({"ABC", "DEF", "HELLO", "WORLD"})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 2, false, false)
                          .AddColumn<types::Int64Value>({1, 1})
                          .AddColumn<types::Int64Value>({1, 3})
                          .AddColumn<types::StringValue>({"ABC", "DEF"})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd, 0, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({})
                       .AddColumn<types::Int64Value>({})
                       .AddColumn<types::StringValue>({})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 0, false, false)
                          .AddColumn<types::Int64Value>({})
                          .AddColumn<types::Int64Value>({})
                          .AddColumn<types::StringValue>({})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd, 3, true, true)
                       .AddColumn<types::Int64Value>({1, 2, 3})
                       .AddColumn<types::Int64Value>({1, 4, 6})
                       .AddColumn<types::StringValue>({"Hello", "world", "now"})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 1, true, true)
                          .AddColumn<types::Int64Value>({1})
                          .AddColumn<types::Int64Value>({1})
                          .AddColumn<types::StringValue>({"Hello"})
                          .get())
      .Close();
}

TEST_F(FilterNodeTest, string_pred) {
  auto op_proto = planpb::testutils::CreateTestFilterTwoColsString();
  plan_node_ = plan::FilterOperator::FromProto(op_proto, /*id*/ 1);

  RowDescriptor input_rd({types::DataType::STRING, types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd(
      {types::DataType::STRING, types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<FilterNode, plan::FilterOperator>(
      *plan_node_, output_rd, {input_rd}, exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::StringValue>({"A", "B", "A", "D"})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9})
                       .AddColumn<types::Int64Value>({2, 4, 7, 10})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 2, false, false)
                          .AddColumn<types::StringValue>({"A", "A"})
                          .AddColumn<types::Int64Value>({1, 6})
                          .AddColumn<types::Int64Value>({2, 7})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd, 3, true, true)
                       .AddColumn<types::StringValue>({"C", "B", "A"})
                       .AddColumn<types::Int64Value>({1, 4, 6})
                       .AddColumn<types::Int64Value>({2, 5, 7})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 1, true, true)
                          .AddColumn<types::StringValue>({"A"})
                          .AddColumn<types::Int64Value>({6})
                          .AddColumn<types::Int64Value>({7})
                          .get())
      .Close();
}

TEST_F(FilterNodeTest, child_fail) {
  auto op_proto = planpb::testutils::CreateTestFilterTwoCols();
  plan_node_ = plan::FilterOperator::FromProto(op_proto, /*id*/ 1);

  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<FilterNode, plan::FilterOperator>(
      *plan_node_, output_rd, {input_rd}, exec_state_.get());
  tester.ConsumeNextShouldFail(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                                   .AddColumn<types::Int64Value>({1, 2, 3, 4})
                                   .AddColumn<types::Int64Value>({1, 3, 6, 9})
                                   .AddColumn<types::Int64Value>({1, 3, 6, 9})
                                   .get(),
                               0, error::InvalidArgument("args"));
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
