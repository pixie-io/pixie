#include "src/carnot/funcs/builtins/math_ops.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"

namespace pl {
namespace carnot {
namespace builtins {

void RegisterMathOpsOrDie(udf::Registry* registry) {
  CHECK(registry != nullptr);
  /*****************************************
   * Scalar UDFs.
   *****************************************/
  // Addition
  registry->RegisterOrDie<AddUDF<types::Int64Value, types::Int64Value, types::Int64Value>>(
      "px.add");
  registry->RegisterOrDie<AddUDF<types::Float64Value, types::Float64Value, types::Int64Value>>(
      "px.add");
  registry->RegisterOrDie<AddUDF<types::Float64Value, types::Int64Value, types::Float64Value>>(
      "px.add");
  registry->RegisterOrDie<AddUDF<types::Float64Value, types::Float64Value, types::Float64Value>>(
      "px.add");
  registry->RegisterOrDie<AddUDF<types::Time64NSValue, types::Time64NSValue, types::Int64Value>>(
      "px.add");
  registry
      ->RegisterOrDie<AddUDF<types::Time64NSValue, types::Duration64NSValue, types::Time64NSValue>>(
          "px.add");
  registry->RegisterOrDie<
      AddUDF<types::Duration64NSValue, types::Duration64NSValue, types::Duration64NSValue>>(
      "px.add");
  // Subtraction
  registry->RegisterOrDie<SubtractUDF<types::Int64Value, types::Int64Value, types::Int64Value>>(
      "px.subtract");
  registry->RegisterOrDie<SubtractUDF<types::Float64Value, types::Float64Value, types::Int64Value>>(
      "px.subtract");
  registry->RegisterOrDie<SubtractUDF<types::Float64Value, types::Int64Value, types::Float64Value>>(
      "px.subtract");
  registry->RegisterOrDie<SubtractUDF<types::Int64Value, types::Time64NSValue, types::Int64Value>>(
      "px.subtract");
  registry
      ->RegisterOrDie<SubtractUDF<types::Int64Value, types::Time64NSValue, types::Time64NSValue>>(
          "px.subtract");
  registry->RegisterOrDie<SubtractUDF<types::Int64Value, types::Int64Value, types::Time64NSValue>>(
      "px.subtract");
  registry
      ->RegisterOrDie<SubtractUDF<types::Float64Value, types::Float64Value, types::Float64Value>>(
          "px.subtract");
  registry->RegisterOrDie<
      SubtractUDF<types::Time64NSValue, types::Time64NSValue, types::Duration64NSValue>>(
      "px.subtract");
  registry->RegisterOrDie<
      SubtractUDF<types::Duration64NSValue, types::Duration64NSValue, types::Duration64NSValue>>(
      "px.subtract");

  // Division
  registry->RegisterOrDie<DivideUDF<types::Int64Value, types::Int64Value, types::Int64Value>>(
      "px.divide");
  registry->RegisterOrDie<DivideUDF<types::Float64Value, types::Float64Value, types::Int64Value>>(
      "px.divide");
  registry->RegisterOrDie<DivideUDF<types::Float64Value, types::Int64Value, types::Float64Value>>(
      "px.divide");
  registry->RegisterOrDie<DivideUDF<types::Float64Value, types::Float64Value, types::Float64Value>>(
      "px.divide");

  // Multiplication
  registry->RegisterOrDie<MultiplyUDF<types::Int64Value, types::Int64Value, types::Int64Value>>(
      "px.multiply");
  registry->RegisterOrDie<MultiplyUDF<types::Float64Value, types::Float64Value, types::Int64Value>>(
      "px.multiply");
  registry->RegisterOrDie<MultiplyUDF<types::Float64Value, types::Int64Value, types::Float64Value>>(
      "px.multiply");
  registry
      ->RegisterOrDie<MultiplyUDF<types::Float64Value, types::Float64Value, types::Float64Value>>(
          "px.multiply");

  // Modulo
  registry->RegisterOrDie<ModuloUDF<types::Int64Value, types::Time64NSValue, types::Int64Value>>(
      "px.modulo");
  registry->RegisterOrDie<ModuloUDF<types::Int64Value, types::Time64NSValue, types::Time64NSValue>>(
      "px.modulo");
  registry->RegisterOrDie<ModuloUDF<types::Int64Value, types::Int64Value, types::Time64NSValue>>(
      "px.modulo");
  registry->RegisterOrDie<ModuloUDF<types::Int64Value, types::Int64Value, types::Int64Value>>(
      "px.modulo");

  // Or (||)
  registry->RegisterOrDie<LogicalOrUDF<types::Int64Value, types::Int64Value>>("px.logicalOr");
  registry->RegisterOrDie<LogicalOrUDF<types::BoolValue, types::BoolValue>>("px.logicalOr");
  // And (&&)
  registry->RegisterOrDie<LogicalAndUDF<types::Int64Value, types::Int64Value>>("px.logicalAnd");
  registry->RegisterOrDie<LogicalAndUDF<types::BoolValue, types::BoolValue>>("px.logicalAnd");
  // Not (!)
  registry->RegisterOrDie<LogicalNotUDF<types::Int64Value>>("px.logicalNot");
  registry->RegisterOrDie<LogicalNotUDF<types::BoolValue>>("px.logicalNot");
  // Negate (-)
  registry->RegisterOrDie<NegateUDF<types::Int64Value>>("px.negate");
  registry->RegisterOrDie<NegateUDF<types::Float64Value>>("px.negate");
  // ==
  registry->RegisterOrDie<EqualUDF<types::Int64Value, types::Int64Value>>("px.equal");
  registry->RegisterOrDie<EqualUDF<types::StringValue, types::StringValue>>("px.equal");
  registry->RegisterOrDie<EqualUDF<types::BoolValue, types::BoolValue>>("px.equal");
  registry->RegisterOrDie<EqualUDF<types::BoolValue, types::Int64Value>>("px.equal");
  registry->RegisterOrDie<EqualUDF<types::Int64Value, types::BoolValue>>("px.equal");
  registry->RegisterOrDie<EqualUDF<types::Int64Value, types::Float64Value>>("px.equal");
  registry->RegisterOrDie<EqualUDF<types::Float64Value, types::Int64Value>>("px.equal");
  registry->RegisterOrDie<EqualUDF<types::UInt128Value, types::UInt128Value>>("px.equal");
  registry->RegisterOrDie<EqualUDF<types::Duration64NSValue, types::Duration64NSValue>>("px.equal");
  registry->RegisterOrDie<EqualUDF<types::Time64NSValue, types::Time64NSValue>>("px.equal");
  registry->RegisterOrDie<ApproxEqualUDF<types::Float64Value, types::Float64Value>>("px.equal");

  // !=
  registry->RegisterOrDie<NotEqualUDF<types::Int64Value, types::Int64Value>>("px.notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::StringValue, types::StringValue>>("px.notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::BoolValue, types::BoolValue>>("px.notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::BoolValue, types::Int64Value>>("px.notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::Int64Value, types::BoolValue>>("px.notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::Int64Value, types::Float64Value>>("px.notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::Float64Value, types::Int64Value>>("px.notEqual");
  registry->RegisterOrDie<ApproxNotEqualUDF<types::Float64Value, types::Float64Value>>(
      "px.notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::Time64NSValue, types::Time64NSValue>>("px.notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::Duration64NSValue, types::Duration64NSValue>>(
      "px.notEqual");
  // ~=
  registry->RegisterOrDie<ApproxEqualUDF<types::Float64Value, types::Float64Value>>(
      "px.approxEqual");
  // >
  registry->RegisterOrDie<GreaterThanUDF<types::Int64Value, types::Int64Value>>("px.greaterThan");
  registry->RegisterOrDie<GreaterThanUDF<types::Float64Value, types::Float64Value>>(
      "px.greaterThan");
  registry->RegisterOrDie<GreaterThanUDF<types::StringValue, types::StringValue>>("px.greaterThan");
  // >=
  registry->RegisterOrDie<GreaterThanEqualUDF<types::Int64Value, types::Int64Value>>(
      "px.greaterThanEqual");
  registry->RegisterOrDie<GreaterThanEqualUDF<types::Float64Value, types::Float64Value>>(
      "px.greaterThanEqual");
  registry->RegisterOrDie<GreaterThanUDF<types::StringValue, types::StringValue>>(
      "px.greaterThanEqual");

  registry->RegisterOrDie<GreaterThanUDF<types::Time64NSValue, types::Time64NSValue>>(
      "px.greaterThanEqual");
  registry->RegisterOrDie<GreaterThanUDF<types::Duration64NSValue, types::Duration64NSValue>>(
      "px.greaterThanEqual");
  // <
  registry->RegisterOrDie<LessThanUDF<types::Int64Value, types::Int64Value>>("px.lessThan");
  registry->RegisterOrDie<LessThanUDF<types::Float64Value, types::Float64Value>>("px.lessThan");
  registry->RegisterOrDie<LessThanUDF<types::StringValue, types::StringValue>>("px.lessThan");
  // <=
  registry->RegisterOrDie<LessThanEqualUDF<types::Int64Value, types::Int64Value>>(
      "px.lessThanEqual");
  registry->RegisterOrDie<LessThanEqualUDF<types::Float64Value, types::Float64Value>>(
      "px.lessThanEqual");
  registry->RegisterOrDie<LessThanEqualUDF<types::Time64NSValue, types::Time64NSValue>>(
      "px.lessThanEqual");
  registry->RegisterOrDie<LessThanEqualUDF<types::Duration64NSValue, types::Duration64NSValue>>(
      "px.lessThanEqual");
  registry->RegisterOrDie<LessThanUDF<types::StringValue, types::StringValue>>("px.lessThanEqual");

  // Bin
  registry->RegisterOrDie<BinUDF<types::Int64Value, types::Int64Value, types::Int64Value>>(
      "px.bin");
  registry->RegisterOrDie<BinUDF<types::Int64Value, types::Int64Value, types::Time64NSValue>>(
      "px.bin");
  registry->RegisterOrDie<BinUDF<types::Time64NSValue, types::Time64NSValue, types::Int64Value>>(
      "px.bin");
  registry->RegisterOrDie<BinUDF<types::Time64NSValue, types::Time64NSValue, types::Time64NSValue>>(
      "px.bin");

  // Round
  registry->RegisterOrDie<RoundUDF>("px.round");

  /*****************************************
   * Aggregate UDFs.
   *****************************************/
  // Mean
  registry->RegisterOrDie<MeanUDA<types::Float64Value>>("px.mean");
  registry->RegisterOrDie<MeanUDA<types::Int64Value>>("px.mean");
  registry->RegisterOrDie<MeanUDA<types::Duration64NSValue>>("px.mean");
  registry->RegisterOrDie<MeanUDA<types::BoolValue>>("px.mean");
  // Sum
  registry->RegisterOrDie<SumUDA<types::Float64Value>>("px.sum");
  registry->RegisterOrDie<SumUDA<types::Int64Value>>("px.sum");
  registry->RegisterOrDie<SumUDA<types::Duration64NSValue>>("px.sum");
  registry->RegisterOrDie<SumUDA<types::BoolValue>>("px.sum");
  // Max
  registry->RegisterOrDie<MaxUDA<types::Float64Value>>("px.max");
  registry->RegisterOrDie<MaxUDA<types::Int64Value>>("px.max");
  registry->RegisterOrDie<MaxUDA<types::Time64NSValue>>("px.max");
  registry->RegisterOrDie<MaxUDA<types::Duration64NSValue>>("px.max");
  // Min
  registry->RegisterOrDie<MinUDA<types::Float64Value>>("px.min");
  registry->RegisterOrDie<MinUDA<types::Int64Value>>("px.min");
  registry->RegisterOrDie<MinUDA<types::Time64NSValue>>("px.min");
  registry->RegisterOrDie<MinUDA<types::Duration64NSValue>>("px.min");
  // Count
  registry->RegisterOrDie<CountUDA<types::Float64Value>>("px.count");
  registry->RegisterOrDie<CountUDA<types::Int64Value>>("px.count");
  registry->RegisterOrDie<CountUDA<types::Time64NSValue>>("px.count");
  registry->RegisterOrDie<CountUDA<types::Duration64NSValue>>("px.count");
  registry->RegisterOrDie<CountUDA<types::BoolValue>>("px.count");
  registry->RegisterOrDie<CountUDA<types::StringValue>>("px.count");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
