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

#include <stdint.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <absl/numeric/int128.h>
#include <absl/strings/str_format.h>

#include "src/carnot/plan/plan_node.h"
#include "src/carnot/plan/plan_state.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace plan {

// Forward declare columns since we need it for some functions in ScalarExpression.
class Column;

enum Expression { kFunc, kColumn, kConstant, kAgg };

/**
 * ScalarExpression is a pure-virtual interface class that defines an expression that can
 * have at most one output value. A ScalarExpression may contain other ScalarExpressions.
 */
class ScalarExpression : public PlanNode {
 public:
  static StatusOr<std::unique_ptr<ScalarExpression>> FromProto(const planpb::ScalarExpression& pb);

  ~ScalarExpression() override = default;
  bool is_initialized() const { return is_initialized_; }

  /**
   * Compute the output data type of the expression.
   *
   * @param input_schema The input schema which is used to look up potential dependencies.
   * @return Either an Status or the compute data type.
   */
  virtual StatusOr<types::DataType> OutputDataType(
      const PlanState& state, const table_store::schema::Schema& input_schema) const = 0;

  /**
   * Computes the column dependencies and returns a reference to all of them. No
   * ownership transfer is done for the columns and the lifetime is the same as the
   * parent class.
   *
   * @return A list of column dependencies, if any.
   */
  virtual std::vector<const Column*> ColumnDeps() = 0;

  /**
   * Get a list of the dependent ScalarExpression. For some expressions this might be
   * an empty list.
   *
   * @return All dependent ScalarExpressions. The lifetime is the same as the parent class.
   */
  virtual std::vector<ScalarExpression*> Deps() const = 0;

  /**
   * ExpressionType gets the column type from the proto.
   * @return Expression ValueCase.
   */
  virtual Expression ExpressionType() const = 0;

  /**
   * Generate a debug string.
   * @return information about the underlying expression.
   */
  std::string DebugString() const override = 0;

 protected:
  ScalarExpression() = default;
  bool is_initialized_ = false;
};

using ScalarExpressionPtrVector = std::vector<std::unique_ptr<ScalarExpression>>;

/**
 * Column is a type of ScalarExpression that refers to a single column output of a
 * particular operator.
 */
class Column : public ScalarExpression {
 public:
  Column() = default;
  ~Column() override = default;

  /// Initializes the column value based on the passed in protobuf msg.
  Status Init(const planpb::Column& pb);

  // Implementation of base class methods.
  StatusOr<types::DataType> OutputDataType(
      const PlanState& state, const table_store::schema::Schema& input_schema) const override;
  std::vector<const Column*> ColumnDeps() override;
  std::vector<ScalarExpression*> Deps() const override;
  Expression ExpressionType() const override;
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
  ScalarValue() = default;
  ~ScalarValue() override = default;

  /// Initializes the constant scalar value based on the passed in protobuf msg.
  Status Init(const planpb::ScalarValue& pb);

  // Override base class methods.
  std::vector<const Column*> ColumnDeps() override;
  StatusOr<types::DataType> OutputDataType(
      const PlanState& state, const table_store::schema::Schema& input_schema) const override;
  std::vector<ScalarExpression*> Deps() const override;
  Expression ExpressionType() const override;
  std::string DebugString() const override;

  /// Returns the data type of the constant value.
  types::DataType DataType() const { return pb_.data_type(); }
  // Accessor functions that return the value based on type.
  // If the wrong function is called then an invalid value may be returned.
  bool BoolValue() const;
  int64_t Int64Value() const;
  double Float64Value() const;
  std::string StringValue() const;
  int64_t Time64NSValue() const;
  absl::uint128 UInt128Value() const;
  bool IsNull() const;

  std::shared_ptr<types::BaseValueType> ToBaseValueType() const;

 private:
  planpb::ScalarValue pb_;
};

class ScalarFunc : public ScalarExpression {
 public:
  ScalarFunc() = default;
  ~ScalarFunc() override = default;

  Status Init(const planpb::ScalarFunc& pb);
  // Override base class methods.
  std::vector<const Column*> ColumnDeps() override;
  StatusOr<types::DataType> OutputDataType(
      const PlanState& state, const table_store::schema::Schema& input_schema) const override;
  std::vector<ScalarExpression*> Deps() const override;
  Expression ExpressionType() const override;
  std::string DebugString() const override;

  std::string name() const { return name_; }
  int64_t udf_id() const { return udf_id_; }
  const ScalarExpressionPtrVector& arg_deps() const { return arg_deps_; }
  const std::vector<types::DataType> registry_arg_types() const { return registry_arg_types_; }
  const std::vector<ScalarValue> init_arguments() const { return init_arguments_; }

 private:
  std::string name_;
  int64_t udf_id_;
  ScalarExpressionPtrVector arg_deps_;
  std::vector<types::DataType> registry_arg_types_;
  std::vector<ScalarValue> init_arguments_;
};

class AggregateExpression : public ScalarExpression {
 public:
  AggregateExpression() = default;
  ~AggregateExpression() override = default;

  Status Init(const planpb::AggregateExpression& pb);
  // Override base class methods.
  std::vector<const Column*> ColumnDeps() override;
  StatusOr<types::DataType> OutputDataType(
      const PlanState& state, const table_store::schema::Schema& input_schema) const override;
  std::vector<ScalarExpression*> Deps() const override;
  Expression ExpressionType() const override;
  std::string DebugString() const override;

  std::string name() const { return name_; }
  int64_t uda_id() const { return uda_id_; }
  const ScalarExpressionPtrVector& arg_deps() const { return arg_deps_; }
  const std::vector<types::DataType> registry_arg_types() const { return registry_arg_types_; }
  const std::vector<ScalarValue> init_arguments() const { return init_arguments_; }

 private:
  std::string name_;
  int64_t uda_id_;
  ScalarExpressionPtrVector arg_deps_;  // Args can be ScalarValue or Column.
  std::vector<types::DataType> registry_arg_types_;
  std::vector<ScalarValue> init_arguments_;
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
class ExpressionWalker {
 public:
  /**
   * ScalarExpression Walker functions are just functions that get called whenever
   * a particular scalar expression is seen. They are required to return a value
   * and they get a list of all the children values.
   */
  template <typename TExpression>
  using ExpressionWalkFn = std::function<TReturn(const TExpression&, const std::vector<TReturn>&)>;

  using ScalarFuncWalkFn = ExpressionWalkFn<ScalarFunc>;
  using ScalarValueWalkFn = ExpressionWalkFn<ScalarValue>;
  using ColumnWalkFn = ExpressionWalkFn<Column>;
  using AggregateFuncWalkFn = ExpressionWalkFn<AggregateExpression>;
  /**
   * Register callback for when a scalar func is encountered.
   * @param fn The function to call when a ScalarFunc is encountered.
   * @return self to allow chaining
   */
  ExpressionWalker& OnScalarFunc(const ScalarFuncWalkFn& fn) {
    scalar_func_walk_fn_ = fn;
    return *this;
  }

  /**
   * Register callback for when a ScalarValue is encountered.
   * @param fn The function to call when a ScalarValue is encountered.
   * @return self to allow chaining.
   */
  ExpressionWalker& OnScalarValue(const ScalarValueWalkFn& fn) {
    scalar_value_walk_fn_ = fn;
    return *this;
  }

  /**
   * Register callback for when a Column is encountered.
   * @param fn The function to call when a Column is encountered.
   * @return self to allow chaining.
   */
  ExpressionWalker& OnColumn(const ColumnWalkFn& fn) {
    column_walk_fn_ = fn;
    return *this;
  }

  /**
   * Register callback for when a Aggregate expression is encountered.
   * @param fn The function to call.
   * @return self to allow chaining.
   */
  ExpressionWalker& OnAggregateExpression(const AggregateFuncWalkFn& fn) {
    aggregate_func_walk_fn_ = fn;
    return *this;
  }

  /**
   * Perform a post order walk of the expression graph.
   * @param expression The expression to walk.
   * @return A StatusOr where the valid value is the registered return type.
   */
  StatusOr<TReturn> Walk(const ScalarExpression& expression) {
    std::vector<TReturn> child_values;
    for (const auto* child : expression.Deps()) {
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
  StatusOr<TReturn> CallAs(const TWalkFunc& fn, const ScalarExpression& expression,
                           std::vector<TReturn> child_values) {
    if (!fn) {
      return error::NotFound("no func");
    }
    return fn(static_cast<const T&>(expression), child_values);
  }

  StatusOr<TReturn> CallWalkFn(const ScalarExpression& expression,
                               std::vector<TReturn> child_values) {
    const auto expression_type = expression.ExpressionType();
    switch (expression_type) {
      case Expression::kColumn:
        return CallAs<Column>(column_walk_fn_, expression, child_values);
      case Expression::kConstant:
        return CallAs<ScalarValue>(scalar_value_walk_fn_, expression, child_values);
      case Expression::kFunc:
        return CallAs<ScalarFunc>(scalar_func_walk_fn_, expression, child_values);
      case Expression::kAgg:
        return CallAs<AggregateExpression>(aggregate_func_walk_fn_, expression, child_values);
      default:
        return error::InvalidArgument("Expression type: $0 is invalid", expression_type);
    }
  }

  ScalarFuncWalkFn scalar_func_walk_fn_;
  ScalarValueWalkFn scalar_value_walk_fn_;
  ColumnWalkFn column_walk_fn_;
  AggregateFuncWalkFn aggregate_func_walk_fn_;
};

using ScalarExpressionVector = std::vector<std::shared_ptr<ScalarExpression>>;
using ConstScalarExpressionVector = std::vector<std::shared_ptr<const ScalarExpression>>;

}  // namespace plan
}  // namespace carnot
}  // namespace px
