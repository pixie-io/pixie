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

#include <arrow/array.h>
#include <arrow/memory_pool.h>
#include <benchmark/benchmark.h>
#include <cstddef>
#include <memory>
#include <vector>

#include <google/protobuf/text_format.h>
#include <sole.hpp>

#include "src/carnot/exec/exec_state.h"
#include "src/carnot/exec/expression_evaluator.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/plan/plan_state.h"
#include "src/carnot/plan/scalar_expression.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf/base.h"
#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"
#include "src/common/base/base.h"
#include "src/datagen/datagen.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/types.h"
#include "src/table_store/table/table_store.h"
#include "src/table_store/table_store.h"

using ScalarExpression = px::carnot::plan::ScalarExpression;
using ScalarExpressionVector = std::vector<std::shared_ptr<ScalarExpression>>;
using px::carnot::exec::ExecState;
using px::carnot::exec::MockMetricsStubGenerator;
using px::carnot::exec::MockResultSinkStubGenerator;
using px::carnot::exec::MockTraceStubGenerator;
using px::carnot::exec::ScalarExpressionEvaluator;
using px::carnot::exec::ScalarExpressionEvaluatorType;
using px::carnot::planpb::testutils::kAddScalarFuncNestedPbtxt;
using px::carnot::planpb::testutils::kAddScalarFuncPbtxt;
using px::carnot::planpb::testutils::kColumnReferencePbtxt;
using px::carnot::planpb::testutils::kScalarInt64ValuePbtxt;
using px::carnot::udf::FunctionContext;
using px::carnot::udf::Registry;
using px::carnot::udf::ScalarUDF;
using px::table_store::schema::RowBatch;
using px::table_store::schema::RowDescriptor;
using px::types::DataType;
using px::types::Int64Value;
using px::types::ToArrow;

class AddUDF : public ScalarUDF {
 public:
  Int64Value Exec(FunctionContext*, Int64Value v1, Int64Value v2) { return v1.val + v2.val; }
};

// NOLINTNEXTLINE : runtime/references.
void BM_ScalarExpressionTwoCols(benchmark::State& state,
                                const ScalarExpressionEvaluatorType& eval_type, const char* pbtxt) {
  px::carnot::planpb::ScalarExpression se_pb;
  size_t data_size = state.range(0);

  google::protobuf::TextFormat::MergeFromString(pbtxt, &se_pb);
  auto s_or_se = px::carnot::plan::ScalarExpression::FromProto(se_pb);
  CHECK(s_or_se.ok());
  std::shared_ptr<ScalarExpression> se = s_or_se.ConsumeValueOrDie();

  auto func_registry = std::make_unique<Registry>("test_registry");
  auto table_store = std::make_shared<px::table_store::TableStore>();
  PX_CHECK_OK(func_registry->Register<AddUDF>("add"));
  auto exec_state = std::make_unique<ExecState>(
      func_registry.get(), table_store, MockResultSinkStubGenerator, MockMetricsStubGenerator,
      MockTraceStubGenerator, sole::uuid4(), nullptr);
  EXPECT_OK(exec_state->AddScalarUDF(
      0, "add",
      std::vector<px::types::DataType>({px::types::DataType::INT64, px::types::DataType::INT64})));

  auto in1 = px::datagen::CreateLargeData<Int64Value>(data_size);
  auto in2 = px::datagen::CreateLargeData<Int64Value>(data_size);

  RowDescriptor rd({DataType::INT64, DataType::INT64});
  auto input_rb = std::make_unique<RowBatch>(rd, in1.size());

  PX_CHECK_OK(input_rb->AddColumn(ToArrow(in1, arrow::default_memory_pool())));
  PX_CHECK_OK(input_rb->AddColumn(ToArrow(in2, arrow::default_memory_pool())));
  // NOLINTNEXTLINE : clang-analyzer-deadcode.DeadStores.
  for (auto _ : state) {
    RowDescriptor rd_output({DataType::INT64});
    RowBatch output_rb(rd_output, input_rb->num_rows());
    auto function_ctx = std::make_unique<px::carnot::udf::FunctionContext>(nullptr, nullptr);
    auto evaluator = ScalarExpressionEvaluator::Create({se}, eval_type, function_ctx.get());
    PX_CHECK_OK(evaluator->Open(exec_state.get()));
    PX_CHECK_OK(evaluator->Evaluate(exec_state.get(), *input_rb, &output_rb));
    PX_CHECK_OK(evaluator->Close(exec_state.get()));

    benchmark::DoNotOptimize(output_rb);
    CHECK_EQ(static_cast<size_t>(output_rb.ColumnAt(0)->length()), data_size);
  }
  state.SetBytesProcessed(int64_t(state.iterations()) * 2 * in1.size() * sizeof(int64_t));
}

BENCHMARK_CAPTURE(BM_ScalarExpressionTwoCols, eval_col_arrow,
                  ScalarExpressionEvaluatorType::kArrowNative, kColumnReferencePbtxt)
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);
BENCHMARK_CAPTURE(BM_ScalarExpressionTwoCols, eval_col_native,
                  ScalarExpressionEvaluatorType::kVectorNative, kColumnReferencePbtxt)
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

BENCHMARK_CAPTURE(BM_ScalarExpressionTwoCols, eval_const_arrow,
                  ScalarExpressionEvaluatorType::kArrowNative, kScalarInt64ValuePbtxt)
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);
BENCHMARK_CAPTURE(BM_ScalarExpressionTwoCols, eval_const_native,
                  ScalarExpressionEvaluatorType::kVectorNative, kScalarInt64ValuePbtxt)
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

BENCHMARK_CAPTURE(BM_ScalarExpressionTwoCols, two_cols_add_nested_arrow,
                  ScalarExpressionEvaluatorType::kArrowNative, kAddScalarFuncNestedPbtxt)
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);
BENCHMARK_CAPTURE(BM_ScalarExpressionTwoCols, two_cols_add_nested_native,
                  ScalarExpressionEvaluatorType::kVectorNative, kAddScalarFuncNestedPbtxt)
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);

BENCHMARK_CAPTURE(BM_ScalarExpressionTwoCols, two_cols_simple_add_arrow,
                  ScalarExpressionEvaluatorType::kArrowNative, kAddScalarFuncNestedPbtxt)
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);
BENCHMARK_CAPTURE(BM_ScalarExpressionTwoCols, two_cols_simple_add_vector,
                  ScalarExpressionEvaluatorType::kVectorNative, kAddScalarFuncNestedPbtxt)
    ->RangeMultiplier(2)
    ->Range(1, 1 << 16);
