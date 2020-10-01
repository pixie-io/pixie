#include "src/carnot/funcs/builtins/math_ops.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"

namespace pl {
namespace carnot {
namespace builtins {

udf::ScalarUDFDocBuilder BinDoc() {
  return udf::ScalarUDFDocBuilder("Rounds value to the nearest multiple.")
      .Details("Takes the passed in value(s) and bin them to the nearest multiple of a bin.")
      .Example("# bin column b to multiples of 50\\ndf.a = px.bin(df.b, 50)")
      .Arg("value", "The value to bin.")
      .Arg("bin", "The bin value to clip to.")
      .Returns("The value rounded down to the nearest multiple of bin.");
}

void RegisterMathOpsOrDie(udf::Registry* registry) {
  CHECK(registry != nullptr);
  /*****************************************
   * Scalar UDFs.
   *****************************************/
  // Addition
  registry->RegisterOrDie<AddUDF<types::Int64Value, types::Int64Value, types::Int64Value>>("add");
  registry->RegisterOrDie<AddUDF<types::Float64Value, types::Float64Value, types::Int64Value>>(
      "add");
  registry->RegisterOrDie<AddUDF<types::Float64Value, types::Int64Value, types::Float64Value>>(
      "add");
  registry->RegisterOrDie<AddUDF<types::Float64Value, types::Float64Value, types::Float64Value>>(
      "add");
  registry->RegisterOrDie<AddUDF<types::Time64NSValue, types::Time64NSValue, types::Int64Value>>(
      "add");
  registry->RegisterOrDie<AddUDF<types::Time64NSValue, types::Int64Value, types::Time64NSValue>>(
      "add");
  registry->RegisterOrDie<AddUDF<types::StringValue, types::StringValue, types::StringValue>>(
      "add");
  // Subtraction
  registry->RegisterOrDie<SubtractUDF<types::Int64Value, types::Int64Value, types::Int64Value>>(
      "subtract");
  registry->RegisterOrDie<SubtractUDF<types::Float64Value, types::Float64Value, types::Int64Value>>(
      "subtract");
  registry->RegisterOrDie<SubtractUDF<types::Float64Value, types::Int64Value, types::Float64Value>>(
      "subtract");
  registry->RegisterOrDie<SubtractUDF<types::Int64Value, types::Time64NSValue, types::Int64Value>>(
      "subtract");
  registry
      ->RegisterOrDie<SubtractUDF<types::Int64Value, types::Time64NSValue, types::Time64NSValue>>(
          "subtract");
  registry->RegisterOrDie<SubtractUDF<types::Int64Value, types::Int64Value, types::Time64NSValue>>(
      "subtract");
  registry
      ->RegisterOrDie<SubtractUDF<types::Float64Value, types::Float64Value, types::Float64Value>>(
          "subtract");

  // Division
  registry->RegisterOrDie<DivideUDF<types::Int64Value, types::Int64Value, types::Int64Value>>(
      "divide");
  registry->RegisterOrDie<DivideUDF<types::Float64Value, types::Float64Value, types::Int64Value>>(
      "divide");
  registry->RegisterOrDie<DivideUDF<types::Float64Value, types::Int64Value, types::Float64Value>>(
      "divide");
  registry->RegisterOrDie<DivideUDF<types::Float64Value, types::Float64Value, types::Float64Value>>(
      "divide");

  // Multiplication
  registry->RegisterOrDie<MultiplyUDF<types::Float64Value, types::Int64Value, types::Int64Value>>(
      "multiply");
  registry->RegisterOrDie<MultiplyUDF<types::Float64Value, types::Float64Value, types::Int64Value>>(
      "multiply");
  registry->RegisterOrDie<MultiplyUDF<types::Float64Value, types::Int64Value, types::Float64Value>>(
      "multiply");
  registry
      ->RegisterOrDie<MultiplyUDF<types::Float64Value, types::Float64Value, types::Float64Value>>(
          "multiply");

  // Modulo
  registry->RegisterOrDie<ModuloUDF<types::Int64Value, types::Time64NSValue, types::Int64Value>>(
      "modulo");
  registry->RegisterOrDie<ModuloUDF<types::Int64Value, types::Time64NSValue, types::Time64NSValue>>(
      "modulo");
  registry->RegisterOrDie<ModuloUDF<types::Int64Value, types::Int64Value, types::Time64NSValue>>(
      "modulo");
  registry->RegisterOrDie<ModuloUDF<types::Int64Value, types::Int64Value, types::Int64Value>>(
      "modulo");

  // Ceiling/Floor.
  registry->RegisterOrDie<CeilUDF>("ceil");
  registry->RegisterOrDie<FloorUDF>("floor");

  // Or (||)
  registry->RegisterOrDie<LogicalOrUDF<types::Int64Value, types::Int64Value>>("logicalOr");
  registry->RegisterOrDie<LogicalOrUDF<types::BoolValue, types::BoolValue>>("logicalOr");
  // And (&&)
  registry->RegisterOrDie<LogicalAndUDF<types::Int64Value, types::Int64Value>>("logicalAnd");
  registry->RegisterOrDie<LogicalAndUDF<types::BoolValue, types::BoolValue>>("logicalAnd");
  // Not (!)
  registry->RegisterOrDie<LogicalNotUDF<types::Int64Value>>("logicalNot");
  registry->RegisterOrDie<LogicalNotUDF<types::BoolValue>>("logicalNot");
  // Negate (-)
  registry->RegisterOrDie<NegateUDF<types::Int64Value>>("negate");
  registry->RegisterOrDie<NegateUDF<types::Float64Value>>("negate");
  // Invert (~)
  registry->RegisterOrDie<InvertUDF<types::Int64Value>>("invert");
  // ==
  registry->RegisterOrDie<EqualUDF<types::Int64Value, types::Int64Value>>("equal");
  registry->RegisterOrDie<EqualUDF<types::StringValue, types::StringValue>>("equal");
  registry->RegisterOrDie<EqualUDF<types::BoolValue, types::BoolValue>>("equal");
  registry->RegisterOrDie<EqualUDF<types::BoolValue, types::Int64Value>>("equal");
  registry->RegisterOrDie<EqualUDF<types::Int64Value, types::BoolValue>>("equal");
  registry->RegisterOrDie<EqualUDF<types::Int64Value, types::Float64Value>>("equal");
  registry->RegisterOrDie<EqualUDF<types::Float64Value, types::Int64Value>>("equal");
  registry->RegisterOrDie<EqualUDF<types::UInt128Value, types::UInt128Value>>("equal");
  registry->RegisterOrDie<EqualUDF<types::Time64NSValue, types::Time64NSValue>>("equal");
  registry->RegisterOrDie<ApproxEqualUDF<types::Float64Value, types::Float64Value>>("equal");

  // !=
  registry->RegisterOrDie<NotEqualUDF<types::Int64Value, types::Int64Value>>("notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::StringValue, types::StringValue>>("notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::BoolValue, types::BoolValue>>("notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::BoolValue, types::Int64Value>>("notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::Int64Value, types::BoolValue>>("notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::Int64Value, types::Float64Value>>("notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::Float64Value, types::Int64Value>>("notEqual");
  registry->RegisterOrDie<ApproxNotEqualUDF<types::Float64Value, types::Float64Value>>("notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::Time64NSValue, types::Time64NSValue>>("notEqual");
  // ~=
  registry->RegisterOrDie<ApproxEqualUDF<types::Float64Value, types::Float64Value>>("approxEqual");
  // >
  registry->RegisterOrDie<GreaterThanUDF<types::Int64Value, types::Int64Value>>("greaterThan");
  registry->RegisterOrDie<GreaterThanUDF<types::Float64Value, types::Float64Value>>("greaterThan");
  registry->RegisterOrDie<GreaterThanUDF<types::StringValue, types::StringValue>>("greaterThan");
  // >=
  registry->RegisterOrDie<GreaterThanEqualUDF<types::Int64Value, types::Int64Value>>(
      "greaterThanEqual");
  registry->RegisterOrDie<GreaterThanEqualUDF<types::Float64Value, types::Float64Value>>(
      "greaterThanEqual");
  registry->RegisterOrDie<GreaterThanUDF<types::StringValue, types::StringValue>>(
      "greaterThanEqual");

  registry->RegisterOrDie<GreaterThanUDF<types::Time64NSValue, types::Time64NSValue>>(
      "greaterThanEqual");
  // <
  registry->RegisterOrDie<LessThanUDF<types::Int64Value, types::Int64Value>>("lessThan");
  registry->RegisterOrDie<LessThanUDF<types::Float64Value, types::Float64Value>>("lessThan");
  registry->RegisterOrDie<LessThanUDF<types::StringValue, types::StringValue>>("lessThan");
  // <=
  registry->RegisterOrDie<LessThanEqualUDF<types::Int64Value, types::Int64Value>>("lessThanEqual");
  registry->RegisterOrDie<LessThanEqualUDF<types::Float64Value, types::Float64Value>>(
      "lessThanEqual");
  registry->RegisterOrDie<LessThanEqualUDF<types::Time64NSValue, types::Time64NSValue>>(
      "lessThanEqual");
  registry->RegisterOrDie<LessThanUDF<types::StringValue, types::StringValue>>("lessThanEqual");

  // Bin
  registry->RegisterOrDie<BinUDF<types::Int64Value, types::Int64Value, types::Int64Value>>("bin");
  registry->RegisterOrDie<BinUDF<types::Int64Value, types::Int64Value, types::Time64NSValue>>(
      "bin");
  registry->RegisterOrDie<BinUDF<types::Time64NSValue, types::Time64NSValue, types::Int64Value>>(
      "bin");
  registry->RegisterOrDie<BinUDF<types::Time64NSValue, types::Time64NSValue, types::Time64NSValue>>(
      "bin");
  registry->RegisterOrDie<BinUDF<types::Int64Value, types::Float64Value, types::Int64Value>>("bin");

  // Round
  registry->RegisterOrDie<RoundUDF>("round");

  /*****************************************
   * Aggregate UDFs.
   *****************************************/
  // Mean
  registry->RegisterOrDie<MeanUDA<types::Float64Value>>("mean");
  registry->RegisterOrDie<MeanUDA<types::Int64Value>>("mean");
  registry->RegisterOrDie<MeanUDA<types::BoolValue>>("mean");
  // Sum
  registry->RegisterOrDie<SumUDA<types::Float64Value>>("sum");
  registry->RegisterOrDie<SumUDA<types::Int64Value>>("sum");
  registry->RegisterOrDie<SumUDA<types::BoolValue>>("sum");
  // Max
  registry->RegisterOrDie<MaxUDA<types::Float64Value>>("max");
  registry->RegisterOrDie<MaxUDA<types::Int64Value>>("max");
  registry->RegisterOrDie<MaxUDA<types::Time64NSValue>>("max");
  // Min
  registry->RegisterOrDie<MinUDA<types::Float64Value>>("min");
  registry->RegisterOrDie<MinUDA<types::Int64Value>>("min");
  registry->RegisterOrDie<MinUDA<types::Time64NSValue>>("min");
  // Count
  registry->RegisterOrDie<CountUDA<types::Float64Value>>("count");
  registry->RegisterOrDie<CountUDA<types::Int64Value>>("count");
  registry->RegisterOrDie<CountUDA<types::Time64NSValue>>("count");
  registry->RegisterOrDie<CountUDA<types::BoolValue>>("count");
  registry->RegisterOrDie<CountUDA<types::StringValue>>("count");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
