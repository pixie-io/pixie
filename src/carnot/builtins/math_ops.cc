#include "src/carnot/builtins/math_ops.h"
#include "src/carnot/udf/registry.h"
#include "src/common/common.h"

namespace pl {
namespace carnot {
namespace builtins {

void RegisterMathOpsOrDie(udf::ScalarUDFRegistry* registry) {
  CHECK(registry != nullptr);
  // Addition
  registry->RegisterOrDie<AddUDF<types::Int64Value, types::Int64Value, types::Int64Value>>(
      "pl.add");
  registry->RegisterOrDie<AddUDF<types::Float64Value, types::Float64Value, types::Int64Value>>(
      "pl.add");
  registry->RegisterOrDie<AddUDF<types::Float64Value, types::Int64Value, types::Float64Value>>(
      "pl.add");
  registry->RegisterOrDie<AddUDF<types::Float64Value, types::Float64Value, types::Float64Value>>(
      "pl.add");
  // Subtraction
  registry->RegisterOrDie<SubtractUDF<types::Int64Value, types::Int64Value, types::Int64Value>>(
      "pl.subtract");
  registry->RegisterOrDie<SubtractUDF<types::Float64Value, types::Float64Value, types::Int64Value>>(
      "pl.subtract");
  registry->RegisterOrDie<SubtractUDF<types::Float64Value, types::Int64Value, types::Float64Value>>(
      "pl.subtract");
  registry
      ->RegisterOrDie<SubtractUDF<types::Float64Value, types::Float64Value, types::Float64Value>>(
          "pl.subtract");
  // Division
  registry->RegisterOrDie<DivideUDF<types::Int64Value, types::Int64Value, types::Int64Value>>(
      "pl.divide");
  registry->RegisterOrDie<DivideUDF<types::Float64Value, types::Float64Value, types::Int64Value>>(
      "pl.divide");
  registry->RegisterOrDie<DivideUDF<types::Float64Value, types::Int64Value, types::Float64Value>>(
      "pl.divide");
  registry->RegisterOrDie<DivideUDF<types::Float64Value, types::Float64Value, types::Float64Value>>(
      "pl.divide");
  // Multiplication
  registry->RegisterOrDie<MultiplyUDF<types::Int64Value, types::Int64Value, types::Int64Value>>(
      "pl.multiply");
  registry->RegisterOrDie<MultiplyUDF<types::Float64Value, types::Float64Value, types::Int64Value>>(
      "pl.multiply");
  registry->RegisterOrDie<MultiplyUDF<types::Float64Value, types::Int64Value, types::Float64Value>>(
      "pl.multiply");
  registry
      ->RegisterOrDie<MultiplyUDF<types::Float64Value, types::Float64Value, types::Float64Value>>(
          "pl.multiply");
  // Modulo
  registry->RegisterOrDie<ModuloUDF>("pl.modulo");
  // Or (||)
  registry->RegisterOrDie<LogicalOrUDF<types::Int64Value, types::Int64Value>>("pl.logicalOr");
  registry->RegisterOrDie<LogicalOrUDF<types::BoolValue, types::BoolValue>>("pl.logicalOr");
  // And (&&)
  registry->RegisterOrDie<LogicalAndUDF<types::Int64Value, types::Int64Value>>("pl.logicalAnd");
  registry->RegisterOrDie<LogicalAndUDF<types::BoolValue, types::BoolValue>>("pl.logicalAnd");
  // Not (!)
  registry->RegisterOrDie<LogicalNotUDF<types::Int64Value>>("pl.logicalNot");
  registry->RegisterOrDie<LogicalNotUDF<types::BoolValue>>("pl.logicalNot");
  // Negate (-)
  registry->RegisterOrDie<NegateUDF<types::Int64Value>>("pl.negate");
  registry->RegisterOrDie<NegateUDF<types::Float64Value>>("pl.negate");
  // ==
  registry->RegisterOrDie<EqualUDF<types::Int64Value, types::Int64Value>>("pl.equal");
  registry->RegisterOrDie<EqualUDF<types::StringValue, types::StringValue>>("pl.equal");
  // ~=
  registry->RegisterOrDie<ApproxEqualUDF<types::Float64Value, types::Float64Value>>(
      "pl.approxEqual");
  // >
  registry->RegisterOrDie<GreaterThanUDF<types::Int64Value, types::Int64Value>>("pl.greaterThan");
  registry->RegisterOrDie<GreaterThanUDF<types::Float64Value, types::Float64Value>>(
      "pl.greaterThan");
  registry->RegisterOrDie<GreaterThanUDF<types::StringValue, types::StringValue>>("pl.greaterThan");
  // <
  registry->RegisterOrDie<LessThanUDF<types::Int64Value, types::Int64Value>>("pl.lessThan");
  registry->RegisterOrDie<LessThanUDF<types::Float64Value, types::Float64Value>>("pl.lessThan");
  registry->RegisterOrDie<LessThanUDF<types::StringValue, types::StringValue>>("pl.lessThan");
}

void RegisterMathOpsOrDie(udf::UDARegistry* registry) {
  CHECK(registry != nullptr);
  // Mean
  registry->RegisterOrDie<MeanUDA<types::Float64Value>>("pl.mean");
  registry->RegisterOrDie<MeanUDA<types::Int64Value>>("pl.mean");
  // Sum
  registry->RegisterOrDie<SumUDA<types::Float64Value>>("pl.sum");
  registry->RegisterOrDie<SumUDA<types::Int64Value>>("pl.sum");
  // Max
  registry->RegisterOrDie<MaxUDA<types::Float64Value>>("pl.max");
  registry->RegisterOrDie<MaxUDA<types::Int64Value>>("pl.max");
  // Min
  registry->RegisterOrDie<MinUDA<types::Float64Value>>("pl.min");
  registry->RegisterOrDie<MinUDA<types::Int64Value>>("pl.min");
  // Count
  registry->RegisterOrDie<CountUDA<types::Float64Value>>("pl.count");
  registry->RegisterOrDie<CountUDA<types::Int64Value>>("pl.count");
  registry->RegisterOrDie<CountUDA<types::BoolValue>>("pl.count");
  registry->RegisterOrDie<CountUDA<types::StringValue>>("pl.count");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
