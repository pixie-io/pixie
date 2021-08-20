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

#pragma once

#include <arrow/array.h>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/exec/exec_state.h"
#include "src/carnot/plan/scalar_expression.h"
#include "src/carnot/udf/base.h"
#include "src/carnot/udf/udf.h"
#include "src/common/base/base.h"
#include "src/shared/types/column_wrapper.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace exec {

std::shared_ptr<arrow::Array> EvalScalarToArrow(ExecState* exec_state, const plan::ScalarValue& val,
                                                size_t count);

std::shared_ptr<types::ColumnWrapper> EvalScalarToColumnWrapper(ExecState*,
                                                                const plan::ScalarValue& val,
                                                                size_t count);

/**
 * Base expression evaluator class.
 */
class ExpressionEvaluator {
 public:
  ExpressionEvaluator() = default;
  virtual ~ExpressionEvaluator() = default;

  /**
   * Open should be called once execution.
   * Memory allocation and setup should be handled here.
   * @param exec_state The execution state.
   * @return Status of Open.
   */
  virtual Status Open(ExecState* exec_state) = 0;

  /**
   * Evaluate should be called once per row batch.
   * @param exec_state The execution state.
   * @param input The input RowBatch.
   * @param output A pointer to the output Rowbatch. This function expects a valid output RowBatch.
   * @return Status of the evaluation.
   */
  virtual Status Evaluate(ExecState* exec_state, const table_store::schema::RowBatch& input,
                          table_store::schema::RowBatch* output) = 0;

  /**
   * Close should be called when this evaluator will no longer be used. Calling Evaluate or Open
   * after close is called is an error.
   * @param exec_state The execution state.
   * @return Status of closing.
   */
  virtual Status Close(ExecState* exec_state) = 0;

  virtual std::string DebugString() = 0;
};

/**
 * ScalarExpressionEvaluatorType is an enum can be used to select different underlying
 * expression evaluators.
 */
enum class ScalarExpressionEvaluatorType : uint8_t {
  kVectorNative = 0,
  kArrowNative = 1,
};

/**
 * Base class for all scalar expression evaluators.
 */
class ScalarExpressionEvaluator : public ExpressionEvaluator {
 public:
  explicit ScalarExpressionEvaluator(plan::ConstScalarExpressionVector expressions,
                                     udf::FunctionContext* function_ctx)
      : expressions_(std::move(expressions)), function_ctx_(function_ctx) {}

  /**
   * Creates a new Scalar expression evaluator.
   * @param expressions The expressions to evaluate.
   * @param type The type of scalar expression evaluator to create.
   * @return A unique_ptr to a ScalarExpressionEvaluator.
   */
  static std::unique_ptr<ScalarExpressionEvaluator> Create(
      const plan::ConstScalarExpressionVector& expressions,
      const ScalarExpressionEvaluatorType& type, udf::FunctionContext* function_ctx);

  Status Evaluate(ExecState* exec_state, const table_store::schema::RowBatch& input,
                  table_store::schema::RowBatch* output) override;
  std::string DebugString() override;

 protected:
  // Function called for each individual expression in expressions_.
  // Implement in derived class.
  virtual Status EvaluateSingleExpression(ExecState* exec_state,
                                          const table_store::schema::RowBatch& input,
                                          const plan::ScalarExpression& expr,
                                          table_store::schema::RowBatch* output) = 0;
  Status InitFuncsInExpression(ExecState* exec_state,
                               std::shared_ptr<const plan::ScalarExpression> expr);
  plan::ConstScalarExpressionVector expressions_;
  udf::FunctionContext* function_ctx_ = nullptr;
  std::map<int64_t, std::unique_ptr<udf::ScalarUDF>> id_to_udf_map_;
};

/**
 * A scalar expression evaluator thar uses native C++ vectors for intermediate state.
 * (The input is always assumed to be RowBatches with arrow::Arrays).
 */
class VectorNativeScalarExpressionEvaluator : public ScalarExpressionEvaluator {
 public:
  explicit VectorNativeScalarExpressionEvaluator(
      const plan::ConstScalarExpressionVector& expressions, udf::FunctionContext* function_ctx)
      : ScalarExpressionEvaluator(expressions, function_ctx) {}

  Status Open(ExecState* exec_state) override;
  Status Close(ExecState* exec_state) override;

  StatusOr<types::SharedColumnWrapper> EvaluateSingleExpression(
      ExecState* exec_state, const table_store::schema::RowBatch& input,
      const plan::ScalarExpression& expr);

 protected:
  Status EvaluateSingleExpression(ExecState* exec_state, const table_store::schema::RowBatch& input,
                                  const plan::ScalarExpression& expr,
                                  table_store::schema::RowBatch* output) override;
};

/**
 * A scalar expression evaluator that uses Arrow arrays for intermediate state.
 */
class ArrowNativeScalarExpressionEvaluator : public ScalarExpressionEvaluator {
 public:
  explicit ArrowNativeScalarExpressionEvaluator(
      const plan::ConstScalarExpressionVector& expressions, udf::FunctionContext* function_ctx)
      : ScalarExpressionEvaluator(expressions, function_ctx) {}

  Status Open(ExecState* exec_state) override;
  Status Close(ExecState* exec_state) override;

 protected:
  Status EvaluateSingleExpression(ExecState* exec_state, const table_store::schema::RowBatch& input,
                                  const plan::ScalarExpression& expr,
                                  table_store::schema::RowBatch* output) override;
};

}  // namespace exec
}  // namespace carnot
}  // namespace px
