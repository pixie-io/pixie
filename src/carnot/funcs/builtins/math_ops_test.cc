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

#include <gtest/gtest.h>
#include <numeric>
#include <type_traits>
#include <vector>

#include "src/carnot/funcs/builtins/math_ops.h"
#include "src/carnot/udf/test_utils.h"
#include "src/common/base/base.h"
#include "src/common/testing/testing.h"

namespace px {
namespace carnot {
namespace builtins {

TEST(MathOps, basic_int64_add_test) {
  auto udf_tester = udf::UDFTester<AddUDF<types::Int64Value>>();
  udf_tester.ForInput(1, 2).Expect(3);
}

TEST(MathOps, basic_float64_add_test) {
  auto udf_tester = udf::UDFTester<AddUDF<types::Float64Value>>();
  udf_tester.ForInput(1.5, 2.6).Expect(4.1);
}

TEST(MathOps, basic_mixed_add_test) {
  auto udf_tester = udf::UDFTester<AddUDF<types::Float64Value>>();
  udf_tester.ForInput(1, 2.6).Expect(3.6);
}

TEST(MathOps, basic_string_add_test) {
  auto udf_tester = udf::UDFTester<AddUDF<types::StringValue>>();
  udf_tester.ForInput("a", "b").Expect("ab");
}

TEST(MathOps, basic_int64_subtract_test) {
  auto udf_tester = udf::UDFTester<SubtractUDF<types::Int64Value>>();
  udf_tester.ForInput(1, 2).Expect(-1);
}

TEST(MathOps, basic_float64_subtract_test) {
  auto udf_tester = udf::UDFTester<SubtractUDF<types::Float64Value>>();
  udf_tester.ForInput(1.5, 2.6).Expect(-1.1);
}

TEST(MathOps, basic_mixed_subtract_test) {
  auto udf_tester = udf::UDFTester<SubtractUDF<types::Float64Value>>();
  udf_tester.ForInput(1.5, 2).Expect(-0.5);
}

TEST(MathOps, basic_int64_divide_test) {
  auto udf_tester = udf::UDFTester<DivideUDF<types::Int64Value, types::Int64Value>>();
  udf_tester.ForInput(1, 2).Expect(0.5);
}

TEST(MathOps, basic_float64_divide_test) {
  auto udf_tester = udf::UDFTester<DivideUDF<types::Float64Value, types::Float64Value>>();
  udf_tester.ForInput(1.5, 5).Expect(0.3);
}

TEST(MathOps, basic_mixed_divide_test) {
  auto udf_tester = udf::UDFTester<DivideUDF<types::Float64Value, types::Int64Value>>();
  udf_tester.ForInput(1.5, 5).Expect(0.3);
}

TEST(MathOps, basic_int64_multiply_test) {
  auto udf_tester = udf::UDFTester<MultiplyUDF<types::Int64Value>>();
  udf_tester.ForInput(1, 2).Expect(2);
}

TEST(MathOps, basic_float64_multiply_test) {
  auto udf_tester = udf::UDFTester<MultiplyUDF<types::Float64Value>>();
  udf_tester.ForInput(1.5, 5).Expect(7.5);
}

TEST(MathOps, log) {
  auto udf_tester = udf::UDFTester<LogUDF<types::Int64Value, types::Int64Value>>();
  udf_tester.ForInput(2, 4).Expect(2);
}

TEST(MathOps, ln) {
  auto udf_tester = udf::UDFTester<LnUDF<types::Float64Value>>();
  udf_tester.ForInput(std::exp(1.0)).Expect(1);
}

TEST(MathOps, log2) {
  auto udf_tester = udf::UDFTester<Log2UDF<types::Float64Value>>();
  udf_tester.ForInput(4.0).Expect(2);
}

TEST(MathOps, log10) {
  auto udf_tester = udf::UDFTester<Log10UDF<types::Float64Value>>();
  udf_tester.ForInput(100).Expect(2);
}

TEST(MathOps, pow) {
  auto udf_tester = udf::UDFTester<PowUDF<types::Float64Value, types::Float64Value>>();
  udf_tester.ForInput(2.0, 2.0).Expect(4);
}

TEST(MathOps, abs) {
  auto udf_tester = udf::UDFTester<ExpUDF<types::Int64Value>>();
  udf_tester.ForInput(1).Expect(std::exp(1.0));
}

TEST(MathOps, exp) {
  auto udf_tester = udf::UDFTester<AbsUDF<types::Int64Value>>();
  udf_tester.ForInput(-2).Expect(2);
  udf_tester.ForInput(4).Expect(4);
}

TEST(MathOps, sqrt) {
  auto udf_tester = udf::UDFTester<SqrtUDF<types::Float64Value>>();
  udf_tester.ForInput(4).Expect(2);
}

TEST(MathOps, basic_mixed_multiply_test) {
  auto udf_tester =
      udf::UDFTester<MultiplyUDF<types::Float64Value, types::Float64Value, types::Int64Value>>();
  udf_tester.ForInput(1.5, 5).Expect(7.5);
}

TEST(MathOps, basic_modulo_test_int64_int64) {
  auto udf_tester =
      udf::UDFTester<ModuloUDF<types::Int64Value, types::Int64Value, types::Int64Value>>();
  udf_tester.ForInput(10, 7).Expect(3);
}

TEST(MathOps, basic_modulo_test_time64_int64) {
  auto udf_tester =
      udf::UDFTester<ModuloUDF<types::Int64Value, types::Time64NSValue, types::Int64Value>>();
  udf_tester.ForInput(10, 7).Expect(3);
}

TEST(MathOps, basic_bool_or_test) {
  auto udf_tester = udf::UDFTester<LogicalOrUDF<types::BoolValue>>();
  udf_tester.ForInput(true, false).Expect(true);
  udf_tester.ForInput(false, false).Expect(false);
}

TEST(MathOps, basic_int64_or_test) {
  auto udf_tester = udf::UDFTester<LogicalOrUDF<types::Int64Value>>();
  udf_tester.ForInput(4, 0).Expect(true);
  udf_tester.ForInput(0, 0).Expect(false);
}

TEST(MathOps, basic_bool_and_test) {
  auto udf_tester = udf::UDFTester<LogicalAndUDF<types::BoolValue>>();
  udf_tester.ForInput(true, false).Expect(false);
  udf_tester.ForInput(false, false).Expect(false);
  udf_tester.ForInput(true, true).Expect(true);
}

TEST(MathOps, basic_int64_and_test) {
  auto udf_tester = udf::UDFTester<LogicalAndUDF<types::Int64Value>>();
  udf_tester.ForInput(4, 0).Expect(false);
  udf_tester.ForInput(0, 0).Expect(false);
  udf_tester.ForInput(5, 3).Expect(true);
}

TEST(MathOps, basic_bool_not_test) {
  auto udf_tester = udf::UDFTester<LogicalNotUDF<types::BoolValue>>();
  udf_tester.ForInput(true).Expect(false);
  udf_tester.ForInput(false).Expect(true);
}

TEST(MathOps, basic_int64_not_test) {
  auto udf_tester = udf::UDFTester<LogicalNotUDF<types::Int64Value>>();
  udf_tester.ForInput(4).Expect(false);
  udf_tester.ForInput(0).Expect(true);
}

TEST(MathOps, basic_int64_negate_test) {
  auto udf_tester = udf::UDFTester<NegateUDF<types::Int64Value>>();
  udf_tester.ForInput(-4).Expect(4);
  udf_tester.ForInput(0).Expect(0);
  udf_tester.ForInput(4).Expect(-4);
}

TEST(MathOps, basic_ceil_test) {
  auto udf_tester = udf::UDFTester<CeilUDF>();
  udf_tester.ForInput(4.0).Expect(4);
  udf_tester.ForInput(4.5).Expect(5);
}

TEST(MathOps, basic_floor_test) {
  auto udf_tester = udf::UDFTester<FloorUDF>();
  udf_tester.ForInput(4.0).Expect(4);
  udf_tester.ForInput(4.5).Expect(4);
}

TEST(MathOps, basic_int64_equal_test) {
  auto udf_tester = udf::UDFTester<EqualUDF<types::Int64Value, types::Int64Value>>();
  udf_tester.ForInput(-4, -4).Expect(true);
  udf_tester.ForInput(5, 2).Expect(false);
}

TEST(MathOps, basic_string_equal_test) {
  auto udf_tester = udf::UDFTester<EqualUDF<types::StringValue>>();
  udf_tester.ForInput("abc", "abc").Expect(true);
  udf_tester.ForInput("abc", "abb").Expect(false);
}

TEST(MathOps, basic_int64_not_equal_test) {
  auto udf_tester = udf::UDFTester<NotEqualUDF<types::Int64Value>>();
  udf_tester.ForInput(-4, -4).Expect(false);
  udf_tester.ForInput(5, 2).Expect(true);
}

TEST(MathOps, basic_bool_equal_test) {
  auto udf_tester = udf::UDFTester<EqualUDF<types::BoolValue>>();
  udf_tester.ForInput(false, true).Expect(false);
  udf_tester.ForInput(true, false).Expect(false);
  udf_tester.ForInput(true, true).Expect(true);
  udf_tester.ForInput(false, false).Expect(true);
}

TEST(MathOps, basic_bool_not_equal_test) {
  auto udf_tester = udf::UDFTester<NotEqualUDF<types::BoolValue>>();
  udf_tester.ForInput(false, true).Expect(true);
  udf_tester.ForInput(true, false).Expect(true);
  udf_tester.ForInput(true, true).Expect(false);
  udf_tester.ForInput(false, false).Expect(false);
}

TEST(MathOps, basic_int_bool_not_equal_test) {
  auto udf_tester = udf::UDFTester<NotEqualUDF<types::Int64Value, types::BoolValue>>();
  udf_tester.ForInput(0, true).Expect(true);
  udf_tester.ForInput(1, false).Expect(true);
  udf_tester.ForInput(0, false).Expect(false);
  // If you look at the python compiler, you can use
  // any n > 1 is true if used in program structures
  // however, `2 != True` evaluates to `True`.
  udf_tester.ForInput(2, true).Expect(true);
}

TEST(MathOps, basic_int_bool_equal_test) {
  auto udf_tester = udf::UDFTester<EqualUDF<types::Int64Value, types::BoolValue>>();
  udf_tester.ForInput(0, true).Expect(false);
  udf_tester.ForInput(1, false).Expect(false);
  udf_tester.ForInput(0, false).Expect(true);
  udf_tester.ForInput(1, true).Expect(true);
  // If you look at the python compiler, you can use
  // any n > 1 is true if used in program structures
  // however, `2 == True` evaluates to `False`.
  udf_tester.ForInput(2, true).Expect(false);
}

TEST(MathOps, basic_float_int_not_equal_test) {
  auto udf_tester = udf::UDFTester<NotEqualUDF<types::Float64Value, types::Int64Value>>();
  udf_tester.ForInput(1.0, 1).Expect(false);
  udf_tester.ForInput(1.5, 1).Expect(true);
  udf_tester.ForInput(100.0, 1).Expect(true);
  udf_tester.ForInput(3.14159, 3).Expect(true);
}

TEST(MathOps, basic_float_int_equal_test) {
  auto udf_tester = udf::UDFTester<EqualUDF<types::Float64Value, types::Int64Value>>();
  udf_tester.ForInput(1.0, 1).Expect(true);
  udf_tester.ForInput(1.5, 1).Expect(false);
  udf_tester.ForInput(100.0, 1).Expect(false);
  udf_tester.ForInput(3.14159, 3).Expect(false);
}

TEST(MathOps, basic_uint128_equal_test) {
  auto udf_tester = udf::UDFTester<EqualUDF<types::UInt128Value>>();
  udf_tester.ForInput(types::UInt128Value(1, 2), absl::MakeUint128(1, 2)).Expect(true);
  udf_tester.ForInput(types::UInt128Value(1, 2), absl::uint128(12)).Expect(false);
  udf_tester.ForInput(types::UInt128Value(551, 2), absl::MakeUint128(551, 2)).Expect(true);
}

TEST(MathOps, basic_string_not_equal_test) {
  auto udf_tester = udf::UDFTester<NotEqualUDF<types::StringValue>>();
  udf_tester.ForInput("abc", "abc").Expect(false);
  udf_tester.ForInput("abc", "abb").Expect(true);
}

TEST(MathOps, basic_float64_approx_equal_test) {
  auto udf_tester = udf::UDFTester<ApproxEqualUDF>();
  udf_tester.ForInput(-4.1234, -4.1234).Expect(true);
  udf_tester.ForInput(-4.003, -4.008).Expect(false);
}

TEST(MathOps, basic_float64_approx_not_equal_test) {
  auto udf_tester = udf::UDFTester<ApproxNotEqualUDF>();
  udf_tester.ForInput(-4.1234, -4.1234).Expect(false);
  udf_tester.ForInput(-4.003, -4.008).Expect(true);
}

TEST(MathOps, basic_int64_greater_than_test) {
  auto udf_tester = udf::UDFTester<GreaterThanUDF<types::Int64Value>>();
  udf_tester.ForInput(-4, -4).Expect(false);
  udf_tester.ForInput(5, 2).Expect(true);
  udf_tester.ForInput(2, 5).Expect(false);
}

TEST(MathOps, basic_float64_greater_than_test) {
  auto udf_tester = udf::UDFTester<GreaterThanUDF<types::Float64Value>>();
  udf_tester.ForInput(-4.64, -4.64).Expect(false);
  udf_tester.ForInput(5.1, 2.5).Expect(true);
  udf_tester.ForInput(2.5, 5.1).Expect(false);
}

TEST(MathOps, basic_mixed_greater_than_test) {
  auto udf_tester = udf::UDFTester<GreaterThanUDF<types::Int64Value, types::Float64Value>>();
  udf_tester.ForInput(-4, -4.64).Expect(true);
  udf_tester.ForInput(6, 2.5).Expect(true);
  udf_tester.ForInput(2, 5.1).Expect(false);
  udf_tester.ForInput(2, 2.0).Expect(false);
}

TEST(MathOps, basic_string_greater_than_test) {
  auto udf_tester = udf::UDFTester<GreaterThanUDF<types::StringValue>>();
  udf_tester.ForInput("abc", "bcd").Expect(false);
  udf_tester.ForInput("bcd", "abc").Expect(true);
  udf_tester.ForInput("abc", "abc").Expect(false);
}

TEST(MathOps, basic_int64_greater_than_equal_test) {
  auto udf_tester = udf::UDFTester<GreaterThanEqualUDF<types::Int64Value>>();
  udf_tester.ForInput(-4, -4).Expect(true);
  udf_tester.ForInput(5, 2).Expect(true);
  udf_tester.ForInput(2, 5).Expect(false);
}
TEST(MathOps, basic_float64_greater_than_equal_test) {
  auto udf_tester = udf::UDFTester<GreaterThanEqualUDF<types::Float64Value>>();
  udf_tester.ForInput(-4.64, -4.64).Expect(true);
  udf_tester.ForInput(5.1, 2.5).Expect(true);
  udf_tester.ForInput(2.5, 5.1).Expect(false);
}

TEST(MathOps, basic_mixed_greater_than_equal_test) {
  auto udf_tester = udf::UDFTester<GreaterThanEqualUDF<types::Int64Value, types::Float64Value>>();
  udf_tester.ForInput(-4, -4.64).Expect(true);
  udf_tester.ForInput(6, 2.5).Expect(true);
  udf_tester.ForInput(2, 5.1).Expect(false);
  udf_tester.ForInput(2, 2.0).Expect(true);
}

TEST(MathOps, basic_string_greater_than_equal_test) {
  auto udf_tester = udf::UDFTester<GreaterThanEqualUDF<types::StringValue>>();
  udf_tester.ForInput("abc", "bcd").Expect(false);
  udf_tester.ForInput("bcd", "abc").Expect(true);
  udf_tester.ForInput("abc", "abc").Expect(true);
}

TEST(MathOps, basic_int64_less_than_test) {
  auto udf_tester = udf::UDFTester<LessThanUDF<types::Int64Value>>();
  udf_tester.ForInput(-4, -4).Expect(false);
  udf_tester.ForInput(5, 2).Expect(false);
  udf_tester.ForInput(2, 5).Expect(true);
}

TEST(MathOps, basic_float64_less_than_test) {
  auto udf_tester = udf::UDFTester<LessThanUDF<types::Float64Value>>();
  udf_tester.ForInput(-4.64, -4.64).Expect(false);
  udf_tester.ForInput(5.1, 2.5).Expect(false);
  udf_tester.ForInput(2.5, 5.1).Expect(true);
}

TEST(MathOps, basic_mixed_less_than_test) {
  auto udf_tester = udf::UDFTester<LessThanUDF<types::Int64Value, types::Float64Value>>();
  udf_tester.ForInput(-4, -4.64).Expect(false);
  udf_tester.ForInput(6, 2.5).Expect(false);
  udf_tester.ForInput(2, 5.1).Expect(true);
  udf_tester.ForInput(2, 2.0).Expect(false);
}

TEST(MathOps, basic_string_less_than_test) {
  auto udf_tester = udf::UDFTester<LessThanUDF<types::StringValue>>();
  udf_tester.ForInput("abc", "bcd").Expect(true);
  udf_tester.ForInput("bcd", "abc").Expect(false);
  udf_tester.ForInput("abc", "abc").Expect(false);
}

TEST(MathOps, basic_int64_less_than_equal_test) {
  auto udf_tester = udf::UDFTester<LessThanEqualUDF<types::Int64Value>>();
  udf_tester.ForInput(-4, -4).Expect(true);
  udf_tester.ForInput(5, 2).Expect(false);
  udf_tester.ForInput(2, 5).Expect(true);
}

TEST(MathOps, basic_float64_less_than_equal_test) {
  auto udf_tester = udf::UDFTester<LessThanEqualUDF<types::Float64Value>>();
  udf_tester.ForInput(-4.64, -4.64).Expect(true);
  udf_tester.ForInput(5.1, 2.5).Expect(false);
  udf_tester.ForInput(2.5, 5.1).Expect(true);
}

TEST(MathOps, basic_mixed_less_than_equal_test) {
  auto udf_tester = udf::UDFTester<LessThanEqualUDF<types::Int64Value, types::Float64Value>>();
  udf_tester.ForInput(-4, -4.64).Expect(false);
  udf_tester.ForInput(6, 2.5).Expect(false);
  udf_tester.ForInput(2, 5.1).Expect(true);
  udf_tester.ForInput(2, 2.0).Expect(true);
}

TEST(MathOps, basic_string_less_than_equal_test) {
  auto udf_tester = udf::UDFTester<LessThanEqualUDF<types::StringValue>>();
  udf_tester.ForInput("abc", "bcd").Expect(true);
  udf_tester.ForInput("bcd", "abc").Expect(false);
  udf_tester.ForInput("abc", "abc").Expect(true);
}

TEST(MathOps, int_int_bin_test) {
  auto udf_tester = udf::UDFTester<BinUDF<types::Int64Value>>();
  udf_tester.ForInput(11, 2).Expect(10);
}

TEST(MathOps, int_float_bin_test) {
  auto udf_tester =
      udf::UDFTester<BinUDF<types::Int64Value, types::Float64Value, types::Int64Value>>();
  udf_tester.ForInput(11.5, 2).Expect(10);
}

TEST(MathOps, int_time_bin_test) {
  auto udf_tester =
      udf::UDFTester<BinUDF<types::Int64Value, types::Int64Value, types::Time64NSValue>>();
  udf_tester.ForInput(0, 2).Expect(0);
}

TEST(MathOps, time_int_bin_test) {
  auto udf_tester =
      udf::UDFTester<BinUDF<types::Time64NSValue, types::Time64NSValue, types::Int64Value>>();
  udf_tester.ForInput(11, 3).Expect(9);
}

TEST(MathOps, time_time_bin_test) {
  auto udf_tester =
      udf::UDFTester<BinUDF<types::Time64NSValue, types::Time64NSValue, types::Int64Value>>();
  udf_tester.ForInput(11, 2).Expect(10);
}

TEST(MathOps, round_test) {
  auto udf_tester = udf::UDFTester<RoundUDF>();
  udf_tester.ForInput(11.2, 2).Expect("11.20");
  udf_tester.ForInput(11.23456, 2).Expect("11.23");
  udf_tester.ForInput(0.0, 5).Expect("0.00000");
  udf_tester.ForInput(20.29438, 0).Expect("20");
  udf_tester.ForInput(1234, 3).Expect("1234.000");
}

TEST(MathOps, time_to_int64_test) {
  auto udf_tester = udf::UDFTester<TimeToInt64UDF>();
  udf_tester.ForInput(0).Expect(0);
}

TEST(MathOps, int64_to_time_test) {
  auto udf_tester = udf::UDFTester<Int64ToTimeUDF>();
  udf_tester.ForInput(0).Expect(0);
}

TEST(MathOps, basic_float64_mean_uda_test) {
  auto inputs = std::vector<double>({1.234, 2.442, 1.04, 5.322, 6.333});
  uint64_t size = inputs.size();
  double expected_mean =
      std::accumulate(std::begin(inputs), std::end(inputs), 0.0,
                      [&](double memo, double val) { return memo + (val / size); });

  auto uda_tester = udf::UDATester<MeanUDA<types::Float64Value>>();
  uda_tester.ForInput(1.234).ForInput(2.442).ForInput(1.04).ForInput(5.322).ForInput(6.333).Expect(
      expected_mean);
}

TEST(MathOps, basic_bool_mean_uda_test) {
  auto inputs = std::vector<bool>({true, true, false, false, false, false});
  uint64_t size = inputs.size();
  double expected_mean =
      std::accumulate(std::begin(inputs), std::end(inputs), 0.0,
                      [&](double memo, double val) { return memo + (val / size); });

  auto uda_tester = udf::UDATester<MeanUDA<types::BoolValue>>();
  uda_tester.ForInput(true)
      .ForInput(true)
      .ForInput(false)
      .ForInput(false)
      .ForInput(false)
      .ForInput(false)
      .Expect(expected_mean);
}

TEST(MathOps, basic_int64_mean_uda_test) {
  auto inputs = std::vector<uint64_t>({3, 6, 10, 5, 2});
  uint64_t size = inputs.size();
  double expected_mean =
      std::accumulate(std::begin(inputs), std::end(inputs), 0.0,
                      [&](double memo, double val) { return memo + (val / size); });

  auto uda_tester = udf::UDATester<MeanUDA<types::Int64Value>>();
  uda_tester.ForInput(3).ForInput(6).ForInput(10).ForInput(5).ForInput(2).Expect(expected_mean);
}

TEST(MathOps, merge_mean_uda_test) {
  auto inputs = std::vector<uint64_t>({3, 6, 10, 5, 2, 1, 4, 5, 2, 8});
  uint64_t size = inputs.size();
  double expected_mean =
      std::accumulate(std::begin(inputs), std::end(inputs), 0.0,
                      [&](double memo, double val) { return memo + (val / size); });

  auto uda_tester = udf::UDATester<MeanUDA<types::Int64Value>>();
  uda_tester.ForInput(3).ForInput(6).ForInput(10).ForInput(5).ForInput(2);

  auto other_uda_tester = udf::UDATester<MeanUDA<types::Int64Value>>();
  other_uda_tester.ForInput(1).ForInput(4).ForInput(5).ForInput(2).ForInput(8);

  uda_tester.Merge(&other_uda_tester).Expect(expected_mean);
}

TEST(MathOps, basic_float64_sum_uda_test) {
  auto inputs = std::vector<double>({1.234, 2.442, 1.04, 5.322, 6.333});
  double expected_sum = std::accumulate(std::begin(inputs), std::end(inputs), 0.0,
                                        [&](double memo, double val) { return memo + val; });

  auto uda_tester = udf::UDATester<SumUDA<types::Float64Value>>();
  uda_tester.ForInput(1.234).ForInput(2.442).ForInput(1.04).ForInput(5.322).ForInput(6.333).Expect(
      expected_sum);
}

TEST(MathOps, basic_bool_sum_uda_test) {
  auto inputs = std::vector<bool>({0, 1, 1, 0, 0});
  int64_t expected_sum = std::accumulate(std::begin(inputs), std::end(inputs), 0,
                                         [&](int64_t memo, bool val) { return memo + val; });

  auto uda_tester = udf::UDATester<SumUDA<types::BoolValue, types::Int64Value>>();
  uda_tester.ForInput(0).ForInput(1).ForInput(1).ForInput(0).ForInput(0).Expect(expected_sum);
}

TEST(MathOps, basic_int64_sum_uda_test) {
  auto inputs = std::vector<uint64_t>({3, 6, 10, 5, 2});
  uint64_t expected_sum = std::accumulate(std::begin(inputs), std::end(inputs), 0,
                                          [&](uint64_t memo, uint64_t val) { return memo + val; });

  auto uda_tester = udf::UDATester<SumUDA<types::Int64Value>>();
  uda_tester.ForInput(3).ForInput(6).ForInput(10).ForInput(5).ForInput(2).Expect(expected_sum);
}

TEST(MathOps, merge_sum_test) {
  auto inputs = std::vector<uint64_t>({3, 6, 10, 5, 2, 1, 4, 5, 2, 8});
  uint64_t expected_sum = std::accumulate(std::begin(inputs), std::end(inputs), 0,
                                          [&](uint64_t memo, uint64_t val) { return memo + val; });

  auto uda_tester = udf::UDATester<SumUDA<types::Int64Value>>();
  uda_tester.ForInput(3).ForInput(6).ForInput(10).ForInput(5).ForInput(2);

  auto other_uda_tester = udf::UDATester<SumUDA<types::Int64Value>>();
  other_uda_tester.ForInput(1).ForInput(4).ForInput(5).ForInput(2).ForInput(8);

  uda_tester.Merge(&other_uda_tester).Expect(expected_sum);
}

TEST(MathOps, basic_int64_max_uda_test) {
  auto uda_tester = udf::UDATester<MaxUDA<types::Int64Value>>();
  uda_tester.ForInput(3).Expect(3);
  uda_tester.ForInput(5).ForInput(2).ForInput(7).ForInput(1).Expect(7);
}

TEST(MathOps, merge_max_test) {
  auto uda_tester = udf::UDATester<MaxUDA<types::Int64Value>>();
  uda_tester.ForInput(3).ForInput(6).ForInput(10).ForInput(5).ForInput(2);

  auto other_uda_tester = udf::UDATester<MaxUDA<types::Int64Value>>();
  other_uda_tester.ForInput(1).ForInput(4).ForInput(5).ForInput(2).ForInput(11);

  uda_tester.Merge(&other_uda_tester).Expect(11);

  auto another_uda_tester = udf::UDATester<MaxUDA<types::Int64Value>>();
  another_uda_tester.Merge(&uda_tester).Expect(11);
}

TEST(MathOps, basic_int64_min_uda_test) {
  auto uda_tester = udf::UDATester<MinUDA<types::Int64Value>>();
  uda_tester.ForInput(3).Expect(3);
  uda_tester.ForInput(5).ForInput(2).ForInput(7).ForInput(1).Expect(1);
}

TEST(MathOps, basic_float64_min_uda_test) {
  auto uda_tester = udf::UDATester<MinUDA<types::Float64Value>>();
  uda_tester.ForInput(-4.64).Expect(-4.64);
  uda_tester.ForInput(-4.64123445435)
      .ForInput(2.252242424)
      .ForInput(1.1)
      .ForInput(-1.1234566)
      .Expect(-4.64123445435);
}

TEST(MathOps, merge_min_test) {
  auto uda_tester = udf::UDATester<MinUDA<types::Int64Value>>();
  uda_tester.ForInput(3).ForInput(6).ForInput(10).ForInput(5).ForInput(2);

  auto other_uda_tester = udf::UDATester<MinUDA<types::Int64Value>>();
  other_uda_tester.ForInput(1).ForInput(4).ForInput(5).ForInput(2).ForInput(11);

  uda_tester.Merge(&other_uda_tester).Expect(1);

  auto another_uda_tester = udf::UDATester<MinUDA<types::Int64Value>>();
  another_uda_tester.Merge(&uda_tester).Expect(1);
}

TEST(MathOps, basic_int64_count_uda_test) {
  auto uda_tester = udf::UDATester<CountUDA<types::Int64Value>>();
  uda_tester.ForInput(5).ForInput(2).ForInput(7).ForInput(1).Expect(4);
}

TEST(MathOps, merge_count_test) {
  auto uda_tester = udf::UDATester<CountUDA<types::Int64Value>>();
  uda_tester.ForInput(3).ForInput(6).ForInput(10).ForInput(5).ForInput(2);

  auto other_uda_tester = udf::UDATester<CountUDA<types::Int64Value>>();
  other_uda_tester.ForInput(1).ForInput(4).ForInput(5).ForInput(2);

  uda_tester.Merge(&other_uda_tester).Expect(9);
}

// TODO(michellenguyen, PP-2580): We should make UDA tester automatically check Merge and Partial
// aggregates if more than one input is given. Since our UDAs are arithmetic the ordering should not
// matter.
TEST(MathOps, partial_count_test) {
  auto uda_tester = udf::UDATester<CountUDA<types::Int64Value>>();
  uda_tester.ForInput(3).ForInput(6).ForInput(10).ForInput(5).ForInput(2).Expect(5);
}
}  // namespace builtins
}  // namespace carnot
}  // namespace px
