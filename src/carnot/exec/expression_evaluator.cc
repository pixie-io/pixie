#include <arrow/array.h>
#include <arrow/buffer.h>
#include <arrow/builder.h>
#include <memory>

#include "src/carnot/exec/exec_state.h"
#include "src/carnot/exec/expression_evaluator.h"
#include "src/carnot/udf/arrow_adapter.h"
#include "src/carnot/udf/udf.h"

namespace pl {
namespace carnot {
namespace exec {

using udf::ArrowToCarnotType;
using udf::BoolValueColumnWrapper;
using udf::ColumnWrapper;
using udf::Float64ValueColumnWrapper;
using udf::Int64ValueColumnWrapper;
using udf::MakeArrowBuilder;
using udf::SharedColumnWrapper;
using udf::StringValueColumnWrapper;
using udf::UDFBaseValue;
using udf::UDFDataType;
using udf::UDFDataTypeTraits;

std::unique_ptr<ScalarExpressionEvaluator> ScalarExpressionEvaluator::Create(
    const plan::ConstScalarExpressionVector &expressions,
    const ScalarExpressionEvaluatorType &type) {
  switch (type) {
    case ScalarExpressionEvaluatorType::kVectorNative:
      return std::make_unique<VectorNativeScalarExpressionEvaluator>(expressions);
    case ScalarExpressionEvaluatorType::kArrowNative:
      return std::make_unique<ArrowNativeScalarExpressionEvaluator>(expressions);
  }
}

namespace {

// Evaluate a scalar value to an arrow::Array.
template <typename TBuilder, typename TArray, typename T>
std::shared_ptr<arrow::Array> EvalScalarFixedImpl(arrow::MemoryPool *mem_pool, T val,
                                                  size_t count) {
  TBuilder builder(mem_pool);
  CHECK(builder.Reserve(count).ok());
  for (size_t i = 0; i < count; ++i) {
    builder.UnsafeAppend(val);
  }
  std::shared_ptr<arrow::Array> arr;
  CHECK(builder.Finish(&arr).ok());
  return arr;
}

// Specialization for binary types.
template <typename TBuilder, typename TArray, typename T>
std::shared_ptr<arrow::Array> EvalScalarBinaryImpl(arrow::MemoryPool *mem_pool, T val,
                                                   size_t count) {
  TBuilder builder(mem_pool);
  CHECK(builder.Reserve(count).ok());
  CHECK(builder.ReserveData(count * val.size()).ok());

  for (size_t i = 0; i < count; ++i) {
    builder.UnsafeAppend(val);
  }
  std::shared_ptr<arrow::Array> arr;
  CHECK(builder.Finish(&arr).ok());
  return arr;
}

// Evaluate Scalar to arrow.
// PL_CARNOT_UPDATE_FOR_NEW_TYPES.
std::shared_ptr<arrow::Array> EvalScalarToArrow(ExecState *exec_state, const plan::ScalarValue &val,
                                                size_t count) {
  auto mem_pool = exec_state->exec_mem_pool();
  switch (val.DataType()) {
    case types::BOOLEAN:
      return EvalScalarFixedImpl<arrow::BooleanBuilder, arrow::BooleanArray>(
          mem_pool, val.BoolValue(), count);
    case types::INT64:
      return EvalScalarFixedImpl<arrow::Int64Builder, arrow::Int64Array>(mem_pool, val.Int64Value(),
                                                                         count);
    case types::FLOAT64:
      return EvalScalarFixedImpl<arrow::DoubleBuilder, arrow::DoubleArray>(
          mem_pool, val.Float64Value(), count);
    case types::STRING:
      return EvalScalarBinaryImpl<arrow::StringBuilder, arrow::StringArray>(
          mem_pool, val.StringValue(), count);
    default:
      CHECK(0) << "Unknown data type";
  }
}

// Eval scalar value to type erased column wrapper.
// PL_CARNOT_UPDATE_FOR_NEW_TYPES.
std::shared_ptr<ColumnWrapper> EvalScalarToColumnWrapper(ExecState *, const plan::ScalarValue &val,
                                                         size_t count) {
  switch (val.DataType()) {
    case types::BOOLEAN:
      return std::make_shared<BoolValueColumnWrapper>(count, val.BoolValue());
    case types::INT64:
      return std::make_shared<Int64ValueColumnWrapper>(count, val.Int64Value());
    case types::FLOAT64:
      return std::make_shared<Float64ValueColumnWrapper>(count, val.Float64Value());
    case types::STRING:
      return std::make_shared<StringValueColumnWrapper>(count, val.StringValue());
    default:
      CHECK(0) << "Unknown data type";
  }
}

}  // namespace

Status ScalarExpressionEvaluator::Evaluate(ExecState *exec_state, const RowBatch &input,
                                           RowBatch *output) {
  CHECK(exec_state != nullptr);
  CHECK(output != nullptr);
  CHECK_EQ(static_cast<size_t>(output->num_columns()), expressions_.size());

  for (const auto expression : expressions_) {
    PL_RETURN_IF_ERROR(EvaluateSingleExpression(exec_state, input, *expression, output));
  }
  return Status::OK();
}

Status VectorNativeScalarExpressionEvaluator::Open(ExecState *) {
  // Nothing here yet.
  return Status();
}

Status VectorNativeScalarExpressionEvaluator::Close(ExecState *) {
  // Nothing here yet.
  return Status();
}
Status VectorNativeScalarExpressionEvaluator::EvaluateSingleExpression(
    ExecState *exec_state, const RowBatch &input, const plan::ScalarExpression &expr,
    RowBatch *output) {
  CHECK(exec_state != nullptr);
  CHECK(output != nullptr);
  CHECK_GT(input.num_columns(), 0);

  size_t num_rows = input.num_rows();

  // Since this evaluator uses vectors internally and the inputs/outputs
  // always have to be arrow::arrays, we just evaluate the case where the
  // expression is a constant/column without using the expression walker.
  // Fast path for just having a constant.
  if (expr.ExpressionType() == plan::Expression::kConstant) {
    auto scalar_expr = static_cast<const plan::ScalarValue &>(expr);
    auto arr = EvalScalarToArrow(exec_state, scalar_expr, num_rows);
    PL_RETURN_IF_ERROR(output->AddColumn(arr));
    return Status::OK();
  }

  // Fast path for just a column (copy it directly to the output).
  if (expr.ExpressionType() == plan::Expression::kColumn) {
    // Trivial copy reference for arrow column.
    auto col_expr = static_cast<const plan::Column &>(expr);
    PL_RETURN_IF_ERROR(output->AddColumn(input.ColumnAt(col_expr.Index())));
    return Status::OK();
  }

  // Path for scalar funcs an their dependencies to get evaluated.
  // The Arrow arrays are converted to type erased column wrappers
  // and then evaluated.
  plan::ScalarExpressionWalker<SharedColumnWrapper> walker;
  walker.OnScalarValue(
      [&](const plan::ScalarValue &val,
          const std::vector<SharedColumnWrapper> &children) -> SharedColumnWrapper {
        DCHECK_EQ(children.size(), 0ULL);
        return EvalScalarToColumnWrapper(exec_state, val, num_rows);
      });

  walker.OnColumn([&](const plan::Column &col,
                      const std::vector<SharedColumnWrapper> &children) -> SharedColumnWrapper {
    DCHECK_EQ(children.size(), 0ULL);
    return ColumnWrapper::FromArrow(input.ColumnAt(col.Index()));
  });

  walker.OnScalarFunc([&](const plan::ScalarFunc &fn,
                          const std::vector<SharedColumnWrapper> &children) -> SharedColumnWrapper {
    auto registry = exec_state->scalar_udf_registry();
    std::vector<udf::UDFDataType> arg_types;
    arg_types.reserve(children.size());
    for (const auto child : children) {
      arg_types.emplace_back(child->DataType());
    }

    // TODO(zasgar): PL-253 - We should move this into the Open function,
    // but it's a bit complicated because we might have functions with different
    // init_args (when supported).
    auto s_or_def = registry->GetDefinition(fn.name(), arg_types);
    PL_CHECK_OK(s_or_def);
    auto def = s_or_def.ConsumeValueOrDie();
    auto udf = def->Make();

    std::vector<const udf::ColumnWrapper *> raw_children;
    raw_children.reserve(children.size());
    for (const auto child : children) {
      raw_children.emplace_back(child.get());
    }
    auto output = udf::ColumnWrapper::Make(def->exec_return_type(), num_rows);
    // TODO(zasgar): need a better way to handle errors.
    PL_CHECK_OK(def->ExecBatch(udf.get(), nullptr /*ctx*/, raw_children, output.get(), num_rows));
    return output;
  });

  PL_ASSIGN_OR_RETURN(auto result, walker.Walk(expr));
  PL_RETURN_IF_ERROR(output->AddColumn(result->ConvertToArrow(exec_state->exec_mem_pool())));
  return Status::OK();
}

Status ArrowNativeScalarExpressionEvaluator::Open(ExecState *) {
  // Nothing here yet.
  return Status();
}
Status ArrowNativeScalarExpressionEvaluator::Close(ExecState *) {
  // Nothing here yet.
  return Status();
}

Status exec::ArrowNativeScalarExpressionEvaluator::EvaluateSingleExpression(
    exec::ExecState *exec_state, const exec::RowBatch &input, const plan::ScalarExpression &expr,
    exec::RowBatch *output) {
  size_t num_rows = input.num_rows();
  plan::ScalarExpressionWalker<std::shared_ptr<arrow::Array>> walker;
  walker.OnScalarValue(
      [&](const plan::ScalarValue &val, const std::vector<std::shared_ptr<arrow::Array>> &children)
          -> std::shared_ptr<arrow::Array> {
        DCHECK_EQ(children.size(), 0ULL);
        return EvalScalarToArrow(exec_state, val, num_rows);
      });

  walker.OnColumn(
      [&](const plan::Column &col, const std::vector<std::shared_ptr<arrow::Array>> &children)
          -> std::shared_ptr<arrow::Array> {
        DCHECK_EQ(children.size(), 0ULL);
        return input.ColumnAt(col.Index());
      });

  walker.OnScalarFunc(
      [&](const plan::ScalarFunc &fn, const std::vector<std::shared_ptr<arrow::Array>> &children)
          -> std::shared_ptr<arrow::Array> {
        auto registry = exec_state->scalar_udf_registry();
        std::vector<udf::UDFDataType> arg_types;
        arg_types.reserve(children.size());
        for (const auto child : children) {
          arg_types.emplace_back(ArrowToCarnotType(child->type_id()));
        }
        // TODO(zasgar): PL-253 - We should move this into the Open function,
        // but it's a bit complicated because we might have functions with different
        // init_args (when supported).
        auto s_or_def = registry->GetDefinition(fn.name(), arg_types);
        PL_CHECK_OK(s_or_def);
        auto def = s_or_def.ConsumeValueOrDie();
        auto udf = def->Make();

        auto output = MakeArrowBuilder(def->exec_return_type(), arrow::default_memory_pool());

        std::vector<arrow::Array *> raw_children;
        raw_children.reserve(children.size());
        for (const auto &child : children) {
          raw_children.push_back(child.get());
        }

        PL_CHECK_OK(
            def->ExecBatchArrow(udf.get(), nullptr /*ctx*/, raw_children, output.get(), num_rows));

        std::shared_ptr<arrow::Array> output_array;
        PL_CHECK_OK(output->Finish(&output_array));
        return output_array;
      });

  PL_ASSIGN_OR_RETURN(auto result, walker.Walk(expr));

  PL_RETURN_IF_ERROR(output->AddColumn(result));
  return Status::OK();
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
