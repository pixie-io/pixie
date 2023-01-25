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

#include <arrow/array.h>
#include <arrow/builder.h>
#include <arrow/memory_pool.h>
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <ostream>
#include <vector>

#include <absl/strings/str_join.h>
#include <absl/strings/substitute.h>

#include "src/carnot/exec/exec_state.h"
#include "src/carnot/udf/udf_definition.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/types.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"

namespace px {
namespace carnot {
namespace exec {

// PX_CARNOT_UPDATE_FOR_NEW_TYPES
using table_store::schema::CopyValueRepeated;
using table_store::schema::RowBatch;
using types::ArrowToDataType;
using types::BaseValueType;
using types::BoolValueColumnWrapper;
using types::ColumnWrapper;
using types::DataType;
using types::DataTypeTraits;
using types::Float64ValueColumnWrapper;
using types::GetArrowBuilder;
using types::Int64ValueColumnWrapper;
using types::MakeArrowBuilder;
using types::SharedColumnWrapper;
using types::StringValueColumnWrapper;
using types::Time64NSValueColumnWrapper;

std::unique_ptr<ScalarExpressionEvaluator> ScalarExpressionEvaluator::Create(
    const plan::ConstScalarExpressionVector& expressions, const ScalarExpressionEvaluatorType& type,
    udf::FunctionContext* function_ctx) {
  switch (type) {
    case ScalarExpressionEvaluatorType::kVectorNative:
      return std::make_unique<VectorNativeScalarExpressionEvaluator>(expressions, function_ctx);
    case ScalarExpressionEvaluatorType::kArrowNative:
      return std::make_unique<ArrowNativeScalarExpressionEvaluator>(expressions, function_ctx);
    default:
      CHECK(0) << "Unknown expression type";
  }
}

namespace {
// Evaluate a scalar value to an arrow::Array.
template <types::DataType T>
std::shared_ptr<arrow::Array> EvalScalar(
    arrow::MemoryPool* mem_pool, const typename px::types::DataTypeTraits<T>::native_type& val,
    size_t count) {
  auto builder = GetArrowBuilder<T>(mem_pool);
  PX_CHECK_OK(builder->Reserve(count));
  PX_CHECK_OK(CopyValueRepeated<T>(builder.get(), val, count));
  std::shared_ptr<arrow::Array> arr;
  PX_CHECK_OK(builder->Finish(&arr));
  return arr;
}

}  // namespace

// Evaluate Scalar to arrow.
// PX_CARNOT_UPDATE_FOR_NEW_TYPES.
std::shared_ptr<arrow::Array> EvalScalarToArrow(ExecState* exec_state, const plan::ScalarValue& val,
                                                size_t count) {
  auto mem_pool = exec_state->exec_mem_pool();
  switch (val.DataType()) {
    case types::BOOLEAN:
      return EvalScalar<DataType::BOOLEAN>(mem_pool, val.BoolValue(), count);
    case types::INT64:
      return EvalScalar<DataType::INT64>(mem_pool, val.Int64Value(), count);
    case types::FLOAT64:
      return EvalScalar<DataType::FLOAT64>(mem_pool, val.Float64Value(), count);
    case types::STRING:
      return EvalScalar<DataType::STRING>(mem_pool, val.StringValue(), count);
    case types::TIME64NS:
      return EvalScalar<DataType::TIME64NS>(mem_pool, val.Time64NSValue(), count);
    case types::UINT128:
      return EvalScalar<DataType::UINT128>(mem_pool, val.UInt128Value(), count);
    default:
      CHECK(0) << "Unknown data type";
  }
}

// Eval scalar value to type erased column wrapper.
// PX_CARNOT_UPDATE_FOR_NEW_TYPES.
std::shared_ptr<ColumnWrapper> EvalScalarToColumnWrapper(ExecState*, const plan::ScalarValue& val,
                                                         size_t count) {
  switch (val.DataType()) {
    case types::BOOLEAN:
      return std::make_shared<types::BoolValueColumnWrapper>(count, val.BoolValue());
    case types::INT64:
      return std::make_shared<types::Int64ValueColumnWrapper>(count, val.Int64Value());
    case types::FLOAT64:
      return std::make_shared<types::Float64ValueColumnWrapper>(count, val.Float64Value());
    case types::STRING:
      return std::make_shared<types::StringValueColumnWrapper>(count, val.StringValue());
    case types::TIME64NS:
      return std::make_shared<types::Time64NSValueColumnWrapper>(
          count, types::Time64NSValue(val.Time64NSValue()));
    case types::UINT128:
      return std::make_shared<types::UInt128ValueColumnWrapper>(count, val.UInt128Value());
    default:
      CHECK(0) << "Unknown data type";
  }
}

Status ScalarExpressionEvaluator::Evaluate(ExecState* exec_state, const RowBatch& input,
                                           RowBatch* output) {
  CHECK(exec_state != nullptr);
  CHECK(output != nullptr);
  CHECK_EQ(static_cast<size_t>(output->num_columns()), expressions_.size());

  for (const auto& expression : expressions_) {
    PX_RETURN_IF_ERROR(EvaluateSingleExpression(exec_state, input, *expression, output));
  }
  return Status::OK();
}
std::string ScalarExpressionEvaluator::DebugString() {
  std::vector<std::string> debug_strs(expressions_.size());
  std::transform(begin(expressions_), end(expressions_), begin(debug_strs),
                 [](auto val) { return val->DebugString(); });
  return absl::Substitute("ExpressionEvaluator<$0>", absl::StrJoin(debug_strs, ","));
}

Status ScalarExpressionEvaluator::InitFuncsInExpression(
    ExecState* exec_state, std::shared_ptr<const plan::ScalarExpression> expr) {
  plan::ExpressionWalker<bool> walker;
  walker.OnScalarValue([](auto, auto) -> bool { return true; });
  walker.OnColumn([](auto, auto) -> bool { return true; });
  walker.OnScalarFunc([&](const plan::ScalarFunc& fn, const std::vector<bool>&) -> bool {
    auto def = exec_state->GetScalarUDFDefinition(fn.udf_id());
    auto udf = id_to_udf_map_[fn.udf_id()].get();

    std::vector<std::shared_ptr<types::BaseValueType>> init_args;
    for (const auto& scalar_val : fn.init_arguments()) {
      init_args.push_back(scalar_val.ToBaseValueType());
    }
    PX_CHECK_OK(def->ExecInit(udf, function_ctx_, init_args));
    return true;
  });

  PX_RETURN_IF_ERROR(walker.Walk(*expr));
  return Status::OK();
}

Status VectorNativeScalarExpressionEvaluator::Open(ExecState* exec_state) {
  for (const auto& kv : exec_state->id_to_scalar_udf_map()) {
    auto udf = kv.second->Make();
    id_to_udf_map_[kv.first] = std::move(udf);
  }
  for (auto expr : expressions_) {
    PX_RETURN_IF_ERROR(InitFuncsInExpression(exec_state, expr));
  }
  return Status::OK();
}

Status VectorNativeScalarExpressionEvaluator::Close(ExecState*) {
  // Nothing here yet.
  return Status();
}

StatusOr<types::SharedColumnWrapper>
VectorNativeScalarExpressionEvaluator::EvaluateSingleExpression(
    ExecState* exec_state, const RowBatch& input, const plan::ScalarExpression& expr) {
  CHECK(exec_state != nullptr);
  CHECK_GT(input.num_columns(), 0);

  size_t num_rows = input.num_rows();

  // Path for scalar funcs an their dependencies to get evaluated.
  // The Arrow arrays are converted to type erased column wrappers
  // and then evaluated.
  plan::ExpressionWalker<types::SharedColumnWrapper> walker;
  walker.OnScalarValue(
      [&](const plan::ScalarValue& val,
          const std::vector<types::SharedColumnWrapper>& children) -> types::SharedColumnWrapper {
        DCHECK_EQ(children.size(), 0ULL);
        return EvalScalarToColumnWrapper(exec_state, val, num_rows);
      });

  walker.OnColumn(
      [&](const plan::Column& col,
          const std::vector<types::SharedColumnWrapper>& children) -> types::SharedColumnWrapper {
        DCHECK_EQ(children.size(), 0ULL);
        return ColumnWrapper::FromArrow(input.ColumnAt(col.Index()));
      });

  walker.OnScalarFunc(
      [&](const plan::ScalarFunc& fn,
          const std::vector<types::SharedColumnWrapper>& children) -> types::SharedColumnWrapper {
        std::vector<types::DataType> arg_types;
        arg_types.reserve(children.size());
        for (const auto& child : children) {
          arg_types.emplace_back(child->data_type());
        }

        auto def = exec_state->GetScalarUDFDefinition(fn.udf_id());
        auto udf = id_to_udf_map_[fn.udf_id()].get();

        std::vector<const types::ColumnWrapper*> raw_children;
        raw_children.reserve(children.size());
        for (const auto& child : children) {
          raw_children.emplace_back(child.get());
        }
        auto output = types::ColumnWrapper::Make(def->exec_return_type(), num_rows);
        // TODO(zasgar): need a better way to handle errors.
        PX_CHECK_OK(def->ExecBatch(udf, function_ctx_, raw_children, output.get(), num_rows));
        return output;
      });

  return walker.Walk(expr);
}

Status VectorNativeScalarExpressionEvaluator::EvaluateSingleExpression(
    ExecState* exec_state, const RowBatch& input, const plan::ScalarExpression& expr,
    RowBatch* output) {
  CHECK(exec_state != nullptr);
  CHECK(output != nullptr);
  CHECK_GT(input.num_columns(), 0);

  size_t num_rows = input.num_rows();

  // Since this evaluator uses vectors internally and the inputs/outputs
  // always have to be arrow::arrays, we just evaluate the case where the
  // expression is a constant/column without using the expression walker.
  // Fast path for just having a constant.
  if (expr.ExpressionType() == plan::Expression::kConstant) {
    auto scalar_expr = static_cast<const plan::ScalarValue&>(expr);
    auto arr = EvalScalarToArrow(exec_state, scalar_expr, num_rows);
    PX_RETURN_IF_ERROR(output->AddColumn(arr));
    return Status::OK();
  }

  // Fast path for just a column (copy it directly to the output).
  if (expr.ExpressionType() == plan::Expression::kColumn) {
    // Trivial copy reference for arrow column.
    auto col_expr = static_cast<const plan::Column&>(expr);
    PX_RETURN_IF_ERROR(output->AddColumn(input.ColumnAt(col_expr.Index())));
    return Status::OK();
  }

  PX_ASSIGN_OR_RETURN(auto result, VectorNativeScalarExpressionEvaluator::EvaluateSingleExpression(
                                       exec_state, input, expr));
  PX_RETURN_IF_ERROR(output->AddColumn(result->ConvertToArrow(exec_state->exec_mem_pool())));
  return Status::OK();
}

Status ArrowNativeScalarExpressionEvaluator::Open(ExecState* exec_state) {
  for (const auto& kv : exec_state->id_to_scalar_udf_map()) {
    auto udf = kv.second->Make();
    id_to_udf_map_[kv.first] = std::move(udf);
  }
  for (const auto& expr : expressions_) {
    PX_RETURN_IF_ERROR(InitFuncsInExpression(exec_state, expr));
  }
  return Status::OK();
}
Status ArrowNativeScalarExpressionEvaluator::Close(ExecState*) {
  // Nothing here yet.
  return Status();
}

Status exec::ArrowNativeScalarExpressionEvaluator::EvaluateSingleExpression(
    exec::ExecState* exec_state, const RowBatch& input, const plan::ScalarExpression& expr,
    RowBatch* output) {
  size_t num_rows = input.num_rows();
  plan::ExpressionWalker<std::shared_ptr<arrow::Array>> walker;
  walker.OnScalarValue(
      [&](const plan::ScalarValue& val, const std::vector<std::shared_ptr<arrow::Array>>& children)
          -> std::shared_ptr<arrow::Array> {
        DCHECK_EQ(children.size(), 0ULL);
        return EvalScalarToArrow(exec_state, val, num_rows);
      });

  walker.OnColumn(
      [&](const plan::Column& col, const std::vector<std::shared_ptr<arrow::Array>>& children)
          -> std::shared_ptr<arrow::Array> {
        DCHECK_EQ(children.size(), 0ULL);
        return input.ColumnAt(col.Index());
      });

  walker.OnScalarFunc(
      [&](const plan::ScalarFunc& fn, const std::vector<std::shared_ptr<arrow::Array>>& children)
          -> std::shared_ptr<arrow::Array> {
        std::vector<types::DataType> arg_types;
        arg_types.reserve(children.size());
        for (const auto& child : children) {
          arg_types.emplace_back(ArrowToDataType(child->type_id()));
        }

        auto def = exec_state->GetScalarUDFDefinition(fn.udf_id());
        auto udf = id_to_udf_map_[fn.udf_id()].get();

        auto output = MakeArrowBuilder(def->exec_return_type(), arrow::default_memory_pool());

        std::vector<arrow::Array*> raw_children;
        raw_children.reserve(children.size());
        for (const auto& child : children) {
          raw_children.push_back(child.get());
        }

        PX_CHECK_OK(def->ExecBatchArrow(udf, function_ctx_, raw_children, output.get(), num_rows));

        std::shared_ptr<arrow::Array> output_array;
        PX_CHECK_OK(output->Finish(&output_array));
        return output_array;
      });

  PX_ASSIGN_OR_RETURN(auto result, walker.Walk(expr));

  PX_RETURN_IF_ERROR(output->AddColumn(result));
  return Status::OK();
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
