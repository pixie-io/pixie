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

#include "src/carnot/exec/union_node.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sole.hpp>

#include "src/carnot/exec/test_utils.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf/base.h"
#include "src/carnot/udf/registry.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;
using ::testing::_;
using types::Time64NSValue;
using udf::FunctionContext;

// Cases:
// 1) unordered
// 2) disjoint ranges
// 3) overlapping ranges
// 4) mix of disjoint and overlapping

class UnionNodeTest : public ::testing::Test {
 public:
  UnionNodeTest() {
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

TEST_F(UnionNodeTest, unordered) {
  auto op_proto = planpb::testutils::CreateTestUnionUnorderedPB();
  plan_node_ = plan::UnionOperator::FromProto(op_proto, /*id*/ 1);

  RowDescriptor input_rd_0({types::DataType::STRING, types::DataType::INT64});
  RowDescriptor input_rd_1({types::DataType::INT64, types::DataType::STRING});

  RowDescriptor output_rd({types::DataType::STRING, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<UnionNode, plan::UnionOperator>(
      *plan_node_, output_rd, {input_rd_0, input_rd_1}, exec_state_.get());
  tester.node()->disable_data_flush_timeout();

  tester
      .ConsumeNext(RowBatchBuilder(input_rd_0, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::StringValue>({"ABC", "DEF", "HELLO", "WORLD"})
                       .AddColumn<types::Int64Value>({3, 1, 0, 100})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 4, false, false)
                          .AddColumn<types::StringValue>({"ABC", "DEF", "HELLO", "WORLD"})
                          .AddColumn<types::Int64Value>({3, 1, 0, 100})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd_1, 2, true, true)
                       .AddColumn<types::Int64Value>({-100, -102})
                       .AddColumn<types::StringValue>({"FOO", "BAR"})
                       .get(),
                   1)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 2, false, false)
                          .AddColumn<types::StringValue>({"FOO", "BAR"})
                          .AddColumn<types::Int64Value>({-100, -102})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd_0, 3, true, true)
                       .AddColumn<types::StringValue>({"1", "2", "3"})
                       .AddColumn<types::Int64Value>({4, 5, 6})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 3, true, true)
                          .AddColumn<types::StringValue>({"1", "2", "3"})
                          .AddColumn<types::Int64Value>({4, 5, 6})
                          .get())
      .Close();
}

// non-overlapping time ranges.
TEST_F(UnionNodeTest, ordered_disjoint) {
  auto op_proto = planpb::testutils::CreateTestUnionOrderedPB();
  plan_node_ = plan::UnionOperator::FromProto(op_proto, /*id*/ 1);

  RowDescriptor input_rd_0({types::DataType::STRING, types::DataType::TIME64NS});
  RowDescriptor input_rd_1({types::DataType::TIME64NS, types::DataType::STRING});

  RowDescriptor output_rd({types::DataType::STRING, types::DataType::TIME64NS});

  auto tester = exec::ExecNodeTester<UnionNode, plan::UnionOperator>(
      *plan_node_, output_rd, {input_rd_0, input_rd_1}, exec_state_.get());
  tester.node()->disable_data_flush_timeout();

  tester
      .ConsumeNext(RowBatchBuilder(input_rd_0, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::StringValue>({"A", "B", "C", "D"})
                       .AddColumn<types::Time64NSValue>({0, 1, 2, 3})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_0, 3, false, false)
                       .AddColumn<types::StringValue>({"E", "F", "G"})
                       .AddColumn<types::Time64NSValue>({4, 5, 6})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_1, 2, false, false)
                       .AddColumn<types::Time64NSValue>({10, 11})
                       .AddColumn<types::StringValue>({"Z", "Y"})
                       .get(),
                   1, 1)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::StringValue>({"A", "B", "C", "D", "E"})
                          .AddColumn<types::Time64NSValue>({0, 1, 2, 3, 4})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd_0, 1, true, true)
                       .AddColumn<types::StringValue>({"H"})
                       .AddColumn<types::Time64NSValue>({100})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_1, 4, true, true)
                       .AddColumn<types::Time64NSValue>({20, 25, 30, 40})
                       .AddColumn<types::StringValue>({"X", "W", "V", "U"})
                       .get(),
                   1, 2)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::StringValue>({"F", "G", "Z", "Y", "X"})
                          .AddColumn<types::Time64NSValue>({5, 6, 10, 11, 20})
                          .get())
      .ExpectRowBatch(RowBatchBuilder(output_rd, 4, true, true)
                          .AddColumn<types::StringValue>({"W", "V", "U", "H"})
                          .AddColumn<types::Time64NSValue>({25, 30, 40, 100})
                          .get())
      .Close();
}

// Partially overlapping time ranges.
TEST_F(UnionNodeTest, ordered_partial_overlap_string) {
  auto op_proto = planpb::testutils::CreateTestUnionOrderedPB();
  plan_node_ = plan::UnionOperator::FromProto(op_proto, /*id*/ 1);

  RowDescriptor input_rd_0({types::DataType::STRING, types::DataType::TIME64NS});
  RowDescriptor input_rd_1({types::DataType::TIME64NS, types::DataType::STRING});

  RowDescriptor output_rd({types::DataType::STRING, types::DataType::TIME64NS});

  auto tester = exec::ExecNodeTester<UnionNode, plan::UnionOperator>(
      *plan_node_, output_rd, {input_rd_0, input_rd_1}, exec_state_.get());
  tester.node()->disable_data_flush_timeout();

  tester
      .ConsumeNext(RowBatchBuilder(input_rd_0, 2, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::StringValue>({"A", "B"})
                       .AddColumn<types::Time64NSValue>({0, 1})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_1, 2, false, false)
                       .AddColumn<types::Time64NSValue>({1, 2})
                       .AddColumn<types::StringValue>({"b", "c"})
                       .get(),
                   1, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_0, 8, true, true)
                       .AddColumn<types::StringValue>({"E", "F", "G", "H", "I", "J", "K", "L"})
                       .AddColumn<types::Time64NSValue>({4, 5, 6, 7, 8, 9, 10, 11})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_1, 2, false, false)
                       .AddColumn<types::Time64NSValue>({4, 5})
                       .AddColumn<types::StringValue>({"e", "f"})
                       .get(),
                   1, 1)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::StringValue>({"A", "B", "b", "c", "E"})
                          .AddColumn<types::Time64NSValue>({0, 1, 1, 2, 4})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd_1, 1, true, true)
                       .AddColumn<types::Time64NSValue>({11})
                       .AddColumn<types::StringValue>({"l"})
                       .get(),
                   1, 2)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::StringValue>({"e", "F", "f", "G", "H"})
                          .AddColumn<types::Time64NSValue>({4, 5, 5, 6, 7})
                          .get())
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, true, true)
                          .AddColumn<types::StringValue>({"I", "J", "K", "L", "l"})
                          .AddColumn<types::Time64NSValue>({8, 9, 10, 11, 11})
                          .get())
      .Close();
}

// // overlapping time ranges
TEST_F(UnionNodeTest, ordered_full_overlap) {
  auto op_proto = planpb::testutils::CreateTestUnionOrderedPB();
  plan_node_ = plan::UnionOperator::FromProto(op_proto, /*id*/ 1);

  RowDescriptor input_rd_0({types::DataType::STRING, types::DataType::TIME64NS});
  RowDescriptor input_rd_1({types::DataType::TIME64NS, types::DataType::STRING});

  RowDescriptor output_rd({types::DataType::STRING, types::DataType::TIME64NS});

  auto tester = exec::ExecNodeTester<UnionNode, plan::UnionOperator>(
      *plan_node_, output_rd, {input_rd_0, input_rd_1}, exec_state_.get());
  tester.node()->disable_data_flush_timeout();

  tester
      .ConsumeNext(RowBatchBuilder(input_rd_0, 5, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::StringValue>({"A", "B", "C", "D", "E"})
                       .AddColumn<types::Time64NSValue>({0, 1, 2, 3, 4})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_1, 5, false, false)
                       .AddColumn<types::Time64NSValue>({0, 1, 2, 3, 4})
                       .AddColumn<types::StringValue>({"a", "b", "c", "d", "e"})
                       .get(),
                   1, 1)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::StringValue>({"A", "a", "B", "b", "C"})
                          .AddColumn<types::Time64NSValue>({0, 0, 1, 1, 2})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd_0, 5, false, false)
                       .AddColumn<types::StringValue>({"F", "G", "H", "I", "J"})
                       .AddColumn<types::Time64NSValue>({5, 6, 7, 8, 9})
                       .get(),
                   0, 1)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::StringValue>({"c", "D", "d", "E", "e"})
                          .AddColumn<types::Time64NSValue>({2, 3, 3, 4, 4})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd_1, 5, true, true)
                       .AddColumn<types::Time64NSValue>({5, 6, 7, 8, 9})
                       .AddColumn<types::StringValue>({"f", "g", "h", "i", "j"})
                       .get(),
                   1, 1)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::StringValue>({"F", "f", "G", "g", "H"})
                          .AddColumn<types::Time64NSValue>({5, 5, 6, 6, 7})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd_0, 5, true, true)
                       .AddColumn<types::StringValue>({"K", "L", "M", "N", "O"})
                       .AddColumn<types::Time64NSValue>({10, 11, 12, 13, 14})
                       .get(),
                   0, 2)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::StringValue>({"h", "I", "i", "J", "j"})
                          .AddColumn<types::Time64NSValue>({7, 8, 8, 9, 9})
                          .get())
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, true, true)
                          .AddColumn<types::StringValue>({"K", "L", "M", "N", "O"})
                          .AddColumn<types::Time64NSValue>({10, 11, 12, 13, 14})
                          .get())
      .Close();
}

TEST_F(UnionNodeTest, no_rows_parent) {
  auto op_proto = planpb::testutils::CreateTestUnionOrderedPB();
  plan_node_ = plan::UnionOperator::FromProto(op_proto, /*id*/ 1);

  RowDescriptor input_rd_0({types::DataType::STRING, types::DataType::TIME64NS});
  RowDescriptor input_rd_1({types::DataType::TIME64NS, types::DataType::STRING});

  RowDescriptor output_rd({types::DataType::STRING, types::DataType::TIME64NS});

  auto tester = exec::ExecNodeTester<UnionNode, plan::UnionOperator>(
      *plan_node_, output_rd, {input_rd_0, input_rd_1}, exec_state_.get());
  tester.node()->disable_data_flush_timeout();

  tester
      .ConsumeNext(RowBatchBuilder(input_rd_0, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::StringValue>({"A", "B", "C", "D"})
                       .AddColumn<types::Time64NSValue>({0, 1, 2, 3})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_1, 0, true, true)
                       .AddColumn<types::Time64NSValue>({})
                       .AddColumn<types::StringValue>({})
                       .get(),
                   1, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_0, 1, true, true)
                       .AddColumn<types::StringValue>({"H"})
                       .AddColumn<types::Time64NSValue>({100})
                       .get(),
                   0, 1)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, true, true)
                          .AddColumn<types::StringValue>({"A", "B", "C", "D", "H"})
                          .AddColumn<types::Time64NSValue>({0, 1, 2, 3, 100})
                          .get())
      .Close();
}

TEST_F(UnionNodeTest, many_empty_rbs) {
  auto op_proto = planpb::testutils::CreateTestUnionOrderedPB();
  plan_node_ = plan::UnionOperator::FromProto(op_proto, /*id*/ 1);

  RowDescriptor input_rd_0({types::DataType::STRING, types::DataType::TIME64NS});
  RowDescriptor input_rd_1({types::DataType::TIME64NS, types::DataType::STRING});

  RowDescriptor output_rd({types::DataType::STRING, types::DataType::TIME64NS});

  auto tester = exec::ExecNodeTester<UnionNode, plan::UnionOperator>(
      *plan_node_, output_rd, {input_rd_0, input_rd_1}, exec_state_.get());
  tester.node()->disable_data_flush_timeout();

  tester
      .ConsumeNext(RowBatchBuilder(input_rd_0, 2, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::StringValue>({"A", "B"})
                       .AddColumn<types::Time64NSValue>({0, 1})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_0, 0, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::StringValue>({})
                       .AddColumn<types::Time64NSValue>({})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_0, 0, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::StringValue>({})
                       .AddColumn<types::Time64NSValue>({})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_1, 2, false, false)
                       .AddColumn<types::Time64NSValue>({1, 2})
                       .AddColumn<types::StringValue>({"b", "c"})
                       .get(),
                   1, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_1, 0, false, false)
                       .AddColumn<types::Time64NSValue>({})
                       .AddColumn<types::StringValue>({})
                       .get(),
                   1, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_0, 8, true, true)
                       .AddColumn<types::StringValue>({"E", "F", "G", "H", "I", "J", "K", "L"})
                       .AddColumn<types::Time64NSValue>({4, 5, 6, 7, 8, 9, 10, 11})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_1, 2, false, false)
                       .AddColumn<types::Time64NSValue>({4, 5})
                       .AddColumn<types::StringValue>({"e", "f"})
                       .get(),
                   1, 1)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::StringValue>({"A", "B", "b", "c", "E"})
                          .AddColumn<types::Time64NSValue>({0, 1, 1, 2, 4})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd_1, 1, false, false)
                       .AddColumn<types::Time64NSValue>({11})
                       .AddColumn<types::StringValue>({"l"})
                       .get(),
                   1, 2)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::StringValue>({"e", "F", "f", "G", "H"})
                          .AddColumn<types::Time64NSValue>({4, 5, 5, 6, 7})
                          .get())
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::StringValue>({"I", "J", "K", "L", "l"})
                          .AddColumn<types::Time64NSValue>({8, 9, 10, 11, 11})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd_1, 0, false, false)
                       .AddColumn<types::Time64NSValue>({})
                       .AddColumn<types::StringValue>({})
                       .get(),
                   1, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_1, 0, true, true)
                       .AddColumn<types::Time64NSValue>({})
                       .AddColumn<types::StringValue>({})
                       .get(),
                   1, 1)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 0, true, true)
                          .AddColumn<types::StringValue>({})
                          .AddColumn<types::Time64NSValue>({})
                          .get())
      .Close();
}

TEST_F(UnionNodeTest, all_multiple_empty_rbs) {
  auto op_proto = planpb::testutils::CreateTestUnionOrderedPB();
  plan_node_ = plan::UnionOperator::FromProto(op_proto, /*id*/ 1);

  RowDescriptor input_rd_0({types::DataType::STRING, types::DataType::TIME64NS});
  RowDescriptor input_rd_1({types::DataType::TIME64NS, types::DataType::STRING});

  RowDescriptor output_rd({types::DataType::STRING, types::DataType::TIME64NS});

  // Multiple empty
  auto tester = exec::ExecNodeTester<UnionNode, plan::UnionOperator>(
      *plan_node_, output_rd, {input_rd_0, input_rd_1}, exec_state_.get());
  tester.node()->disable_data_flush_timeout();

  tester
      .ConsumeNext(RowBatchBuilder(input_rd_0, 0, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::StringValue>({})
                       .AddColumn<types::Time64NSValue>({})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_0, 0, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::StringValue>({})
                       .AddColumn<types::Time64NSValue>({})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_1, 0, /*eow*/ true, /*eos*/ true)
                       .AddColumn<types::Time64NSValue>({})
                       .AddColumn<types::StringValue>({})
                       .get(),
                   1, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_0, 0, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::StringValue>({})
                       .AddColumn<types::Time64NSValue>({})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_0, 0, /*eow*/ true, /*eos*/ true)
                       .AddColumn<types::StringValue>({})
                       .AddColumn<types::Time64NSValue>({})
                       .get(),
                   0, 1)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 0, true, true)
                          .AddColumn<types::StringValue>({})
                          .AddColumn<types::Time64NSValue>({})
                          .get())
      .Close();
}

TEST_F(UnionNodeTest, all_single_empty_rbs) {
  auto op_proto = planpb::testutils::CreateTestUnionOrderedPB();
  plan_node_ = plan::UnionOperator::FromProto(op_proto, /*id*/ 1);

  RowDescriptor input_rd_0({types::DataType::STRING, types::DataType::TIME64NS});
  RowDescriptor input_rd_1({types::DataType::TIME64NS, types::DataType::STRING});

  RowDescriptor output_rd({types::DataType::STRING, types::DataType::TIME64NS});

  // One from each
  auto tester = exec::ExecNodeTester<UnionNode, plan::UnionOperator>(
      *plan_node_, output_rd, {input_rd_0, input_rd_1}, exec_state_.get());
  tester.node()->disable_data_flush_timeout();

  tester
      .ConsumeNext(RowBatchBuilder(input_rd_1, 0, /*eow*/ true, /*eos*/ true)
                       .AddColumn<types::Time64NSValue>({})
                       .AddColumn<types::StringValue>({})
                       .get(),
                   1, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_0, 0, /*eow*/ true, /*eos*/ true)
                       .AddColumn<types::StringValue>({})
                       .AddColumn<types::Time64NSValue>({})
                       .get(),
                   0, 1)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 0, true, true)
                          .AddColumn<types::StringValue>({})
                          .AddColumn<types::Time64NSValue>({})
                          .get())
      .Close();
}

TEST_F(UnionNodeTest, end_on_empty_rb) {
  auto op_proto = planpb::testutils::CreateTestUnionOrderedPB();
  plan_node_ = plan::UnionOperator::FromProto(op_proto, /*id*/ 1);
  RowDescriptor input_rd_0({types::DataType::STRING, types::DataType::TIME64NS});
  RowDescriptor input_rd_1({types::DataType::TIME64NS, types::DataType::STRING});
  RowDescriptor output_rd({types::DataType::STRING, types::DataType::TIME64NS});
  // Multiple empty
  auto tester = exec::ExecNodeTester<UnionNode, plan::UnionOperator>(
      *plan_node_, output_rd, {input_rd_0, input_rd_1}, exec_state_.get());
  tester.node()->disable_data_flush_timeout();

  tester
      .ConsumeNext(RowBatchBuilder(input_rd_0, 1, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::StringValue>({"hello"})
                       .AddColumn<types::Time64NSValue>({123})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_0, 0, /*eow*/ true, /*eos*/ true)
                       .AddColumn<types::StringValue>({})
                       .AddColumn<types::Time64NSValue>({})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_1, 0, /*eow*/ true, /*eos*/ true)
                       .AddColumn<types::Time64NSValue>({})
                       .AddColumn<types::StringValue>({})
                       .get(),
                   1, 1)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 1, true, true)
                          .AddColumn<types::StringValue>({"hello"})
                          .AddColumn<types::Time64NSValue>({123})
                          .get())
      .Close();
}

TEST_F(UnionNodeTest, ordered_timeout_not_hit) {
  auto op_proto = planpb::testutils::CreateTestUnionOrderedPB();
  plan_node_ = plan::UnionOperator::FromProto(op_proto, /*id*/ 1);

  RowDescriptor input_rd_0({types::DataType::STRING, types::DataType::TIME64NS});
  RowDescriptor input_rd_1({types::DataType::TIME64NS, types::DataType::STRING});

  RowDescriptor output_rd({types::DataType::STRING, types::DataType::TIME64NS});

  auto tester = exec::ExecNodeTester<UnionNode, plan::UnionOperator>(
      *plan_node_, output_rd, {input_rd_0, input_rd_1}, exec_state_.get());

  // 0 timeout, row batches should be emitted as soon as they can.
  tester.node()->disable_data_flush_timeout();

  tester
      .ConsumeNext(RowBatchBuilder(input_rd_0, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::StringValue>({"A", "B", "C", "D"})
                       .AddColumn<types::Time64NSValue>({0, 1, 2, 3})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_1, 2, false, false)
                       .AddColumn<types::Time64NSValue>({10, 11})
                       .AddColumn<types::StringValue>({"Z", "Y"})
                       .get(),
                   1, 1)
      .ConsumeNext(RowBatchBuilder(input_rd_0, 3, false, false)
                       .AddColumn<types::StringValue>({"E", "F", "G"})
                       .AddColumn<types::Time64NSValue>({4, 5, 6})
                       .get(),
                   0, 0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::StringValue>({"A", "B", "C", "D", "E"})
                          .AddColumn<types::Time64NSValue>({0, 1, 2, 3, 4})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd_0, 1, true, true)
                       .AddColumn<types::StringValue>({"H"})
                       .AddColumn<types::Time64NSValue>({100})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_1, 4, true, true)
                       .AddColumn<types::Time64NSValue>({20, 25, 30, 40})
                       .AddColumn<types::StringValue>({"X", "W", "V", "U"})
                       .get(),
                   1, 2)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::StringValue>({"F", "G", "Z", "Y", "X"})
                          .AddColumn<types::Time64NSValue>({5, 6, 10, 11, 20})
                          .get())
      .ExpectRowBatch(RowBatchBuilder(output_rd, 4, true, true)
                          .AddColumn<types::StringValue>({"W", "V", "U", "H"})
                          .AddColumn<types::Time64NSValue>({25, 30, 40, 100})
                          .get())
      .Close();
}

TEST_F(UnionNodeTest, ordered_timeout_hit) {
  auto op_proto = planpb::testutils::CreateTestUnionOrderedPB();
  plan_node_ = plan::UnionOperator::FromProto(op_proto, /*id*/ 1);

  RowDescriptor input_rd_0({types::DataType::STRING, types::DataType::TIME64NS});
  RowDescriptor input_rd_1({types::DataType::TIME64NS, types::DataType::STRING});

  RowDescriptor output_rd({types::DataType::STRING, types::DataType::TIME64NS});

  auto tester = exec::ExecNodeTester<UnionNode, plan::UnionOperator>(
      *plan_node_, output_rd, {input_rd_0, input_rd_1}, exec_state_.get());

  // -1 timeout, row batches should be emitted as soon as they can.
  tester.node()->set_data_flush_timeout(std::chrono::milliseconds(-1));

  tester
      .ConsumeNext(RowBatchBuilder(input_rd_0, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::StringValue>({"A", "B", "C", "D"})
                       .AddColumn<types::Time64NSValue>({0, 1, 2, 3})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_1, 2, false, false)
                       .AddColumn<types::Time64NSValue>({10, 11})
                       .AddColumn<types::StringValue>({"Z", "Y"})
                       .get(),
                   1, 1)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 4, false, false)
                          .AddColumn<types::StringValue>({"A", "B", "C", "D"})
                          .AddColumn<types::Time64NSValue>({0, 1, 2, 3})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd_0, 3, false, false)
                       .AddColumn<types::StringValue>({"E", "F", "G"})
                       .AddColumn<types::Time64NSValue>({4, 5, 6})
                       .get(),
                   0, 1)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 3, false, false)
                          .AddColumn<types::StringValue>({"E", "F", "G"})
                          .AddColumn<types::Time64NSValue>({4, 5, 6})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd_0, 1, true, true)
                       .AddColumn<types::StringValue>({"H"})
                       .AddColumn<types::Time64NSValue>({100})
                       .get(),
                   0, 1)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 2, false, false)
                          .AddColumn<types::StringValue>({"Z", "Y"})
                          .AddColumn<types::Time64NSValue>({10, 11})
                          .get())
      .ConsumeNext(RowBatchBuilder(input_rd_1, 4, true, true)
                       .AddColumn<types::Time64NSValue>({20, 25, 30, 40})
                       .AddColumn<types::StringValue>({"X", "W", "V", "U"})
                       .get(),
                   1, 1)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, true, true)
                          .AddColumn<types::StringValue>({"X", "W", "V", "U", "H"})
                          .AddColumn<types::Time64NSValue>({20, 25, 30, 40, 100})
                          .get())
      .Close();
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
