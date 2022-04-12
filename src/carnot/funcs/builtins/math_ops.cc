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

#include "src/carnot/funcs/builtins/math_ops.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace builtins {
udf::ScalarUDFDocBuilder AddDoc() {
  return udf::ScalarUDFDocBuilder("Arithmetically add the arguments or concatenate the strings.")
      .Details(R"(This function is implicitly invoked by the + operator.
      If both types are strings, then will concate the strings. Trying to
      add any other type to a string will cause an error)")
      .Example(R"doc(# Implicit call.
        | df.sum = df.a + df.b
        | Explicit call.
        | df.sum = px.add(df.a, df.b))doc")
      .Arg("a", "The value to be added to.")
      .Arg("b", "The value to add to the first argument.")
      .Returns("The sum of a and b.");
}

udf::ScalarUDFDocBuilder BinDoc() {
  return udf::ScalarUDFDocBuilder("Rounds value to the nearest multiple.")
      .Details("Takes the passed in value(s) and bin them to the nearest multiple of a bin.")
      .Example(R"doc(
      | # bin column b to multiples of 50
      | df.a = px.bin(df.b, 50)
      )doc")
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
  registry->RegisterOrDie<AddUDF<types::Int64Value>>("add");
  registry->RegisterOrDie<AddUDF<types::Float64Value>>("add");
  registry->RegisterOrDie<AddUDF<types::StringValue>>("add");
  registry->RegisterOrDie<AddUDF<types::Float64Value, types::Float64Value, types::Int64Value>>(
      "add");
  registry->RegisterOrDie<AddUDF<types::Float64Value, types::Int64Value, types::Float64Value>>(
      "add");
  registry->RegisterOrDie<AddUDF<types::Time64NSValue, types::Time64NSValue, types::Int64Value>>(
      "add");
  registry->RegisterOrDie<AddUDF<types::Time64NSValue, types::Int64Value, types::Time64NSValue>>(
      "add");
  // Subtraction
  registry->RegisterOrDie<SubtractUDF<types::Int64Value>>("subtract");
  registry->RegisterOrDie<SubtractUDF<types::Float64Value>>("subtract");
  registry->RegisterOrDie<SubtractUDF<types::Float64Value, types::Float64Value, types::Int64Value>>(
      "subtract");
  registry->RegisterOrDie<SubtractUDF<types::Float64Value, types::Int64Value, types::Float64Value>>(
      "subtract");
  registry
      ->RegisterOrDie<SubtractUDF<types::Time64NSValue, types::Time64NSValue, types::Int64Value>>(
          "subtract");
  registry
      ->RegisterOrDie<SubtractUDF<types::Int64Value, types::Time64NSValue, types::Time64NSValue>>(
          "subtract");
  registry->RegisterOrDie<SubtractUDF<types::Int64Value, types::Int64Value, types::Time64NSValue>>(
      "subtract");

  // Division
  registry->RegisterOrDie<DivideUDF<types::Int64Value, types::Int64Value>>("divide");
  registry->RegisterOrDie<DivideUDF<types::Float64Value, types::Int64Value>>("divide");
  registry->RegisterOrDie<DivideUDF<types::Int64Value, types::Float64Value>>("divide");
  registry->RegisterOrDie<DivideUDF<types::Float64Value, types::Float64Value>>("divide");

  // Multiplication
  registry->RegisterOrDie<MultiplyUDF<types::Int64Value>>("multiply");
  registry->RegisterOrDie<MultiplyUDF<types::Float64Value>>("multiply");
  registry->RegisterOrDie<MultiplyUDF<types::Float64Value, types::Float64Value, types::Int64Value>>(
      "multiply");
  registry->RegisterOrDie<MultiplyUDF<types::Float64Value, types::Int64Value, types::Float64Value>>(
      "multiply");

  // Log.
  registry->RegisterOrDie<LogUDF<types::Float64Value, types::Float64Value>>("log");
  registry->RegisterOrDie<LogUDF<types::Float64Value, types::Int64Value>>("log");
  registry->RegisterOrDie<LogUDF<types::Int64Value, types::Float64Value>>("log");
  registry->RegisterOrDie<LogUDF<types::Int64Value, types::Int64Value>>("log");

  // Ln.
  registry->RegisterOrDie<LnUDF<types::Float64Value>>("ln");
  registry->RegisterOrDie<LnUDF<types::Int64Value>>("ln");

  // Log2.
  registry->RegisterOrDie<Log2UDF<types::Float64Value>>("log2");
  registry->RegisterOrDie<Log2UDF<types::Int64Value>>("log2");

  // Log10.
  registry->RegisterOrDie<Log10UDF<types::Float64Value>>("log10");
  registry->RegisterOrDie<Log10UDF<types::Int64Value>>("log10");

  // Pow.
  registry->RegisterOrDie<PowUDF<types::Float64Value, types::Float64Value>>("pow");
  registry->RegisterOrDie<PowUDF<types::Float64Value, types::Int64Value>>("pow");
  registry->RegisterOrDie<PowUDF<types::Int64Value, types::Float64Value>>("pow");
  registry->RegisterOrDie<PowUDF<types::Int64Value, types::Int64Value>>("pow");

  // Exp.
  registry->RegisterOrDie<ExpUDF<types::Float64Value>>("exp");
  registry->RegisterOrDie<ExpUDF<types::Int64Value>>("exp");

  // Abs.
  registry->RegisterOrDie<AbsUDF<types::Float64Value>>("abs");
  registry->RegisterOrDie<AbsUDF<types::Int64Value>>("abs");

  // sqrt.
  registry->RegisterOrDie<SqrtUDF<types::Float64Value>>("sqrt");
  registry->RegisterOrDie<SqrtUDF<types::Int64Value>>("sqrt");

  // Modulo.
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
  registry->RegisterOrDie<LogicalOrUDF<types::Int64Value>>("logicalOr");
  registry->RegisterOrDie<LogicalOrUDF<types::BoolValue>>("logicalOr");
  // And (&&)
  registry->RegisterOrDie<LogicalAndUDF<types::Int64Value>>("logicalAnd");
  registry->RegisterOrDie<LogicalAndUDF<types::BoolValue>>("logicalAnd");
  // Not (!)
  registry->RegisterOrDie<LogicalNotUDF<types::Int64Value>>("logicalNot");
  registry->RegisterOrDie<LogicalNotUDF<types::BoolValue>>("logicalNot");
  // Negate (-)
  registry->RegisterOrDie<NegateUDF<types::Int64Value>>("negate");
  registry->RegisterOrDie<NegateUDF<types::Float64Value>>("negate");
  // Invert (~)
  registry->RegisterOrDie<InvertUDF<types::Int64Value>>("invert");
  // ==
  registry->RegisterOrDie<EqualUDF<types::Int64Value>>("equal");
  registry->RegisterOrDie<EqualUDF<types::StringValue>>("equal");
  registry->RegisterOrDie<EqualUDF<types::BoolValue>>("equal");
  registry->RegisterOrDie<EqualUDF<types::Time64NSValue>>("equal");
  registry->RegisterOrDie<EqualUDF<types::UInt128Value>>("equal");
  registry->RegisterOrDie<EqualUDF<types::BoolValue, types::Int64Value>>("equal");
  registry->RegisterOrDie<EqualUDF<types::Int64Value, types::BoolValue>>("equal");
  registry->RegisterOrDie<EqualUDF<types::Int64Value, types::Float64Value>>("equal");
  registry->RegisterOrDie<EqualUDF<types::Float64Value, types::Int64Value>>("equal");
  registry->RegisterOrDie<ApproxEqualUDF>("equal");

  // !=
  registry->RegisterOrDie<NotEqualUDF<types::Int64Value>>("notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::StringValue>>("notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::BoolValue>>("notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::Time64NSValue>>("notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::UInt128Value>>("notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::BoolValue, types::Int64Value>>("notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::Int64Value, types::BoolValue>>("notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::Int64Value, types::Float64Value>>("notEqual");
  registry->RegisterOrDie<NotEqualUDF<types::Float64Value, types::Int64Value>>("notEqual");
  registry->RegisterOrDie<ApproxNotEqualUDF>("notEqual");
  // ~=
  registry->RegisterOrDie<ApproxEqualUDF>("approxEqual");
  // >
  registry->RegisterOrDie<GreaterThanUDF<types::Int64Value>>("greaterThan");
  registry->RegisterOrDie<GreaterThanUDF<types::Time64NSValue>>("greaterThan");
  registry->RegisterOrDie<GreaterThanUDF<types::Float64Value>>("greaterThan");
  registry->RegisterOrDie<GreaterThanUDF<types::StringValue>>("greaterThan");
  // >=
  registry->RegisterOrDie<GreaterThanEqualUDF<types::Int64Value>>("greaterThanEqual");
  registry->RegisterOrDie<GreaterThanEqualUDF<types::Float64Value>>("greaterThanEqual");
  registry->RegisterOrDie<GreaterThanEqualUDF<types::StringValue>>("greaterThanEqual");
  registry->RegisterOrDie<GreaterThanEqualUDF<types::Time64NSValue>>("greaterThanEqual");
  // <
  registry->RegisterOrDie<LessThanUDF<types::Int64Value>>("lessThan");
  registry->RegisterOrDie<LessThanUDF<types::Time64NSValue>>("lessThan");
  registry->RegisterOrDie<LessThanUDF<types::Float64Value>>("lessThan");
  registry->RegisterOrDie<LessThanUDF<types::StringValue>>("lessThan");
  // <=
  registry->RegisterOrDie<LessThanEqualUDF<types::Int64Value>>("lessThanEqual");
  registry->RegisterOrDie<LessThanEqualUDF<types::Float64Value>>("lessThanEqual");
  registry->RegisterOrDie<LessThanEqualUDF<types::Time64NSValue>>("lessThanEqual");
  registry->RegisterOrDie<LessThanEqualUDF<types::StringValue>>("lessThanEqual");

  // Bin
  registry->RegisterOrDie<BinUDF<types::Int64Value>>("bin");
  registry->RegisterOrDie<BinUDF<types::Time64NSValue>>("bin");
  registry->RegisterOrDie<BinUDF<types::Int64Value, types::Int64Value, types::Time64NSValue>>(
      "bin");
  registry->RegisterOrDie<BinUDF<types::Time64NSValue, types::Time64NSValue, types::Int64Value>>(
      "bin");
  registry->RegisterOrDie<BinUDF<types::Int64Value, types::Float64Value, types::Int64Value>>("bin");

  // Round
  registry->RegisterOrDie<RoundUDF>("round");

  // Time64 <-> Int64
  registry->RegisterOrDie<TimeToInt64UDF>("time_to_int64");
  registry->RegisterOrDie<Int64ToTimeUDF>("int64_to_time");

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
  registry->RegisterOrDie<SumUDA<types::BoolValue, types::Int64Value>>("sum");
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
  registry->RegisterOrDie<CountUDA<types::UInt128Value>>("count");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
