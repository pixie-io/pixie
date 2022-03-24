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

#include "src/carnot/exec/equijoin_node.h"

#include <absl/strings/substitute.h>
#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <sole.hpp>

#include "src/carnot/exec/test_utils.h"
#include "src/carnot/planpb/plan.pb.h"
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

// Cases tested:
// 1) time ordered inner join (time col = right table, all batches from probe first)
// 2) time ordered left join (time col = left table, batches interleaved)
// 3) non-time ordered full outer join (all batches from build first)
// 4) non-time ordered no matches inner join
// 5) non-time ordered many matches per key inner join

class JoinNodeTest : public ::testing::Test {
 public:
  JoinNodeTest() {
    func_registry_ = std::make_unique<udf::Registry>("test_registry");
    auto table_store = std::make_shared<table_store::TableStore>();
    exec_state_ = std::make_unique<ExecState>(func_registry_.get(), table_store,
                                              MockResultSinkStubGenerator, MockMetricsStubGenerator,
                                              MockTraceStubGenerator, sole::uuid4(), nullptr);
  }

 protected:
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::Registry> func_registry_;
};

std::unique_ptr<plan::Operator> PlanNodeFromPbtxt(const std::string& pbtxt) {
  planpb::Operator op_pb;
  EXPECT_TRUE(google::protobuf::TextFormat::MergeFromString(
      absl::Substitute(planpb::testutils::kOperatorProtoTmpl, "JOIN_OPERATOR", "join_op", pbtxt),
      &op_pb));
  return plan::JoinOperator::FromProto(op_pb, 1);
}

TEST_F(JoinNodeTest, ordered_inner_join) {
  // time_ from right table, all batches from probe (right) first.
  // Left table input: [left_0:Int, left_1:Float]
  // Right table input: [time_:Time64Ns, right_1:Int]
  // Output table: [left_1:Float, right_1:Int, time_:Time64Ns]
  // Left join on left_0=right_1.
  const char* proto = R"(
    type: INNER
    equality_conditions {
      left_column_index: 0
      right_column_index: 1
    }
    output_columns: {
      parent_index: 0
      column_index: 1
    }
    output_columns: {
      parent_index: 1
      column_index: 1
    }
    output_columns: {
      parent_index: 1
      column_index: 0
    }
    column_names: "left_1"
    column_names: "right_1"
    column_names: "time_"
    rows_per_batch: 5
  )";

  auto plan_node = PlanNodeFromPbtxt(proto);
  // Left
  RowDescriptor input_rd_0({types::DataType::INT64, types::DataType::FLOAT64});
  // Right
  RowDescriptor input_rd_1({types::DataType::TIME64NS, types::DataType::INT64});
  // Right[1], Left[1], Left[0]
  RowDescriptor output_rd(
      {types::DataType::FLOAT64, types::DataType::INT64, types::DataType::TIME64NS});
  auto tester = exec::ExecNodeTester<EquijoinNode, plan::JoinOperator>(
      *plan_node, output_rd, {input_rd_0, input_rd_1}, exec_state_.get());

  tester
      // Probe
      .ConsumeNext(RowBatchBuilder(input_rd_1, 4, false, false)
                       .AddColumn<types::Time64NSValue>({10, 20, 30, 31})
                       .AddColumn<types::Int64Value>({1, 2, 3, 3})
                       .get(),
                   1, 0)
      // Probe
      .ConsumeNext(RowBatchBuilder(input_rd_1, 3, true, true)
                       .AddColumn<types::Time64NSValue>({101, 150, 190})
                       .AddColumn<types::Int64Value>({1, 5, 9})
                       .get(),
                   1, 0)
      // Build
      .ConsumeNext(RowBatchBuilder(input_rd_0, 3, false, false)
                       .AddColumn<types::Int64Value>({1, 2, 2})
                       .AddColumn<types::Float64Value>({1.0, 2.0, 2.1})
                       .get(),
                   0, 0)
      // Build
      .ConsumeNext(RowBatchBuilder(input_rd_0, 3, true, true)
                       .AddColumn<types::Int64Value>({9, 1, 1})
                       .AddColumn<types::Float64Value>({9.0, 1.1, 1.2})
                       .get(),
                   0, 2)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::Float64Value>({1.0, 1.1, 1.2, 2.0, 2.1})
                          .AddColumn<types::Int64Value>({1, 1, 1, 2, 2})
                          .AddColumn<types::Time64NSValue>({10, 10, 10, 20, 20})
                          .get(),
                      true)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 4, true, true)
                          .AddColumn<types::Float64Value>({1.0, 1.1, 1.2, 9.0})
                          .AddColumn<types::Int64Value>({1, 1, 1, 9})
                          .AddColumn<types::Time64NSValue>({101, 101, 101, 190})
                          .get(),
                      true)
      .Close();
}

TEST_F(JoinNodeTest, ordered_left_join) {
  // time_ from left (probe) table, batches interleaved
  // Left table input: [time_:Time64Ns, left_1:Int]
  // Right table input: [right_0:Int, right_1:Float]
  // Output table: [right_1:Float, left_1:Int, time_:Time64Ns]
  // Left join on left_1=right_0.
  const char* proto = R"(
    type: LEFT_OUTER
    equality_conditions {
      left_column_index: 1
      right_column_index: 0
    }
    output_columns: {
      parent_index: 1
      column_index: 1
    }
    output_columns: {
      parent_index: 0
      column_index: 1
    }
    output_columns: {
      parent_index: 0
      column_index: 0
    }
    column_names: "right_1"
    column_names: "left_1"
    column_names: "time_"
    rows_per_batch: 5
  )";

  auto plan_node = PlanNodeFromPbtxt(proto);
  // Left (probe)
  RowDescriptor input_rd_0({types::DataType::TIME64NS, types::DataType::INT64});
  // Right (build)
  RowDescriptor input_rd_1({types::DataType::INT64, types::DataType::FLOAT64});
  // Right[1], Left[1], Left[0]
  RowDescriptor output_rd(
      {types::DataType::FLOAT64, types::DataType::INT64, types::DataType::TIME64NS});
  auto tester = exec::ExecNodeTester<EquijoinNode, plan::JoinOperator>(
      *plan_node, output_rd, {input_rd_0, input_rd_1}, exec_state_.get());

  tester
      // Probe
      .ConsumeNext(RowBatchBuilder(input_rd_0, 4, false, false)
                       .AddColumn<types::Time64NSValue>({10, 20, 30, 31})
                       .AddColumn<types::Int64Value>({1, 2, 3, 3})
                       .get(),
                   0, 0)
      // Build
      .ConsumeNext(RowBatchBuilder(input_rd_1, 3, false, false)
                       .AddColumn<types::Int64Value>({1, 2, 2})
                       .AddColumn<types::Float64Value>({1.0, 2.0, 2.1})
                       .get(),
                   1, 0)
      // Probe
      .ConsumeNext(RowBatchBuilder(input_rd_0, 3, true, true)
                       .AddColumn<types::Time64NSValue>({101, 150, 190})
                       .AddColumn<types::Int64Value>({1, 5, 9})
                       .get(),
                   0, 0)
      // Build
      .ConsumeNext(RowBatchBuilder(input_rd_1, 4, true, true)
                       .AddColumn<types::Int64Value>({9, 1, 1, 8})
                       .AddColumn<types::Float64Value>({9.0, 1.1, 1.2, 8.0})
                       .get(),
                   1, 3)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::Float64Value>({1.0, 1.1, 1.2, 2.0, 2.1})
                          .AddColumn<types::Int64Value>({1, 1, 1, 2, 2})
                          .AddColumn<types::Time64NSValue>({10, 10, 10, 20, 20})
                          .get(),
                      true)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::Float64Value>({0, 0, 1.0, 1.1, 1.2})
                          .AddColumn<types::Int64Value>({3, 3, 1, 1, 1})
                          .AddColumn<types::Time64NSValue>({30, 31, 101, 101, 101})
                          .get(),
                      true)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 2, true, true)
                          .AddColumn<types::Float64Value>({0, 9.0})
                          .AddColumn<types::Int64Value>({5, 9})
                          .AddColumn<types::Time64NSValue>({150, 190})
                          .get(),
                      true)
      .Close();
}

TEST_F(JoinNodeTest, unordered_full_outer_join) {
  // All batches from build first
  // Left table input: [left_0:String, left_1:Int64]
  // Right table input: [right_0:Int64, right_1:String]
  // Output table: [left_1:Int, right_1:String, right_0:Int64]
  // Full outer join on left_0=right_1
  const char* proto = R"(
  type: FULL_OUTER
  equality_conditions {
    left_column_index: 0
    right_column_index: 1
  }
  output_columns: {
    parent_index: 0
    column_index: 1
  }
  output_columns: {
    parent_index: 1
    column_index: 1
  }
  output_columns: {
    parent_index: 1
    column_index: 0
  }
  column_names: "left_1"
  column_names: "right_1"
  column_names: "right_0"
  rows_per_batch: 5
)";

  // Left
  RowDescriptor input_rd_0({types::DataType::TIME64NS, types::DataType::INT64});
  // Right
  RowDescriptor input_rd_1({types::DataType::INT64, types::DataType::TIME64NS});
  // Left[1], Right[1], Right[0]
  RowDescriptor output_rd(
      {types::DataType::INT64, types::DataType::TIME64NS, types::DataType::INT64});

  auto plan_node = PlanNodeFromPbtxt(proto);
  auto tester = exec::ExecNodeTester<EquijoinNode, plan::JoinOperator>(
      *plan_node, output_rd, {input_rd_0, input_rd_1}, exec_state_.get());

  tester
      // Build table
      .ConsumeNext(RowBatchBuilder(input_rd_0, 5, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Time64NSValue>({101, 200, 101, 200, 101})
                       .AddColumn<types::Int64Value>({1, 2, 3, 4, 5})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_0, 5, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Time64NSValue>({200, 200, 200, 300, 300})
                       .AddColumn<types::Int64Value>({6, 8, 10, 12, 14})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_0, 2, /*eow*/ true, /*eos*/ true)
                       .AddColumn<types::Time64NSValue>({400, 500})
                       .AddColumn<types::Int64Value>({16, 18})
                       .get(),
                   0, 0)
      // Probe table
      .ConsumeNext(RowBatchBuilder(input_rd_1, 3, true, true)
                       .AddColumn<types::Int64Value>({-10, -20, -30})
                       .AddColumn<types::Time64NSValue>({110, 120, 101})
                       .get(),
                   1, 3)
      // Some matched rows, some unmatched
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::Int64Value>({0, 0, 1, 3, 5})
                          .AddColumn<types::Time64NSValue>({110, 120, 101, 101, 101})
                          .AddColumn<types::Int64Value>({-10, -20, -30, -30, -30})
                          .get(),
                      true)
      // Emit unmatched build buffer rows (outputted over 2 batches with a random order)
      .ExpectRowBatchesData(RowBatchBuilder(output_rd, 9, true, true)
                                .AddColumn<types::Int64Value>({2, 4, 6, 8, 10, 12, 14, 16, 18})
                                .AddColumn<types::Time64NSValue>({0, 0, 0, 0, 0, 0, 0, 0, 0})
                                .AddColumn<types::Int64Value>({0, 0, 0, 0, 0, 0, 0, 0, 0})
                                .get(),
                            2)
      .Close();
}

TEST_F(JoinNodeTest, unordered_no_left_columns) {
  // All batches from build first
  // Left table input: [left_0:String, left_1:Int64]
  // Right table input: [right_0:Int64, right_1:String]
  // Output table: [left_1:Int, right_1:String, right_0:Int64]
  // Full outer join on left_0=right_1
  const char* proto = R"(
  type: FULL_OUTER
  equality_conditions {
    left_column_index: 0
    right_column_index: 1
  }
  output_columns: {
    parent_index: 1
    column_index: 1
  }
  output_columns: {
    parent_index: 1
    column_index: 0
  }
  column_names: "right_1"
  column_names: "right_0"
  rows_per_batch: 5
)";

  // Left
  RowDescriptor input_rd_0({types::DataType::TIME64NS, types::DataType::INT64});
  // Right
  RowDescriptor input_rd_1({types::DataType::INT64, types::DataType::TIME64NS});
  // Left[1], Right[1], Right[0]
  RowDescriptor output_rd({types::DataType::TIME64NS, types::DataType::INT64});

  auto plan_node = PlanNodeFromPbtxt(proto);
  auto tester = exec::ExecNodeTester<EquijoinNode, plan::JoinOperator>(
      *plan_node, output_rd, {input_rd_0, input_rd_1}, exec_state_.get());

  tester
      // Build table
      .ConsumeNext(RowBatchBuilder(input_rd_0, 5, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Time64NSValue>({101, 200, 101, 200, 101})
                       .AddColumn<types::Int64Value>({1, 2, 3, 4, 5})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_0, 5, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Time64NSValue>({200, 200, 200, 300, 300})
                       .AddColumn<types::Int64Value>({6, 8, 10, 12, 14})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_0, 2, /*eow*/ true, /*eos*/ true)
                       .AddColumn<types::Time64NSValue>({400, 500})
                       .AddColumn<types::Int64Value>({16, 18})
                       .get(),
                   0, 0)
      // Probe table
      .ConsumeNext(RowBatchBuilder(input_rd_1, 3, true, true)
                       .AddColumn<types::Int64Value>({-10, -20, -30})
                       .AddColumn<types::Time64NSValue>({110, 120, 101})
                       .get(),
                   1, 3)
      // Some matched rows, some unmatched
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::Time64NSValue>({110, 120, 101, 101, 101})
                          .AddColumn<types::Int64Value>({-10, -20, -30, -30, -30})
                          .get(),
                      true)
      // Emit unmatched build buffer rows (outputted over 2 batches with a random order)
      .ExpectRowBatchesData(RowBatchBuilder(output_rd, 9, true, true)
                                .AddColumn<types::Time64NSValue>({0, 0, 0, 0, 0, 0, 0, 0, 0})
                                .AddColumn<types::Int64Value>({0, 0, 0, 0, 0, 0, 0, 0, 0})
                                .get(),
                            2)
      .Close();
}

TEST_F(JoinNodeTest, unordered_no_right_columns) {
  // All batches from build first
  // Left table input: [left_0:String, left_1:Int64]
  // Right table input: [right_0:Int64, right_1:String]
  // Output table: [left_1:Int, right_1:String, right_0:Int64]
  // Full outer join on left_0=right_1
  const char* proto = R"(
  type: FULL_OUTER
  equality_conditions {
    left_column_index: 0
    right_column_index: 1
  }
  output_columns: {
    parent_index: 0
    column_index: 1
  }
  column_names: "left_1"
  rows_per_batch: 5
)";

  // Left
  RowDescriptor input_rd_0({types::DataType::TIME64NS, types::DataType::INT64});
  // Right
  RowDescriptor input_rd_1({types::DataType::INT64, types::DataType::TIME64NS});
  // Left[1], Right[1], Right[0]
  RowDescriptor output_rd({types::DataType::INT64});

  auto plan_node = PlanNodeFromPbtxt(proto);
  auto tester = exec::ExecNodeTester<EquijoinNode, plan::JoinOperator>(
      *plan_node, output_rd, {input_rd_0, input_rd_1}, exec_state_.get());

  tester
      // Build table
      .ConsumeNext(RowBatchBuilder(input_rd_0, 5, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Time64NSValue>({101, 200, 101, 200, 101})
                       .AddColumn<types::Int64Value>({1, 2, 3, 4, 5})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_0, 5, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Time64NSValue>({200, 200, 200, 300, 300})
                       .AddColumn<types::Int64Value>({6, 8, 10, 12, 14})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd_0, 2, /*eow*/ true, /*eos*/ true)
                       .AddColumn<types::Time64NSValue>({400, 500})
                       .AddColumn<types::Int64Value>({16, 18})
                       .get(),
                   0, 0)
      // Probe table
      .ConsumeNext(RowBatchBuilder(input_rd_1, 3, true, true)
                       .AddColumn<types::Int64Value>({-10, -20, -30})
                       .AddColumn<types::Time64NSValue>({110, 120, 101})
                       .get(),
                   1, 3)
      // Some matched rows, some unmatched
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::Int64Value>({0, 0, 1, 3, 5})
                          .get(),
                      true)
      // Emit unmatched build buffer rows (outputted over 2 batches with a random order)
      .ExpectRowBatchesData(RowBatchBuilder(output_rd, 9, true, true)
                                .AddColumn<types::Int64Value>({2, 4, 6, 8, 10, 12, 14, 16, 18})
                                .get(),
                            2)
      .Close();
}

TEST_F(JoinNodeTest, unordered_no_matches) {
  // Left table input: [left_0:String, left_1:Int64]
  // Right table input: [right_0:Int64, right_1:String]
  // Output table: [left_1:Int, right_1:String, right_0:Int64]
  // Full outer join on left_0=right_1
  const char* proto = R"(
  type: INNER
  equality_conditions {
    left_column_index: 0
    right_column_index: 1
  }
  output_columns: {
    parent_index: 0
    column_index: 1
  }
  output_columns: {
    parent_index: 1
    column_index: 1
  }
  output_columns: {
    parent_index: 1
    column_index: 0
  }
  column_names: "left_1"
  column_names: "right_1"
  column_names: "right_0"
  rows_per_batch: 5
)";

  // Left
  RowDescriptor input_rd_0({types::DataType::TIME64NS, types::DataType::INT64});
  // Right
  RowDescriptor input_rd_1({types::DataType::INT64, types::DataType::TIME64NS});
  // Left[1], Right[1], Right[0]
  RowDescriptor output_rd(
      {types::DataType::INT64, types::DataType::TIME64NS, types::DataType::INT64});

  auto plan_node = PlanNodeFromPbtxt(proto);
  auto tester = exec::ExecNodeTester<EquijoinNode, plan::JoinOperator>(
      *plan_node, output_rd, {input_rd_0, input_rd_1}, exec_state_.get());

  tester
      // Build table
      .ConsumeNext(RowBatchBuilder(input_rd_0, 5, /*eow*/ true, /*eos*/ true)
                       .AddColumn<types::Time64NSValue>({101, 102, 101, 103, 101})
                       .AddColumn<types::Int64Value>({1, 2, 3, 4, 5})
                       .get(),
                   0, 0)
      // Probe table
      .ConsumeNext(RowBatchBuilder(input_rd_1, 3, true, true)
                       .AddColumn<types::Int64Value>({-10, -20, -30})
                       .AddColumn<types::Time64NSValue>({200, 300, 400})
                       .get(),
                   1, 1)
      // No matches
      .ExpectRowBatch(RowBatchBuilder(output_rd, 0, true, true)
                          .AddColumn<types::Int64Value>({})
                          .AddColumn<types::Time64NSValue>({})
                          .AddColumn<types::Int64Value>({})
                          .get(),
                      /*unordered */ false)
      .Close();
}

TEST_F(JoinNodeTest, zero_row_row_batch_right) {
  // Left table input: [left_0:String, left_1:Int64]
  // Right table input: [right_0:Int64, right_1:String]
  // Output table: [left_1:Int, right_1:String, right_0:Int64]
  // Full outer join on left_0=right_1
  const char* proto = R"(
  type: INNER
  equality_conditions {
    left_column_index: 0
    right_column_index: 1
  }
  output_columns: {
    parent_index: 0
    column_index: 1
  }
  output_columns: {
    parent_index: 1
    column_index: 1
  }
  output_columns: {
    parent_index: 1
    column_index: 0
  }
  column_names: "left_1"
  column_names: "right_1"
  column_names: "right_0"
  rows_per_batch: 5
)";

  // Left
  RowDescriptor input_rd_0({types::DataType::TIME64NS, types::DataType::INT64});
  // Right
  RowDescriptor input_rd_1({types::DataType::INT64, types::DataType::TIME64NS});
  // Left[1], Right[1], Right[0]
  RowDescriptor output_rd(
      {types::DataType::INT64, types::DataType::TIME64NS, types::DataType::INT64});

  auto plan_node = PlanNodeFromPbtxt(proto);
  auto tester = exec::ExecNodeTester<EquijoinNode, plan::JoinOperator>(
      *plan_node, output_rd, {input_rd_0, input_rd_1}, exec_state_.get());

  tester
      // Build table
      .ConsumeNext(RowBatchBuilder(input_rd_0, 5, /*eow*/ true, /*eos*/ true)
                       .AddColumn<types::Time64NSValue>({101, 102, 101, 103, 101})
                       .AddColumn<types::Int64Value>({1, 2, 3, 4, 5})
                       .get(),
                   0, 0)
      // Probe table
      .ConsumeNext(RowBatchBuilder(input_rd_1, 0, true, true)
                       .AddColumn<types::Int64Value>({})
                       .AddColumn<types::Time64NSValue>({})
                       .get(),
                   1, 1)
      // No matches
      .ExpectRowBatch(RowBatchBuilder(output_rd, 0, true, true)
                          .AddColumn<types::Int64Value>({})
                          .AddColumn<types::Time64NSValue>({})
                          .AddColumn<types::Int64Value>({})
                          .get(),
                      /*unordered */ false)
      .Close();
}

TEST_F(JoinNodeTest, unordered_many_matches) {
  // Left table input: [left_0:Time, left_1:Int64]
  // Right table input: [right_0:Int64, right_1:Time]
  // Output table: [left_1:Int, right_1(time):Time, right_0:Int64]
  // Full outer join on left_0=right_1
  const char* proto = R"(
  type: INNER
  equality_conditions {
    left_column_index: 0
    right_column_index: 1
  }
  output_columns: {
    parent_index: 0
    column_index: 1
  }
  output_columns: {
    parent_index: 1
    column_index: 1
  }
  output_columns: {
    parent_index: 1
    column_index: 0
  }
  column_names: "left_1"
  column_names: "time_"
  column_names: "right_0"
  rows_per_batch: 5
)";

  // Left
  RowDescriptor input_rd_0({types::DataType::TIME64NS, types::DataType::INT64});
  // Right
  RowDescriptor input_rd_1({types::DataType::INT64, types::DataType::TIME64NS});
  // Left[1], Right[1], Right[0]
  RowDescriptor output_rd(
      {types::DataType::INT64, types::DataType::TIME64NS, types::DataType::INT64});

  auto plan_node = PlanNodeFromPbtxt(proto);
  auto tester = exec::ExecNodeTester<EquijoinNode, plan::JoinOperator>(
      *plan_node, output_rd, {input_rd_0, input_rd_1}, exec_state_.get());

  tester
      // Build(left) table
      .ConsumeNext(RowBatchBuilder(input_rd_0, 5, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Time64NSValue>({101, 102, 103, 101, 102})
                       .AddColumn<types::Int64Value>({1, 2, 3, 4, 5})
                       .get(),
                   0, 0)
      // Build table
      .ConsumeNext(RowBatchBuilder(input_rd_0, 3, /*eow*/ true, /*eos*/ true)
                       .AddColumn<types::Time64NSValue>({103, 101, 104})
                       .AddColumn<types::Int64Value>({6, 7, 8})
                       .get(),
                   0, 0)
      // Probe(right) table
      .ConsumeNext(RowBatchBuilder(input_rd_1, 4, false, false)
                       .AddColumn<types::Int64Value>({10, 20, 30, 40})
                       .AddColumn<types::Time64NSValue>({101, 101, 102, 102})
                       .get(),
                   1, 1)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          // Left[1]
                          .AddColumn<types::Int64Value>({1, 4, 7, 1, 4})
                          // Right[1]
                          .AddColumn<types::Time64NSValue>({101, 101, 101, 101, 101})
                          // Right[0]
                          .AddColumn<types::Int64Value>({10, 10, 10, 20, 20})
                          .get(),
                      true)
      // Probe table
      .ConsumeNext(RowBatchBuilder(input_rd_1, 5, true, true)
                       .AddColumn<types::Int64Value>({50, 60, 70, 80, 90})
                       .AddColumn<types::Time64NSValue>({103, 103, 103, 103, 105})
                       .get(),
                   1, 3)
      // No matches
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::Int64Value>({7, 2, 5, 2, 5})
                          .AddColumn<types::Time64NSValue>({101, 102, 102, 102, 102})
                          .AddColumn<types::Int64Value>({20, 30, 30, 40, 40})
                          .get(),
                      true)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, false, false)
                          .AddColumn<types::Int64Value>({3, 6, 3, 6, 3})
                          .AddColumn<types::Time64NSValue>({103, 103, 103, 103, 103})
                          .AddColumn<types::Int64Value>({50, 50, 60, 60, 70})
                          .get(),
                      true)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 3, true, true)
                          .AddColumn<types::Int64Value>({6, 3, 6})
                          .AddColumn<types::Time64NSValue>({103, 103, 103})
                          .AddColumn<types::Int64Value>({70, 80, 80})
                          .get(),
                      true)
      .Close();
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
