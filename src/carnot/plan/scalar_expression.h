#pragma once
#include <glog/logging.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "src/carnot/plan/compiler_state.h"
#include "src/carnot/plan/proto/plan.pb.h"
#include "src/carnot/plan/schema.h"
#include "src/utils/error.h"
#include "src/utils/status.h"
#include "src/utils/statusor.h"

namespace pl {
namespace carnot {
namespace plan {

// Forward declare columns since we need it for some functions in ScalarExpression.
class Column;

inline std::string ToString(const planpb::ScalarExpression::ValueCase &exp) {
  switch (exp) {
    case planpb::ScalarExpression::kFunc:
      return "Function";
    case planpb::ScalarExpression::kColumn:
      return "Column";
    case planpb::ScalarExpression::kConstant:
      return "Value";
    default:
      std::string err_msg = absl::StrFormat("Unknown expression type: %d", static_cast<int>(exp));
      DCHECK(0) << err_msg;
      LOG(ERROR) << err_msg;
      return "UnknownExp";
  }
}
/**
 * ScalarExpression is a pure-virtual interface class that defines an expression that can
 * have at most one output value. A ScalarExpression may contain other ScalarExpressions.
 */
class ScalarExpression {
 public:
  static StatusOr<std::unique_ptr<ScalarExpression>> FromProto(const planpb::ScalarExpression &pb);

  virtual ~ScalarExpression() = default;
  bool is_initialized() const { return is_initialized_; }

  /**
   * Compute the output data type of the expression.
   *
   * @param input_schema The input schema which is used to look up potential dependencies.
   * @return Either an Status or the compute data type.
   */
  virtual StatusOr<carnotpb::DataType> OutputDataType(const CompilerState &state,
                                                      const Schema &input_schema) const = 0;

  /**
   * Computes the column dependencies and returns a reference to all of them. No
   * ownership transfer is done for the columns and the lifetime is the same as the
   * parent class.
   *
   * @return A list of column dependencies, if any.
   */
  virtual std::vector<const Column *> ColumnDeps() = 0;

  /**
   * Get a list of the dependent ScalarExpression. For some expressions this might be
   * an empty list.
   *
   * @return All dependent ScalarExpressions. The lifetime is the same as the parent class.
   */
  virtual std::vector<ScalarExpression *> Deps() const = 0;

  /**
   * ExpressionType gets the column type from the proto.
   * @return Expression ValueCase.
   */
  virtual planpb::ScalarExpression::ValueCase ExpressionType() const = 0;

  /**
   * Generate a debug string.
   * @return information about the underlying expression.
   */
  virtual std::string DebugString() const = 0;

 protected:
  ScalarExpression() {}
  bool is_initialized_ = false;
};

using ScalarExpressionPtrVector = std::vector<std::unique_ptr<ScalarExpression>>;

/**
 * Column is a type of ScalarExpression that refers to a single column output of a
 * particular operator.
 */
class Column : public ScalarExpression {
 public:
  Column() : ScalarExpression() {}
  virtual ~Column() = default;

  /// Initializes the column value based on the passed in protobuf msg.
  Status Init(const planpb::Column &pb);

  // Implementation of base class methods.
  StatusOr<carnotpb::DataType> OutputDataType(const CompilerState &state,
                                              const Schema &input_schema) const override;
  std::vector<const Column *> ColumnDeps() override;
  std::vector<ScalarExpression *> Deps() const override;
  planpb::ScalarExpression::ValueCase ExpressionType() const override;
  std::string DebugString() const override;

  /// The index of the column in the operator that is referenced.
  int64_t Index() const;

  /// NodeID gets the ID of the operator that this column refers to.
  int64_t NodeID() const;

 private:
  planpb::Column pb_;
};

/**
 * A ScalarValue is just a single constant value.
 */
class ScalarValue : public ScalarExpression {
 public:
  ScalarValue() : ScalarExpression() {}
  virtual ~ScalarValue() = default;

  /// Initializes the constant scalar value based on the passed in protobuf msg.
  Status Init(const planpb::ScalarValue &pb);

  // Override base class methods.
  std::vector<const Column *> ColumnDeps() override;
  StatusOr<carnotpb::DataType> OutputDataType(const CompilerState &state,
                                              const Schema &input_schema) const override;
  std::vector<ScalarExpression *> Deps() const override;
  planpb::ScalarExpression::ValueCase ExpressionType() const override;
  std::string DebugString() const override;

  /// Returns the data type of the constant value.
  carnotpb::DataType DataType() const { return pb_.data_type(); }
  // Accessor functions that return the value based on type.
  // If the wrong function is called then an invalid value may be returned.
  bool BoolValue() const;
  int64_t Int64Value() const;
  double Float64Value() const;
  std::string StringValue() const;
  bool IsNull() const;

 private:
  planpb::ScalarValue pb_;
};

class ScalarFunc : public ScalarExpression {
 public:
  ScalarFunc() : ScalarExpression() {}
  virtual ~ScalarFunc() = default;

  Status Init(const planpb::ScalarFunc &pb);
  // Override base class methods.
  std::vector<const Column *> ColumnDeps() override;
  StatusOr<carnotpb::DataType> OutputDataType(const CompilerState &state,
                                              const Schema &input_schema) const override;
  std::vector<ScalarExpression *> Deps() const override;
  planpb::ScalarExpression::ValueCase ExpressionType() const override;
  std::string DebugString() const override;

  std::string name() const { return name_; }
  const ScalarExpressionPtrVector &arg_deps() const { return arg_deps_; }

 private:
  std::string name_;
  ScalarExpressionPtrVector arg_deps_;
};

/**
 * A walker for scalar expressions that allows returns types.
 *
 * The walker performs a post order walk of the expression graph. A sample usage
 * is:
 * To count the number of functions:
 *    ScalarExpressionWalker<int>()
 *       .OnScalarFunc([](const ScalarFunc& func, const std::vector<int>& child_values) {
 *          int child_values_sum = 0;
 *          for (int child_value: child_values) {
 *             child_values_sum += child_value;
 *          }
 *          return 1 + child_value;
 *       });
 * A return type is not optional.
 * @tparam TReturn The return type of individual walk functions.
 */
template <typename TReturn>
class ScalarExpressionWalker {
 public:
  /**
   * ScalarExpression Walker functions are just functions that get called whenever
   * a particular scalar expression is seen. They are required to return a value
   * and they get a list of all the children values.
   */
  template <typename TExpression>
  using ScalarExpressionWalkFn =
      std::function<TReturn(const TExpression &, const std::vector<TReturn> &)>;

  using ScalarFuncWalkFn = ScalarExpressionWalkFn<ScalarFunc>;
  using ScalarValueWalkFn = ScalarExpressionWalkFn<ScalarValue>;
  using ColumnWalkFn = ScalarExpressionWalkFn<Column>;

  /**
   * Register callback for when a scalar func is encountered.
   * @param fn The function to call when a ScalarFunc is encountered.
   * @return self to allow chaining
   */
  ScalarExpressionWalker &OnScalarFunc(const ScalarFuncWalkFn &fn) {
    scalar_func_walk_fn_ = fn;
    return *this;
  }

  /**
   * Register callback for when a ScalarValue is encountered.
   * @param fn The function to call when a ScalarValue is encountered.
   * @return self to allow chaining.
   */
  ScalarExpressionWalker &OnScalarValue(const ScalarValueWalkFn &fn) {
    scalar_value_walk_fn_ = fn;
    return *this;
  }

  /**
   * Register callback for when a Column is encountered.
   * @param fn The function to call when a Column is encountered.
   * @return self to allow chaining.
   */
  ScalarExpressionWalker &OnColumn(const ColumnWalkFn &fn) {
    column_walk_fn_ = fn;
    return *this;
  }

  /**
   * Perform a post order walk of the expression graph.
   * @param expression The expression to walk.
   * @return A StatusOr where the valid value is the registered return type.
   */
  StatusOr<TReturn> Walk(const ScalarExpression &expression) {
    std::vector<TReturn> child_values;
    for (const auto *child : expression.Deps()) {
      auto res = Walk(*child);
      if (!res.ok() && !error::IsNotFound(res.status())) {
        return res;
      }
      if (res.ok()) {
        child_values.emplace_back(res.ValueOrDie());
      }
    }
    return CallWalkFn(expression, child_values);
  }

 private:
  template <typename T, typename TWalkFunc>
  StatusOr<TReturn> CallAs(const TWalkFunc &fn, const ScalarExpression &expression,
                           std::vector<TReturn> child_values) {
    if (!fn) {
      return error::NotFound("no func");
    }
    return fn(static_cast<const T &>(expression), child_values);
  }

  StatusOr<TReturn> CallWalkFn(const ScalarExpression &expression,
                               std::vector<TReturn> child_values) {
    const auto expression_type = expression.ExpressionType();
    switch (expression_type) {
      case planpb::ScalarExpression::kColumn:
        return CallAs<Column>(column_walk_fn_, expression, child_values);
      case planpb::ScalarExpression::kConstant:
        return CallAs<ScalarValue>(scalar_value_walk_fn_, expression, child_values);
      case planpb::ScalarExpression::kFunc:
        return CallAs<ScalarFunc>(scalar_func_walk_fn_, expression, child_values);
      default:
        return error::InvalidArgument("Expression type: $0 is invalid", expression_type);
    }
  }

  ScalarFuncWalkFn scalar_func_walk_fn_;
  ScalarValueWalkFn scalar_value_walk_fn_;
  ColumnWalkFn column_walk_fn_;
};

}  // namespace plan
}  // namespace carnot
}  // namespace pl
