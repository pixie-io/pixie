#include <benchmark/benchmark.h>
#include <glog/logging.h>
#include <google/protobuf/text_format.h>

#include "src/carnot/exec/expression_evaluator.h"
#include "src/carnot/plan/scalar_expression.h"
#include "src/carnot/proto/plan.pb.h"
#include "src/carnot/proto/test_proto.h"
#include "src/carnot/udf/arrow_adapter.h"
#include "src/carnot/udf/udf.h"
#include "src/common/macros.h"
#include "src/common/status.h"
#include "src/utils/benchmark/utils.h"

using ScalarExpression = pl::carnot::plan::ScalarExpression;
using ScalarExpressionVector = std::vector<std::shared_ptr<ScalarExpression>>;
using pl::carnot::carnotpb::testutils::kAddScalarFuncNestedPbtxt;
using pl::carnot::carnotpb::testutils::kAddScalarFuncPbtxt;
using pl::carnot::carnotpb::testutils::kColumnReferencePbtxt;
using pl::carnot::carnotpb::testutils::kScalarInt64ValuePbtxt;
using pl::carnot::exec::ExecState;
using pl::carnot::exec::RowBatch;
using pl::carnot::exec::RowDescriptor;
using pl::carnot::exec::ScalarExpressionEvaluator;
using pl::carnot::exec::ScalarExpressionEvaluatorType;
using pl::carnot::udf::FunctionContext;
using pl::carnot::udf::Int64Value;
using pl::carnot::udf::ScalarUDF;
using pl::carnot::udf::ScalarUDFRegistry;
using pl::carnot::udf::ToArrow;
using pl::carnot::udf::UDFDataType;

class AddUDF : public ScalarUDF {
 public:
  Int64Value Exec(FunctionContext*, Int64Value v1, Int64Value v2) { return v1.val + v2.val; }
};

// NOLINTNEXTLINE(runtime/references)
void BM_ScalarExpressionTwoCols(benchmark::State& state,
                                const ScalarExpressionEvaluatorType& eval_type, const char* pbtxt) {
  pl::carnot::carnotpb::ScalarExpression se_pb;
  size_t data_size = state.range(0);

  google::protobuf::TextFormat::MergeFromString(pbtxt, &se_pb);
  auto s_or_se = pl::carnot::plan::ScalarExpression::FromProto(se_pb);
  CHECK(s_or_se.ok());
  std::shared_ptr<ScalarExpression> se = s_or_se.ConsumeValueOrDie();

  auto registry = std::make_shared<ScalarUDFRegistry>("test_registry");
  PL_CHECK_OK(registry->Register<AddUDF>("add"));
  auto exec_state = std::make_unique<ExecState>(registry);

  auto in1 = pl::bmutils::CreateLargeData<Int64Value>(data_size);
  auto in2 = pl::bmutils::CreateLargeData<Int64Value>(data_size);

  RowDescriptor rd({UDFDataType::INT64, UDFDataType::INT64});
  auto input_rb = std::make_unique<RowBatch>(rd, in1.size());

  PL_CHECK_OK(input_rb->AddColumn(ToArrow(in1, arrow::default_memory_pool())));
  PL_CHECK_OK(input_rb->AddColumn(ToArrow(in2, arrow::default_memory_pool())));

  for (auto _ : state) {
    RowDescriptor rd_output({UDFDataType::INT64});
    RowBatch output_rb(rd_output, input_rb->num_rows());

    auto evaluator = ScalarExpressionEvaluator::Create({se}, eval_type);
    PL_CHECK_OK(evaluator->Open(exec_state.get()));
    PL_CHECK_OK(evaluator->Evaluate(exec_state.get(), *input_rb, &output_rb));
    PL_CHECK_OK(evaluator->Close(exec_state.get()));

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
