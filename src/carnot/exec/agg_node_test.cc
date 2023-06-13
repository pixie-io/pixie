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

#include "src/carnot/exec/agg_node.h"

#include <algorithm>

#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <sole.hpp>

#include "src/carnot/exec/test_utils.h"
#include "src/carnot/plan/operators.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/udf/base.h"
#include "src/carnot/udf/registry.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/types.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::schema::RowDescriptor;
using ::testing::_;
using types::Int64Value;
using udf::FunctionContext;

// Test UDA, takes the min of two arguments and then sums them.
// TODO(zasgar): move these all to a common file.
class MinSumUDA : public udf::UDA {
 public:
  void Update(udf::FunctionContext*, types::Int64Value arg1, types::Int64Value arg2) {
    sum_ = sum_.val + std::min(arg1.val, arg2.val);
  }
  void Merge(udf::FunctionContext*, const MinSumUDA& other) { sum_ = sum_.val + other.sum_.val; }
  types::Int64Value Finalize(udf::FunctionContext*) { return sum_; }
  StringValue Serialize(udf::FunctionContext*) { return absl::StrCat(sum_.val); }
  Status Deserialize(udf::FunctionContext*, const types::StringValue& serialized) {
    PX_UNUSED(absl::SimpleAtoi(serialized, &sum_.val));
    return Status::OK();
  }

 protected:
  types::Int64Value sum_ = 0;
};

class MinSumWithInitUDA : public udf::UDA {
 public:
  Status Init(udf::FunctionContext*, types::Int64Value init_val) {
    sum_ = init_val;
    return Status::OK();
  }
  void Update(udf::FunctionContext*, types::Int64Value arg1, types::Int64Value arg2) {
    sum_ = sum_.val + std::min(arg1.val, arg2.val);
  }
  void Merge(udf::FunctionContext*, const MinSumWithInitUDA& other) {
    sum_ = sum_.val + other.sum_.val;
  }
  types::Int64Value Finalize(udf::FunctionContext*) { return sum_; }
  StringValue Serialize(udf::FunctionContext*) { return absl::StrCat(sum_.val); }
  Status Deserialize(udf::FunctionContext*, const types::StringValue& serialized) {
    PX_UNUSED(absl::SimpleAtoi(serialized, &sum_.val));
    return Status::OK();
  }

 protected:
  types::Int64Value sum_ = 0;
};

constexpr char kBlockingNoGroupAgg[] = R"(
op_type: AGGREGATE_OPERATOR
agg_op {
  windowed: false
  values {
    name: "minsum"
    args {
      column {
        node:0
        index: 0
      }
    }
    args {
      column {
        node:0
        index: 1
      }
    }
  }
  value_names: "value1"
  partial_agg: true
  finalize_results: true
})";

constexpr char kBlockingSingleGroupAgg[] = R"(
op_type: AGGREGATE_OPERATOR
agg_op {
  windowed: false
  values {
    name: "minsum"
    args {
      column {
        node:0
        index: 0
      }
    }
    args {
      column {
        node:0
        index: 1
      }
    }
  }
  groups {
     node: 0
     index: 0
  }
  group_names: "g1"
  value_names: "value1"
  partial_agg: true
  finalize_results: true
})";

constexpr char kBlockingMultipleGroupAgg[] = R"(
op_type: AGGREGATE_OPERATOR
agg_op {
  windowed: false
  values {
    name: "minsum"
    args {
      column {
        node:0
        index: 2
      }
    }
    args {
      column {
        node:0
        index: 1
      }
    }
  }
  groups {
     node: 0
     index: 0
  }
  groups {
     node: 0
     index: 1
  }
  group_names: "g1"
  group_names: "g2"
  value_names: "value1"
  partial_agg: true
  finalize_results: true
})";

constexpr char kWindowedNoGroupAgg[] = R"(
op_type: AGGREGATE_OPERATOR
agg_op {
  windowed: true
  values {
    name: "minsum"
    args {
      column {
        node:0
        index: 0
      }
    }
    args {
      column {
        node:0
        index: 1
      }
    }
  }
  value_names: "value1"
  partial_agg: true
  finalize_results: true
})";

constexpr char kWindowedSingleGroupAgg[] = R"(
op_type: AGGREGATE_OPERATOR
agg_op {
  windowed: true
  values {
    name: "minsum"
    args {
      column {
        node:0
        index: 0
      }
    }
    args {
      column {
        node:0
        index: 1
      }
    }
  }
  groups {
     node: 0
     index: 0
  }
  group_names: "g1"
  value_names: "value1"
  partial_agg: true
  finalize_results: true
})";

constexpr char kSingleGroupNoValues[] = R"(
op_type: AGGREGATE_OPERATOR
agg_op {
  groups {
     node: 0
     index: 0
  }
  group_names: "g1"
  partial_agg: true
  finalize_results: true
})";

constexpr char kBlockingNoGroupInitArgAgg[] = R"(
op_type: AGGREGATE_OPERATOR
agg_op {
  windowed: false
  values {
    name: "minsum_w_init"
    args {
      column {
        node:0
        index: 0
      }
    }
    args {
      column {
        node:0
        index: 1
      }
    }
    init_args {
      data_type: INT64
      int64_value: 10
    }
    id: 1
  }
  value_names: "value1"
  partial_agg: true
  finalize_results: true
})";

constexpr char kBlockingSingleGroupInitArgAgg[] = R"(
op_type: AGGREGATE_OPERATOR
agg_op {
  windowed: false
  values {
    name: "minsum_w_init"
    args {
      column {
        node:0
        index: 0
      }
    }
    args {
      column {
        node:0
        index: 1
      }
    }
    init_args {
      data_type: INT64
      int64_value: 10
    }
    id: 1
  }
  groups {
     node: 0
     index: 0
  }
  group_names: "g1"
  value_names: "value1"
  partial_agg: true
  finalize_results: true
})";

constexpr char kPartialNoGroupAgg[] = R"(
op_type: AGGREGATE_OPERATOR
agg_op {
  windowed: false
  values {
    name: "minsum"
    args {
      column {
        node:0
        index: 0
      }
    }
    args {
      column {
        node:0
        index: 1
      }
    }
  }
  value_names: "value1"
  partial_agg: true
  finalize_results: false
})";

constexpr char kPartialNoGroupAggFinalize[] = R"(
op_type: AGGREGATE_OPERATOR
agg_op {
  windowed: false
  values {
    name: "minsum"
    args {
      column {
        node:0
        index: 0
      }
    }
    args {
      column {
        node:0
        index: 1
      }
    }
  }
  value_names: "value1"
  partial_agg: false
  finalize_results: true
})";

constexpr char kPartialSingleGroupAgg[] = R"(
op_type: AGGREGATE_OPERATOR
agg_op {
  windowed: false
  values {
    name: "minsum"
    args {
      column {
        node:0
        index: 1
      }
    }
    args {
      column {
        node:0
        index: 2
      }
    }
  }
  groups {
     node: 0
     index: 0
  }
  group_names: "g1"
  value_names: "value1"
  partial_agg: true
  finalize_results: false
})";

constexpr char kPartialSingleGroupAggFinalize[] = R"(
op_type: AGGREGATE_OPERATOR
agg_op {
  windowed: false
  values {
    name: "minsum"
    args {
      column {
        node:0
        index: 1
      }
    }
    args {
      column {
        node:0
        index: 2
      }
    }
  }
  groups {
     node: 0
     index: 0
  }
  group_names: "g1"
  value_names: "value1"
  partial_agg: false
  finalize_results: true
})";

constexpr char kPartialMultipleGroupAgg[] = R"(
op_type: AGGREGATE_OPERATOR
agg_op {
  windowed: false
  values {
    name: "minsum"
    args {
      column {
        node:0
        index: 2
      }
    }
    args {
      column {
        node:0
        index: 1
      }
    }
  }
  groups {
     node: 0
     index: 0
  }
  groups {
     node: 0
     index: 1
  }
  group_names: "g1"
  group_names: "g2"
  value_names: "value1"
  partial_agg: true
  finalize_results: false
})";
constexpr char kPartialMultipleGroupAggFinalize[] = R"(
op_type: AGGREGATE_OPERATOR
agg_op {
  windowed: false
  values {
    name: "minsum"
    args {
      column {
        node:0
        index: 2
      }
    }
    args {
      column {
        node:0
        index: 1
      }
    }
  }
  groups {
     node: 0
     index: 0
  }
  groups {
     node: 0
     index: 1
  }
  group_names: "g1"
  group_names: "g2"
  value_names: "value1"
  partial_agg: false
  finalize_results: true
})";

std::unique_ptr<ExecState> MakeTestExecState(udf::Registry* registry) {
  auto table_store = std::make_shared<table_store::TableStore>();
  return std::make_unique<ExecState>(registry, table_store, MockResultSinkStubGenerator,
                                     MockMetricsStubGenerator, MockTraceStubGenerator,
                                     sole::uuid4(), nullptr);
}

std::unique_ptr<plan::Operator> PlanNodeFromPbtxt(const std::string& pbtxt) {
  planpb::Operator op_pb;
  EXPECT_TRUE(google::protobuf::TextFormat::MergeFromString(pbtxt, &op_pb));
  return plan::AggregateOperator::FromProto(op_pb, 1);
}

class AggNodeTest : public ::testing::Test {
 public:
  AggNodeTest() {
    func_registry_ = std::make_unique<udf::Registry>("test");
    EXPECT_TRUE(func_registry_->Register<MinSumUDA>("minsum").ok());
    EXPECT_TRUE(func_registry_->Register<MinSumWithInitUDA>("minsum_w_init").ok());

    exec_state_ = MakeTestExecState(func_registry_.get());
    EXPECT_OK(exec_state_->AddUDA(0, "minsum",
                                  std::vector<types::DataType>({types::INT64, types::INT64})));
    EXPECT_OK(exec_state_->AddUDA(1, "minsum_w_init", {types::INT64, types::INT64, types::INT64}));
  }

 protected:
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::Registry> func_registry_;
};

TEST_F(AggNodeTest, no_groups_blocking) {
  auto plan_node = PlanNodeFromPbtxt(kBlockingNoGroupAgg);
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});

  RowDescriptor output_rd({types::DataType::INT64});

  auto tester = exec::ExecNodeTester<AggNode, plan::AggregateOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4})
                       .AddColumn<types::Int64Value>({2, 5, 6, 8})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true, true)
                       .AddColumn<types::Int64Value>({5, 6, 3, 4})
                       .AddColumn<types::Int64Value>({1, 5, 3, 8})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 1, true, true)
                          .AddColumn<types::Int64Value>({Int64Value(23)})
                          .get(),
                      false)
      .Close();
}

TEST_F(AggNodeTest, zero_row_row_batch) {
  auto plan_node = PlanNodeFromPbtxt(kBlockingNoGroupAgg);
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});

  RowDescriptor output_rd({types::DataType::INT64});

  auto tester = exec::ExecNodeTester<AggNode, plan::AggregateOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4})
                       .AddColumn<types::Int64Value>({2, 5, 6, 8})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd, 0, true, true)
                       .AddColumn<types::Int64Value>({})
                       .AddColumn<types::Int64Value>({})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 1, true, true)
                          .AddColumn<types::Int64Value>({Int64Value(10)})
                          .get(),
                      false)
      .Close();
}

TEST_F(AggNodeTest, single_group_blocking) {
  auto plan_node = PlanNodeFromPbtxt(kBlockingSingleGroupAgg);
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});

  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<AggNode, plan::AggregateOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 1, 2, 2})
                       .AddColumn<types::Int64Value>({2, 3, 3, 1})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true, true)
                       .AddColumn<types::Int64Value>({5, 6, 3, 4})
                       .AddColumn<types::Int64Value>({1, 5, 3, 8})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 6, true, true)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6})
                          .AddColumn<types::Int64Value>({2, 3, 3, 4, 1, 5})
                          .get(),
                      false)
      .Close();
}

TEST_F(AggNodeTest, multiple_groups_blocking) {
  auto plan_node = PlanNodeFromPbtxt(kBlockingMultipleGroupAgg);
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64});

  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<AggNode, plan::AggregateOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 5, 1, 2})
                       .AddColumn<types::Int64Value>({2, 1, 3, 1})
                       .AddColumn<types::Int64Value>({2, 5, 3, 1})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true, true)
                       .AddColumn<types::Int64Value>({5, 1, 3, 3})
                       .AddColumn<types::Int64Value>({1, 2, 3, 3})
                       .AddColumn<types::Int64Value>({1, 3, 3, 8})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, true, true)
                          .AddColumn<types::Int64Value>({1, 1, 2, 5, 3})
                          .AddColumn<types::Int64Value>({2, 3, 1, 1, 3})
                          .AddColumn<types::Int64Value>({4, 3, 1, 2, 6})
                          .get(),
                      false)
      .Close();
}

TEST_F(AggNodeTest, multiple_groups_with_string_blocking) {
  auto plan_node = PlanNodeFromPbtxt(kBlockingMultipleGroupAgg);
  RowDescriptor input_rd({types::DataType::STRING, types::DataType::INT64, types::DataType::INT64});

  RowDescriptor output_rd(
      {types::DataType::STRING, types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<AggNode, plan::AggregateOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::StringValue>({"abc", "def", "abc", "fgh"})
                       .AddColumn<types::Int64Value>({2, 1, 3, 1})
                       .AddColumn<types::Int64Value>({2, 5, 3, 1})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true, true)
                       .AddColumn<types::StringValue>({"ijk", "abc", "abc", "def"})
                       .AddColumn<types::Int64Value>({1, 2, 3, 3})
                       .AddColumn<types::Int64Value>({1, 3, 3, 8})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 6, true, true)
                          .AddColumn<types::StringValue>({"abc", "def", "abc", "fgh", "ijk", "def"})
                          .AddColumn<types::Int64Value>({2, 1, 3, 1, 1, 3})
                          .AddColumn<types::Int64Value>({4, 1, 6, 1, 1, 3})
                          .get(),
                      false)
      .Close();
}

TEST_F(AggNodeTest, no_groups_windowed) {
  auto plan_node = PlanNodeFromPbtxt(kWindowedNoGroupAgg);
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});

  RowDescriptor output_rd({types::DataType::INT64});

  auto tester = exec::ExecNodeTester<AggNode, plan::AggregateOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4})
                       .AddColumn<types::Int64Value>({2, 5, 6, 8})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true, false)
                       .AddColumn<types::Int64Value>({5, 6, 3, 4})
                       .AddColumn<types::Int64Value>({1, 5, 3, 8})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 1, true, false)
                          .AddColumn<types::Int64Value>({Int64Value(23)})
                          .get(),
                      false)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, false, false)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4})
                       .AddColumn<types::Int64Value>({2, 5, 6, 8})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true, true)
                       .AddColumn<types::Int64Value>({5, 6, 3, 4})
                       .AddColumn<types::Int64Value>({1, 5, 3, 8})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 1, true, true)
                          .AddColumn<types::Int64Value>({Int64Value(23)})
                          .get(),
                      false)
      .Close();
}

TEST_F(AggNodeTest, single_group_windowed) {
  auto plan_node = PlanNodeFromPbtxt(kWindowedSingleGroupAgg);
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});

  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<AggNode, plan::AggregateOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 1, 2, 2})
                       .AddColumn<types::Int64Value>({2, 3, 3, 1})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true, false)
                       .AddColumn<types::Int64Value>({5, 6, 3, 4})
                       .AddColumn<types::Int64Value>({1, 5, 3, 8})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 6, true, false)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6})
                          .AddColumn<types::Int64Value>({2, 3, 3, 4, 1, 5})
                          .get(),
                      false)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, false, false)
                       .AddColumn<types::Int64Value>({1, 1, 2, 2})
                       .AddColumn<types::Int64Value>({2, 3, 3, 1})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true, true)
                       .AddColumn<types::Int64Value>({5, 6, 3, 4})
                       .AddColumn<types::Int64Value>({1, 5, 3, 8})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 6, true, true)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6})
                          .AddColumn<types::Int64Value>({2, 3, 3, 4, 1, 5})
                          .get(),
                      false)
      .Close();
}

TEST_F(AggNodeTest, no_aggregate_expressions) {
  auto plan_node = PlanNodeFromPbtxt(kSingleGroupNoValues);
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});

  RowDescriptor output_rd({types::DataType::INT64});

  auto tester = exec::ExecNodeTester<AggNode, plan::AggregateOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({2, 1, 3, 1})
                       .AddColumn<types::Int64Value>({2, 5, 3, 1})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true, true)
                       .AddColumn<types::Int64Value>({1, 2, 3, 3})
                       .AddColumn<types::Int64Value>({1, 3, 3, 8})
                       .get(),
                   0)
      .ExpectRowBatch(
          RowBatchBuilder(output_rd, 3, true, true).AddColumn<types::Int64Value>({2, 1, 3}).get(),
          false)
      .Close();
}

TEST_F(AggNodeTest, no_groups_blocking_init_args) {
  auto plan_node = PlanNodeFromPbtxt(kBlockingNoGroupInitArgAgg);
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});

  RowDescriptor output_rd({types::DataType::INT64});

  auto tester = exec::ExecNodeTester<AggNode, plan::AggregateOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4})
                       .AddColumn<types::Int64Value>({2, 5, 6, 8})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true, true)
                       .AddColumn<types::Int64Value>({5, 6, 3, 4})
                       .AddColumn<types::Int64Value>({1, 5, 3, 8})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 1, true, true)
                          .AddColumn<types::Int64Value>({Int64Value(23 + 10)})
                          .get(),
                      false)
      .Close();
}

TEST_F(AggNodeTest, single_group_blocking_init_args) {
  auto plan_node = PlanNodeFromPbtxt(kBlockingSingleGroupInitArgAgg);
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});

  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<AggNode, plan::AggregateOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 1, 2, 2})
                       .AddColumn<types::Int64Value>({2, 3, 3, 1})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true, true)
                       .AddColumn<types::Int64Value>({5, 6, 3, 4})
                       .AddColumn<types::Int64Value>({1, 5, 3, 8})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 6, true, true)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6})
                          .AddColumn<types::Int64Value>({12, 13, 13, 14, 11, 15})
                          .get(),
                      false)
      .Close();
}

TEST_F(AggNodeTest, no_groups_partial) {
  auto plan_node = PlanNodeFromPbtxt(kPartialNoGroupAgg);
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::STRING});

  auto tester = exec::ExecNodeTester<AggNode, plan::AggregateOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 2, 3, 4})
                       .AddColumn<types::Int64Value>({2, 5, 6, 8})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true, true)
                       .AddColumn<types::Int64Value>({5, 6, 3, 4})
                       .AddColumn<types::Int64Value>({1, 5, 3, 8})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 1, true, true)
                          .AddColumn<types::StringValue>({types::StringValue("23")})
                          .get(),
                      false)
      .Close();
}

TEST_F(AggNodeTest, no_groups_partial_finalize) {
  auto plan_node = PlanNodeFromPbtxt(kPartialNoGroupAggFinalize);
  RowDescriptor input_rd({types::DataType::STRING});
  RowDescriptor output_rd({types::DataType::INT64});

  auto tester = exec::ExecNodeTester<AggNode, plan::AggregateOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 2, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::StringValue>({"10", "23"})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd, 3, true, true)
                       .AddColumn<types::StringValue>({"1", "0", "0"})
                       .get(),
                   0)
      .ExpectRowBatch(
          RowBatchBuilder(output_rd, 1, true, true).AddColumn<types::Int64Value>({34}).get(), false)
      .Close();
}

TEST_F(AggNodeTest, single_group_partial) {
  auto plan_node = PlanNodeFromPbtxt(kPartialSingleGroupAgg);
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64});

  RowDescriptor output_rd({types::DataType::INT64, types::DataType::STRING});

  auto tester = exec::ExecNodeTester<AggNode, plan::AggregateOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 1, 2, 2})
                       .AddColumn<types::Int64Value>({1, 1, 2, 2})
                       .AddColumn<types::Int64Value>({2, 3, 3, 1})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true, true)
                       .AddColumn<types::Int64Value>({5, 6, 3, 4})
                       .AddColumn<types::Int64Value>({5, 6, 3, 4})
                       .AddColumn<types::Int64Value>({1, 5, 3, 8})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 6, true, true)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6})
                          .AddColumn<types::StringValue>({"2", "3", "3", "4", "1", "5"})
                          .get(),
                      false)
      .Close();
}

TEST_F(AggNodeTest, single_group_partial_finalize) {
  auto plan_node = PlanNodeFromPbtxt(kPartialSingleGroupAggFinalize);
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::STRING});

  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<AggNode, plan::AggregateOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 1, 2, 2})
                       .AddColumn<types::StringValue>({"2", "3", "3", "1"})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true, true)
                       .AddColumn<types::Int64Value>({5, 6, 3, 4})
                       .AddColumn<types::StringValue>({"1", "5", "3", "8"})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 6, true, true)
                          .AddColumn<types::Int64Value>({1, 2, 3, 4, 5, 6})
                          .AddColumn<types::Int64Value>({5, 4, 3, 8, 1, 5})
                          .get(),
                      false)
      .Close();
}

TEST_F(AggNodeTest, multiple_groups_partial) {
  auto plan_node = PlanNodeFromPbtxt(kPartialMultipleGroupAgg);
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64});

  RowDescriptor output_rd(
      {types::DataType::INT64, types::DataType::INT64, types::DataType::STRING});

  auto tester = exec::ExecNodeTester<AggNode, plan::AggregateOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 5, 1, 2})
                       .AddColumn<types::Int64Value>({2, 1, 3, 1})
                       .AddColumn<types::Int64Value>({2, 5, 3, 1})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true, true)
                       .AddColumn<types::Int64Value>({5, 1, 3, 3})
                       .AddColumn<types::Int64Value>({1, 2, 3, 3})
                       .AddColumn<types::Int64Value>({1, 3, 3, 8})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, true, true)
                          .AddColumn<types::Int64Value>({1, 1, 2, 5, 3})
                          .AddColumn<types::Int64Value>({2, 3, 1, 1, 3})
                          .AddColumn<types::StringValue>({"4", "3", "1", "2", "6"})
                          .get(),
                      false)
      .Close();
}

TEST_F(AggNodeTest, multiple_groups_partial_finalize) {
  auto plan_node = PlanNodeFromPbtxt(kPartialMultipleGroupAggFinalize);
  RowDescriptor input_rd({types::DataType::INT64, types::DataType::INT64, types::DataType::STRING});

  RowDescriptor output_rd({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64});

  auto tester = exec::ExecNodeTester<AggNode, plan::AggregateOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  tester
      .ConsumeNext(RowBatchBuilder(input_rd, 4, /*eow*/ false, /*eos*/ false)
                       .AddColumn<types::Int64Value>({1, 5, 1, 2})
                       .AddColumn<types::Int64Value>({2, 1, 3, 1})
                       .AddColumn<types::StringValue>({"2", "5", "3", "1"})
                       .get(),
                   0, 0)
      .ConsumeNext(RowBatchBuilder(input_rd, 4, true, true)
                       .AddColumn<types::Int64Value>({5, 1, 3, 3})
                       .AddColumn<types::Int64Value>({1, 2, 3, 3})
                       .AddColumn<types::StringValue>({"1", "3", "3", "8"})
                       .get(),
                   0)
      .ExpectRowBatch(RowBatchBuilder(output_rd, 5, true, true)
                          .AddColumn<types::Int64Value>({1, 1, 2, 5, 3})
                          .AddColumn<types::Int64Value>({2, 3, 1, 1, 3})
                          .AddColumn<types::Int64Value>({5, 3, 1, 6, 11})
                          .get(),
                      false)
      .Close();
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
