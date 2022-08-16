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

#include "src/carnot/exec/limit_node.h"

#include <memory>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sole.hpp>

#include "src/carnot/exec/test_utils.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf/base.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;
using ::testing::_;
using types::Int64Value;
using udf::FunctionContext;

class LimitNodeTest : public ::testing::Test {
 public:
  LimitNodeTest() {
    auto op_proto = planpb::testutils::CreateTestLimit1PB();
    plan_node_ = plan::LimitOperator::FromProto(op_proto, 1);

    func_registry_ = std::make_unique<udf::Registry>("test_registry");

    auto table_store = std::make_shared<table_store::TableStore>();

    exec_state_ = std::make_unique<ExecState>(func_registry_.get(), table_store,
                                              MockResultSinkStubGenerator, MockMetricsStubGenerator,
                                              MockTraceStubGenerator, sole::uuid4(), nullptr);
  }

 protected:
  std::unique_ptr<plan::Operator> plan_node_;
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::Registry> func_registry_;
};

TEST_F(LimitNodeTest, single_batch) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<LimitNode, plan::LimitOperator>(*plan_node_, output_rd,
                                                                     {input_rd}, exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 12, /*eow*/ true, /*eos*/ true)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9, 12, 15, 1, 3, 6, 9, 12, 15})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 10, true, true)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6, 1, 2, 3, 4})
                          .AddColumn<types::Int64Value>({1, 3, 6, 9, 12, 15, 1, 3, 6, 9})
                          .get())
      .Close();
}

TEST_F(LimitNodeTest, single_empty_batch) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<LimitNode, plan::LimitOperator>(*plan_node_, output_rd,
                                                                     {input_rd}, exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 0, /*eow*/ true, /*eos*/ true)
                       .AddColumn<types::Int64Value>({})
                       .AddColumn<types::Int64Value>({})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 0, true, true)
                          .AddColumn<types::Int64Value>({})
                          .AddColumn<types::Int64Value>({})
                          .get())
      .Close();
}

TEST_F(LimitNodeTest, limit_zero) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto op_proto = planpb::testutils::CreateTestLimit1PB();
  op_proto.mutable_limit_op()->set_limit(0);
  plan_node_ = plan::LimitOperator::FromProto(op_proto, 1);
  auto tester = exec::ExecNodeTester<LimitNode, plan::LimitOperator>(*plan_node_, output_rd,
                                                                     {input_rd}, exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 12, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9, 12, 15, 1, 3, 6, 9, 12, 15})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 0, true, true)
                          .AddColumn<types::Int64Value>({})
                          .AddColumn<types::Int64Value>({})
                          .get())
      .Close();
}

TEST_F(LimitNodeTest, limit_negative) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto op_proto = planpb::testutils::CreateTestLimit1PB();
  op_proto.mutable_limit_op()->set_limit(-5);
  plan_node_ = plan::LimitOperator::FromProto(op_proto, 1);
  auto tester = exec::ExecNodeTester<LimitNode, plan::LimitOperator>(*plan_node_, output_rd,
                                                                     {input_rd}, exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 12, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9, 12, 15, 1, 3, 6, 9, 12, 15})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 0, true, true)
                          .AddColumn<types::Int64Value>({})
                          .AddColumn<types::Int64Value>({})
                          .get())
      .Close();
}

TEST_F(LimitNodeTest, single_batch_exact_boundary) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<LimitNode, plan::LimitOperator>(*plan_node_, output_rd,
                                                                     {input_rd}, exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 10, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6, 1, 2, 3, 4})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9, 12, 15, 1, 3, 6, 9})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 10, true, true)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6, 1, 2, 3, 4})
                          .AddColumn<types::Int64Value>({1, 3, 6, 9, 12, 15, 1, 3, 6, 9})
                          .get())
      .Close();
}

TEST_F(LimitNodeTest, limits_records_split) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<LimitNode, plan::LimitOperator>(*plan_node_, output_rd,
                                                                     {input_rd}, exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 6, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9, 12, 15})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 6, false, false)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6})
                          .AddColumn<types::Int64Value>({1, 3, 6, 9, 12, 15})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd, 6, true, true)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6})
                       .AddColumn<types::Int64Value>({1, 4, 6, 8, 10, 12})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 4, true, true)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4})
                          .AddColumn<types::Int64Value>({1, 4, 6, 8})
                          .get(),
                      0)
      .Close();
}

TEST_F(LimitNodeTest, limits_exact_boundary) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<LimitNode, plan::LimitOperator>(*plan_node_, output_rd,
                                                                     {input_rd}, exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 6, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9, 12, 15})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 6, false, false)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6})
                          .AddColumn<types::Int64Value>({1, 3, 6, 9, 12, 15})
                          .get(),
                      0)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true, true)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4})
                       .AddColumn<types::Int64Value>({1, 4, 6, 8})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 4, true, true)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4})
                          .AddColumn<types::Int64Value>({1, 4, 6, 8})
                          .get())
      .Close();
}

TEST_F(LimitNodeTest, child_fail) {
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<LimitNode, plan::LimitOperator>(*plan_node_, output_rd,
                                                                     {input_rd}, exec_state_.get());
  tester.ConsumeNextShouldFail(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                                   .AddColumn<types::Int64Value>({1, 2, 3, 4})
                                   .AddColumn<types::Int64Value>({1, 3, 6, 9})
                                   .get(),
                               0, error::InvalidArgument("args"));
}

TEST_F(LimitNodeTest, drop_input_columns) {
  auto op_proto = planpb::testutils::CreateTestDropLimit1PB();
  auto drop_limit = plan::LimitOperator::FromProto(op_proto, 1);

  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64, types::INT64});
  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<LimitNode, plan::LimitOperator>(*drop_limit, output_rd,
                                                                     {input_rd}, exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 12, /*eow*/ true, /*eos*/ true)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 5, 6})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9, 12, 15, 1, 3, 6, 9, 12, 15})
                       .AddColumn<types::Int64Value>({1, 4, 8, 12, 16, 20, 1, 4, 8, 12, 16, 20})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 10, true, true)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6, 1, 2, 3, 4})
                          .AddColumn<types::Int64Value>({1, 4, 8, 12, 16, 20, 1, 4, 8, 12})
                          .get())
      .Close();
}

TEST_F(LimitNodeTest, drop_input_columns_fewer_than_limit) {
  auto op_proto = planpb::testutils::CreateTestDropLimit1PB();
  auto drop_limit = plan::LimitOperator::FromProto(op_proto, 1);

  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64, types::INT64});
  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<LimitNode, plan::LimitOperator>(*drop_limit, output_rd,
                                                                     {input_rd}, exec_state_.get());
  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 8, /*eow*/ true, /*eos*/ true)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6, 1, 2})
                       .AddColumn<types::Int64Value>({1, 3, 6, 9, 12, 15, 1, 3})
                       .AddColumn<types::Int64Value>({1, 4, 8, 12, 16, 20, 1, 4})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 8, true, true)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6, 1, 2})
                          .AddColumn<types::Int64Value>({1, 4, 8, 12, 16, 20, 1, 4})
                          .get())
      .Close();
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
