#pragma once
#include <glog/logging.h>
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/plan/proto/plan.pb.h"
#include "src/carnot/plan/schema.h"
#include "src/utils/status.h"
#include "src/utils/statusor.h"

namespace pl {
namespace carnot {
namespace plan {

// Forward declare columns since we need it for some functions in ScalarExpression.
class Column;

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
  virtual StatusOr<planpb::DataType> OutputDataType(const Schema &input_schema) const = 0;

  /**
   * Computes the column dependencies and returns a pointer to all of them. No
   * ownership transfer is done for the columns and the lifetime is the same as the
   * parent class.
   *
   * @return A list of column dependencies, if any.
   */
  virtual std::vector<Column *> ColumnDeps() = 0;

  /**
   * Get a list of the dependent ScalarExpression. For some expressions this might be
   * an empty list.
   *
   * @return All dependent ScalarExpressions. The lifetime is the same as the parent class.
   */
  virtual std::vector<ScalarExpression *> Deps() = 0;

 protected:
  ScalarExpression() {}

  bool is_initialized_ = false;
};

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
  StatusOr<planpb::DataType> OutputDataType(const Schema &input_schema) const override;
  std::vector<Column *> ColumnDeps() override;
  std::vector<ScalarExpression *> Deps() override;

  /// The index of the column in the operator that is referenced.
  int64_t Index() const;

  /// NodeID gets the ID of the operator that this column refers to.
  int64_t NodeID() const;
  std::string DebugString() const;

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
  std::vector<Column *> ColumnDeps() override;
  StatusOr<planpb::DataType> OutputDataType(const Schema &input_schema) const override;
  std::vector<ScalarExpression *> Deps() override;

  /// Returns the data type of the constant value.
  planpb::DataType DataType() const { return pb_.data_type(); }

  // Accessor functions that return the value based on type.
  // If the wrong function is called then an invalid value may be returned.
  bool BoolValue() const;
  int64_t Int64Value() const;
  double Float64Value() const;
  std::string StringValue() const;
  bool IsNull() const;

  std::string DebugString() const;

 private:
  planpb::ScalarValue pb_;
};

}  // namespace plan
}  // namespace carnot
}  // namespace pl
