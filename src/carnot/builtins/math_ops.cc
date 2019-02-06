#include <glog/logging.h>

#include "src/carnot/builtins/math_ops.h"
#include "src/carnot/udf/registry.h"

namespace pl {
namespace carnot {
namespace builtins {

void RegisterMathOpsOrDie(udf::ScalarUDFRegistry* registry) {
  CHECK(registry != nullptr);
  // Addition
  registry->RegisterOrDie<AddUDF<udf::Int64Value, udf::Int64Value, udf::Int64Value>>("add");
  registry->RegisterOrDie<AddUDF<udf::Float64Value, udf::Float64Value, udf::Int64Value>>("add");
  registry->RegisterOrDie<AddUDF<udf::Float64Value, udf::Int64Value, udf::Float64Value>>("add");
  registry->RegisterOrDie<AddUDF<udf::Float64Value, udf::Float64Value, udf::Float64Value>>("add");
  // Subtraction
  registry->RegisterOrDie<SubtractUDF<udf::Int64Value, udf::Int64Value, udf::Int64Value>>(
      "subtract");
  registry->RegisterOrDie<SubtractUDF<udf::Float64Value, udf::Float64Value, udf::Int64Value>>(
      "subtract");
  registry->RegisterOrDie<SubtractUDF<udf::Float64Value, udf::Int64Value, udf::Float64Value>>(
      "subtract");
  registry->RegisterOrDie<SubtractUDF<udf::Float64Value, udf::Float64Value, udf::Float64Value>>(
      "subtract");
  // Division
  registry->RegisterOrDie<DivideUDF<udf::Int64Value, udf::Int64Value, udf::Int64Value>>("divide");
  registry->RegisterOrDie<DivideUDF<udf::Float64Value, udf::Float64Value, udf::Int64Value>>(
      "divide");
  registry->RegisterOrDie<DivideUDF<udf::Float64Value, udf::Int64Value, udf::Float64Value>>(
      "divide");
  registry->RegisterOrDie<DivideUDF<udf::Float64Value, udf::Float64Value, udf::Float64Value>>(
      "divide");
  // Multiplication
  registry->RegisterOrDie<MultiplyUDF<udf::Int64Value, udf::Int64Value, udf::Int64Value>>(
      "multiply");
  registry->RegisterOrDie<MultiplyUDF<udf::Float64Value, udf::Float64Value, udf::Int64Value>>(
      "multiply");
  registry->RegisterOrDie<MultiplyUDF<udf::Float64Value, udf::Int64Value, udf::Float64Value>>(
      "multiply");
  registry->RegisterOrDie<MultiplyUDF<udf::Float64Value, udf::Float64Value, udf::Float64Value>>(
      "multiply");
  // Modulo
  registry->RegisterOrDie<ModuloUDF>("modulo");
  // Or (||)
  registry->RegisterOrDie<LogicalOrUDF<udf::Int64Value, udf::Int64Value>>("logicalOr");
  registry->RegisterOrDie<LogicalOrUDF<udf::BoolValue, udf::BoolValue>>("logicalOr");
  // And (&&)
  registry->RegisterOrDie<LogicalAndUDF<udf::Int64Value, udf::Int64Value>>("logicalAnd");
  registry->RegisterOrDie<LogicalAndUDF<udf::BoolValue, udf::BoolValue>>("logicalAnd");
  // Not (!)
  registry->RegisterOrDie<LogicalNotUDF<udf::Int64Value>>("logicalNot");
  registry->RegisterOrDie<LogicalNotUDF<udf::BoolValue>>("logicalNot");
  // Negate (-)
  registry->RegisterOrDie<NegateUDF<udf::Int64Value>>("negate");
  registry->RegisterOrDie<NegateUDF<udf::Float64Value>>("negate");
  // ==
  registry->RegisterOrDie<EqualUDF<udf::Int64Value, udf::Int64Value>>("equal");
  registry->RegisterOrDie<EqualUDF<udf::StringValue, udf::StringValue>>("equal");
  // ~=
  registry->RegisterOrDie<ApproxEqualUDF<udf::Float64Value, udf::Float64Value>>("approxEqual");
  // >
  registry->RegisterOrDie<GreaterThanUDF<udf::Int64Value, udf::Int64Value>>("greaterThan");
  registry->RegisterOrDie<GreaterThanUDF<udf::Float64Value, udf::Float64Value>>("greaterThan");
  registry->RegisterOrDie<GreaterThanUDF<udf::StringValue, udf::StringValue>>("greaterThan");
  // <
  registry->RegisterOrDie<LessThanUDF<udf::Int64Value, udf::Int64Value>>("lessThan");
  registry->RegisterOrDie<LessThanUDF<udf::Float64Value, udf::Float64Value>>("lessThan");
  registry->RegisterOrDie<LessThanUDF<udf::StringValue, udf::StringValue>>("lessThan");
}

void RegisterMathOpsOrDie(udf::UDARegistry* registry) {
  CHECK(registry != nullptr);
  // Mean
  registry->RegisterOrDie<MeanUDA<udf::Float64Value>>("mean");
  registry->RegisterOrDie<MeanUDA<udf::Int64Value>>("mean");
  // Sum
  registry->RegisterOrDie<SumUDA<udf::Float64Value>>("sum");
  registry->RegisterOrDie<SumUDA<udf::Int64Value>>("sum");
  // Max
  registry->RegisterOrDie<MaxUDA<udf::Float64Value>>("max");
  registry->RegisterOrDie<MaxUDA<udf::Int64Value>>("max");
  // Min
  registry->RegisterOrDie<MinUDA<udf::Float64Value>>("min");
  registry->RegisterOrDie<MinUDA<udf::Int64Value>>("min");
  // Count
  registry->RegisterOrDie<CountUDA<udf::Float64Value>>("count");
  registry->RegisterOrDie<CountUDA<udf::Int64Value>>("count");
  registry->RegisterOrDie<CountUDA<udf::BoolValue>>("count");
  registry->RegisterOrDie<CountUDA<udf::StringValue>>("count");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
