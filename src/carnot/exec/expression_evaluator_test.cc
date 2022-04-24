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

#include "src/carnot/exec/expression_evaluator.h"

#include <arrow/memory_pool.h>
#include <arrow/type_fwd.h>
#include <memory>
#include <string>
#include <vector>

#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <sole.hpp>

#include "src/carnot/exec/exec_state.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/plan/scalar_expression.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf/registry.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace exec {

using px::carnot::planpb::testutils::kAddScalarFuncConstPbtxt;
using px::carnot::planpb::testutils::kAddScalarFuncNestedPbtxt;
using px::carnot::planpb::testutils::kAddScalarFuncPbtxt;
using px::carnot::planpb::testutils::kScalarInt64ValuePbtxt;
using px::carnot::planpb::testutils::kScalarUInt128ValuePbtxt;
using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;
using types::Int64Value;
using types::ToArrow;
using udf::FunctionContext;

class AddUDF : public udf::ScalarUDF {
 public:
  types::Int64Value Exec(FunctionContext*, types::Int64Value v1, types::Int64Value v2) {
    return v1.val + v2.val;
  }
};

class InitArgUDF : public udf::ScalarUDF {
 public:
  Status Init(FunctionContext*, types::StringValue str, types::Int64Value i) {
    str_ = str;
    i_ = i.val;
    return Status::OK();
  }
  types::StringValue Exec(FunctionContext*, types::StringValue arg) {
    return absl::Substitute("$0, $1, $2", str_, i_, arg);
  }

 private:
  std::string str_;
  int64_t i_;
};

std::shared_ptr<plan::ScalarExpression> AddScalarExpr() {
  planpb::ScalarExpression se_pb;
  google::protobuf::TextFormat::MergeFromString(kAddScalarFuncPbtxt, &se_pb);
  auto s_or_se = plan::ScalarExpression::FromProto(se_pb);
  EXPECT_TRUE(s_or_se.ok());
  return s_or_se.ConsumeValueOrDie();
}

std::shared_ptr<plan::ScalarExpression> Int64ConstScalarExpr() {
  planpb::ScalarExpression se_pb;
  google::protobuf::TextFormat::MergeFromString(kScalarInt64ValuePbtxt, &se_pb);
  auto s_or_se = plan::ScalarExpression::FromProto(se_pb);
  EXPECT_TRUE(s_or_se.ok());
  return s_or_se.ConsumeValueOrDie();
}

std::shared_ptr<plan::ScalarExpression> UInt128ConstScalarExpr() {
  planpb::ScalarExpression se_pb;
  google::protobuf::TextFormat::MergeFromString(kScalarUInt128ValuePbtxt, &se_pb);
  auto s_or_se = plan::ScalarExpression::FromProto(se_pb);
  EXPECT_TRUE(s_or_se.ok());
  return s_or_se.ConsumeValueOrDie();
}

std::shared_ptr<plan::ScalarExpression> ScalarExpressionOf(const std::string& pbtxt) {
  planpb::ScalarExpression se_pb;
  google::protobuf::TextFormat::MergeFromString(pbtxt, &se_pb);
  auto s_or_se = plan::ScalarExpression::FromProto(se_pb);
  EXPECT_TRUE(s_or_se.ok());
  return s_or_se.ConsumeValueOrDie();
}

class ScalarExpressionTest : public ::testing::TestWithParam<ScalarExpressionEvaluatorType> {
 public:
  void SetUp() override {
    func_registry_ = std::make_unique<udf::Registry>("test_registry");
    auto table_store = std::make_shared<table_store::TableStore>();

    EXPECT_TRUE(func_registry_->Register<AddUDF>("add").ok());
    EXPECT_TRUE(func_registry_->Register<InitArgUDF>("init_arg").ok());
    exec_state_ = std::make_unique<ExecState>(func_registry_.get(), table_store,
                                              MockResultSinkStubGenerator, MockMetricsStubGenerator,
                                              MockTraceStubGenerator, sole::uuid4(), nullptr);
    EXPECT_OK(exec_state_->AddScalarUDF(
        0, "add", std::vector<types::DataType>({types::DataType::INT64, types::DataType::INT64})));
    EXPECT_OK(
        exec_state_->AddScalarUDF(1, "init_arg", {types::STRING, types::INT64, types::STRING}));

    std::vector<types::Int64Value> in1 = {1, 2, 3};
    std::vector<types::Int64Value> in2 = {3, 4, 5};
    std::vector<types::StringValue> in3 = {"a", "b", "c"};

    RowDescriptor rd({types::DataType::INT64, types::DataType::INT64, types::DataType::STRING});
    input_rb_ = std::make_unique<RowBatch>(rd, in1.size());

    EXPECT_TRUE(input_rb_->AddColumn(ToArrow(in1, arrow::default_memory_pool())).ok());
    EXPECT_TRUE(input_rb_->AddColumn(ToArrow(in2, arrow::default_memory_pool())).ok());
    EXPECT_TRUE(input_rb_->AddColumn(ToArrow(in3, arrow::default_memory_pool())).ok());
  }

 protected:
  std::unique_ptr<ScalarExpressionEvaluator> RunEvaluator(
      const std::vector<std::shared_ptr<const plan::ScalarExpression>>& exprs,
      RowBatch* output_rb) {
    function_ctx_ = std::make_unique<udf::FunctionContext>(nullptr, nullptr);
    auto evaluator = ScalarExpressionEvaluator::Create(exprs, GetParam(), function_ctx_.get());
    EXPECT_TRUE(evaluator->Open(exec_state_.get()).ok())
        << evaluator->Open(exec_state_.get()).msg();
    EXPECT_TRUE(evaluator->Evaluate(exec_state_.get(), *input_rb_, output_rb).ok());
    EXPECT_TRUE(evaluator->Close(exec_state_.get()).ok());
    return evaluator;
  }

  std::shared_ptr<plan::ScalarExpression> se_;
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<RowBatch> input_rb_;
  std::unique_ptr<udf::Registry> func_registry_;
  std::unique_ptr<udf::FunctionContext> function_ctx_;
};

INSTANTIATE_TEST_SUITE_P(TestVecAndArrow, ScalarExpressionTest,
                         ::testing::Values(ScalarExpressionEvaluatorType::kVectorNative,
                                           ScalarExpressionEvaluatorType::kArrowNative));

TEST_P(ScalarExpressionTest, basic_tests) {
  RowDescriptor rd_output({types::DataType::INT64});
  RowBatch output_rb(rd_output, input_rb_->num_rows());

  auto se = AddScalarExpr();
  RunEvaluator({se}, &output_rb);
  auto out_col = output_rb.ColumnAt(0);
  auto casted = static_cast<arrow::Int64Array*>(out_col.get());
  EXPECT_EQ(4, casted->Value(0));
  EXPECT_EQ(6, casted->Value(1));
  EXPECT_EQ(8, casted->Value(2));
}

TEST_P(ScalarExpressionTest, eval_constant) {
  RowDescriptor rd_output({types::DataType::INT64});
  RowBatch output_rb(rd_output, input_rb_->num_rows());

  auto se = Int64ConstScalarExpr();
  RunEvaluator({se}, &output_rb);

  auto out_col = output_rb.ColumnAt(0);
  EXPECT_EQ(3, out_col->length());
  auto casted = static_cast<arrow::Int64Array*>(out_col.get());
  EXPECT_EQ(1337, casted->Value(0));
  EXPECT_EQ(1337, casted->Value(1));
  EXPECT_EQ(1337, casted->Value(2));
}

TEST_P(ScalarExpressionTest, eval_col_const) {
  RowDescriptor rd_output({types::DataType::INT64});
  RowBatch output_rb(rd_output, input_rb_->num_rows());

  auto se = ScalarExpressionOf(kAddScalarFuncConstPbtxt);
  RunEvaluator({se}, &output_rb);

  auto out_col = output_rb.ColumnAt(0);
  EXPECT_EQ(3, out_col->length());
  auto casted = static_cast<arrow::Int64Array*>(out_col.get());
  EXPECT_EQ(1338, casted->Value(0));
  EXPECT_EQ(1339, casted->Value(1));
  EXPECT_EQ(1340, casted->Value(2));
}

TEST_P(ScalarExpressionTest, eval_add_nested) {
  RowDescriptor rd_output({types::DataType::INT64});
  RowBatch output_rb(rd_output, input_rb_->num_rows());

  auto se = ScalarExpressionOf(kAddScalarFuncNestedPbtxt);
  RunEvaluator({se}, &output_rb);

  auto out_col = output_rb.ColumnAt(0);
  EXPECT_EQ(3, out_col->length());
  auto casted = static_cast<arrow::Int64Array*>(out_col.get());
  EXPECT_EQ(1341, casted->Value(0));
  EXPECT_EQ(1343, casted->Value(1));
  EXPECT_EQ(1345, casted->Value(2));
}

TEST_P(ScalarExpressionTest, eval_uint128_constant) {
  RowDescriptor rd_output({types::DataType::UINT128});
  RowBatch output_rb(rd_output, input_rb_->num_rows());

  auto se = UInt128ConstScalarExpr();
  RunEvaluator({se}, &output_rb);

  auto out_col = output_rb.ColumnAt(0);
  EXPECT_EQ(3, out_col->length());
  auto casted = static_cast<arrow::UInt128Array*>(out_col.get());
  EXPECT_EQ(types::UInt128Value(123, 456), casted->Value(0));
  EXPECT_EQ(types::UInt128Value(123, 456), casted->Value(1));
  EXPECT_EQ(types::UInt128Value(123, 456), casted->Value(2));
}

constexpr char kInitArgScalarFunc[] = R"pb(
func {
  name: "init_arg"
  id: 1
  args {
    column {
      node: 0
      index: 2
    }
  }
  init_args {
     data_type: STRING,
     string_value: "init_arg"
  }
  init_args {
    data_type: INT64
    int64_value: 1234
  }
  args_data_types: STRING
}
)pb";

TEST_P(ScalarExpressionTest, eval_init_arg_udf) {
  RowDescriptor rd_output({types::DataType::STRING});
  RowBatch output_rb(rd_output, input_rb_->num_rows());

  auto se = ScalarExpressionOf(kInitArgScalarFunc);
  RunEvaluator({se}, &output_rb);

  auto out_col = output_rb.ColumnAt(0);
  EXPECT_EQ(3, out_col->length());
  auto casted = static_cast<arrow::StringArray*>(out_col.get());
  EXPECT_EQ("init_arg, 1234, a", casted->GetString(0));
  EXPECT_EQ("init_arg, 1234, b", casted->GetString(1));
  EXPECT_EQ("init_arg, 1234, c", casted->GetString(2));
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
