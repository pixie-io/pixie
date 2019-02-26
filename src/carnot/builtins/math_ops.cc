#include "src/carnot/builtins/math_ops.h"
#include "src/carnot/udf/registry.h"
#include "src/common/common.h"

namespace pl {
namespace carnot {
namespace builtins {

void RegisterMathOpsOrDie(udf::ScalarUDFRegistry* registry) {
  CHECK(registry != nullptr);
  // Addition
  registry->RegisterOrDie<AddUDF<udf::Int64Value, udf::Int64Value, udf::Int64Value>>("pl.add");
  registry->RegisterOrDie<AddUDF<udf::Float64Value, udf::Float64Value, udf::Int64Value>>("pl.add");
  registry->RegisterOrDie<AddUDF<udf::Float64Value, udf::Int64Value, udf::Float64Value>>("pl.add");
  registry->RegisterOrDie<AddUDF<udf::Float64Value, udf::Float64Value, udf::Float64Value>>(
      "pl.add");
  // Subtraction
  registry->RegisterOrDie<SubtractUDF<udf::Int64Value, udf::Int64Value, udf::Int64Value>>(
      "pl.subtract");
  registry->RegisterOrDie<SubtractUDF<udf::Float64Value, udf::Float64Value, udf::Int64Value>>(
      "pl.subtract");
  registry->RegisterOrDie<SubtractUDF<udf::Float64Value, udf::Int64Value, udf::Float64Value>>(
      "pl.subtract");
  registry->RegisterOrDie<SubtractUDF<udf::Float64Value, udf::Float64Value, udf::Float64Value>>(
      "pl.subtract");
  // Division
  registry->RegisterOrDie<DivideUDF<udf::Int64Value, udf::Int64Value, udf::Int64Value>>(
      "pl.divide");
  registry->RegisterOrDie<DivideUDF<udf::Float64Value, udf::Float64Value, udf::Int64Value>>(
      "pl.divide");
  registry->RegisterOrDie<DivideUDF<udf::Float64Value, udf::Int64Value, udf::Float64Value>>(
      "pl.divide");
  registry->RegisterOrDie<DivideUDF<udf::Float64Value, udf::Float64Value, udf::Float64Value>>(
      "pl.divide");
  // Multiplication
  registry->RegisterOrDie<MultiplyUDF<udf::Int64Value, udf::Int64Value, udf::Int64Value>>(
      "pl.multiply");
  registry->RegisterOrDie<MultiplyUDF<udf::Float64Value, udf::Float64Value, udf::Int64Value>>(
      "pl.multiply");
  registry->RegisterOrDie<MultiplyUDF<udf::Float64Value, udf::Int64Value, udf::Float64Value>>(
      "pl.multiply");
  registry->RegisterOrDie<MultiplyUDF<udf::Float64Value, udf::Float64Value, udf::Float64Value>>(
      "pl.multiply");
  // Modulo
  registry->RegisterOrDie<ModuloUDF>("pl.modulo");
  // Or (||)
  registry->RegisterOrDie<LogicalOrUDF<udf::Int64Value, udf::Int64Value>>("pl.logicalOr");
  registry->RegisterOrDie<LogicalOrUDF<udf::BoolValue, udf::BoolValue>>("pl.logicalOr");
  // And (&&)
  registry->RegisterOrDie<LogicalAndUDF<udf::Int64Value, udf::Int64Value>>("pl.logicalAnd");
  registry->RegisterOrDie<LogicalAndUDF<udf::BoolValue, udf::BoolValue>>("pl.logicalAnd");
  // Not (!)
  registry->RegisterOrDie<LogicalNotUDF<udf::Int64Value>>("pl.logicalNot");
  registry->RegisterOrDie<LogicalNotUDF<udf::BoolValue>>("pl.logicalNot");
  // Negate (-)
  registry->RegisterOrDie<NegateUDF<udf::Int64Value>>("pl.negate");
  registry->RegisterOrDie<NegateUDF<udf::Float64Value>>("pl.negate");
  // ==
  registry->RegisterOrDie<EqualUDF<udf::Int64Value, udf::Int64Value>>("pl.equal");
  registry->RegisterOrDie<EqualUDF<udf::StringValue, udf::StringValue>>("pl.equal");
  // ~=
  registry->RegisterOrDie<ApproxEqualUDF<udf::Float64Value, udf::Float64Value>>("pl.approxEqual");
  // >
  registry->RegisterOrDie<GreaterThanUDF<udf::Int64Value, udf::Int64Value>>("pl.greaterThan");
  registry->RegisterOrDie<GreaterThanUDF<udf::Float64Value, udf::Float64Value>>("pl.greaterThan");
  registry->RegisterOrDie<GreaterThanUDF<udf::StringValue, udf::StringValue>>("pl.greaterThan");
  // <
  registry->RegisterOrDie<LessThanUDF<udf::Int64Value, udf::Int64Value>>("pl.lessThan");
  registry->RegisterOrDie<LessThanUDF<udf::Float64Value, udf::Float64Value>>("pl.lessThan");
  registry->RegisterOrDie<LessThanUDF<udf::StringValue, udf::StringValue>>("pl.lessThan");
}

void RegisterMathOpsOrDie(udf::UDARegistry* registry) {
  CHECK(registry != nullptr);
  // Mean
  registry->RegisterOrDie<MeanUDA<udf::Float64Value>>("pl.mean");
  registry->RegisterOrDie<MeanUDA<udf::Int64Value>>("pl.mean");
  // Sum
  registry->RegisterOrDie<SumUDA<udf::Float64Value>>("pl.sum");
  registry->RegisterOrDie<SumUDA<udf::Int64Value>>("pl.sum");
  // Max
  registry->RegisterOrDie<MaxUDA<udf::Float64Value>>("pl.max");
  registry->RegisterOrDie<MaxUDA<udf::Int64Value>>("pl.max");
  // Min
  registry->RegisterOrDie<MinUDA<udf::Float64Value>>("pl.min");
  registry->RegisterOrDie<MinUDA<udf::Int64Value>>("pl.min");
  // Count
  registry->RegisterOrDie<CountUDA<udf::Float64Value>>("pl.count");
  registry->RegisterOrDie<CountUDA<udf::Int64Value>>("pl.count");
  registry->RegisterOrDie<CountUDA<udf::BoolValue>>("pl.count");
  registry->RegisterOrDie<CountUDA<udf::StringValue>>("pl.count");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
