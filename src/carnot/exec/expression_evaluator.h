#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_state.h"
#include "src/carnot/exec/row_batch.h"
#include "src/carnot/plan/scalar_expression.h"
#include "src/common/common.h"

namespace pl {
namespace carnot {
namespace exec {

std::shared_ptr<arrow::Array> EvalScalarToArrow(ExecState* exec_state, const plan::ScalarValue& val,
                                                size_t count);

std::shared_ptr<udf::ColumnWrapper> EvalScalarToColumnWrapper(ExecState*,
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
  virtual Status Evaluate(ExecState* exec_state, const RowBatch& input, RowBatch* output) = 0;

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
  explicit ScalarExpressionEvaluator(const plan::ConstScalarExpressionVector& expressions)
      : ExpressionEvaluator(), expressions_(expressions) {}

  /**
   * Creates a new Scalar expression evaluator.
   * @param expressions The expressions to evaluate.
   * @param type The type of scalar expression evaluator to create.
   * @return A unique_ptr to a ScalarExpressionEvaluator.
   */
  static std::unique_ptr<ScalarExpressionEvaluator> Create(
      const plan::ConstScalarExpressionVector& expressions,
      const ScalarExpressionEvaluatorType& type);

  Status Evaluate(ExecState* exec_state, const RowBatch& input, RowBatch* output) override;
  std::string DebugString() override;

 protected:
  // Function called for each individual expression in expressions_.
  // Implement in derived class.
  virtual Status EvaluateSingleExpression(ExecState* exec_state, const RowBatch& input,
                                          const plan::ScalarExpression& expr, RowBatch* output) = 0;
  plan::ConstScalarExpressionVector expressions_;
};

/**
 * A scalar expression evaluator thar uses native C++ vectors for intermediate state.
 * (The input is always assumed to be RowBatches with arrow::Arrays).
 */
class VectorNativeScalarExpressionEvaluator : public ScalarExpressionEvaluator {
 public:
  explicit VectorNativeScalarExpressionEvaluator(
      const plan::ConstScalarExpressionVector& expressions)
      : ScalarExpressionEvaluator(expressions) {}

  Status Open(ExecState* exec_state) override;
  Status Close(ExecState* exec_state) override;

 protected:
  Status EvaluateSingleExpression(ExecState* exec_state, const RowBatch& input,
                                  const plan::ScalarExpression& expr, RowBatch* output) override;
};

/**
 * A scalar expression evaluator that uses Arrow arrays for intermediate state.
 */
class ArrowNativeScalarExpressionEvaluator : public ScalarExpressionEvaluator {
 public:
  explicit ArrowNativeScalarExpressionEvaluator(
      const plan::ConstScalarExpressionVector& expressions)
      : ScalarExpressionEvaluator(expressions) {}

  Status Open(ExecState* exec_state) override;
  Status Close(ExecState* exec_state) override;

 protected:
  Status EvaluateSingleExpression(ExecState* exec_state, const RowBatch& input,
                                  const plan::ScalarExpression& expr, RowBatch* output) override;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
