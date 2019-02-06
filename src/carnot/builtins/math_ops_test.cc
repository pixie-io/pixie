#include <gtest/gtest.h>
#include <numeric>
#include <type_traits>
#include <vector>

#include "src/carnot/builtins/math_ops.h"
#include "src/carnot/udf/test_utils.h"
#include "src/common/macros.h"
#include "src/common/status.h"

namespace pl {
namespace carnot {
namespace builtins {

TEST(MathOps, basic_int64_add_test) {
  auto udf_tester = udf::UDFTester<AddUDF<udf::Int64Value, udf::Int64Value, udf::Int64Value>>();
  udf_tester.ForInput(1, 2).Expect(3);
}

TEST(MathOps, basic_float64_add_test) {
  auto udf_tester =
      udf::UDFTester<AddUDF<udf::Float64Value, udf::Float64Value, udf::Float64Value>>();
  udf_tester.ForInput(1.5, 2.6).Expect(4.1);
}

TEST(MathOps, basic_mixed_add_test) {
  auto udf_tester = udf::UDFTester<AddUDF<udf::Float64Value, udf::Int64Value, udf::Float64Value>>();
  udf_tester.ForInput(1, 2.6).Expect(3.6);
}

TEST(MathOps, basic_int64_subtract_test) {
  auto udf_tester =
      udf::UDFTester<SubtractUDF<udf::Int64Value, udf::Int64Value, udf::Int64Value>>();
  udf_tester.ForInput(1, 2).Expect(-1);
}

TEST(MathOps, basic_float64_subtract_test) {
  auto udf_tester =
      udf::UDFTester<SubtractUDF<udf::Float64Value, udf::Float64Value, udf::Float64Value>>();
  udf_tester.ForInput(1.5, 2.6).Expect(-1.1);
}

TEST(MathOps, basic_mixed_subtract_test) {
  auto udf_tester =
      udf::UDFTester<SubtractUDF<udf::Float64Value, udf::Float64Value, udf::Int64Value>>();
  udf_tester.ForInput(1.5, 2).Expect(-0.5);
}

TEST(MathOps, basic_int64_divide_test) {
  auto udf_tester = udf::UDFTester<DivideUDF<udf::Int64Value, udf::Int64Value, udf::Int64Value>>();
  udf_tester.ForInput(1, 2).Expect(0);
}

TEST(MathOps, basic_float64_divide_test) {
  auto udf_tester =
      udf::UDFTester<DivideUDF<udf::Float64Value, udf::Float64Value, udf::Float64Value>>();
  udf_tester.ForInput(1.5, 5).Expect(0.3);
}

TEST(MathOps, basic_mixed_divide_test) {
  auto udf_tester =
      udf::UDFTester<DivideUDF<udf::Float64Value, udf::Float64Value, udf::Int64Value>>();
  udf_tester.ForInput(1.5, 5).Expect(0.3);
}

TEST(MathOps, basic_int64_multiply_test) {
  auto udf_tester =
      udf::UDFTester<MultiplyUDF<udf::Int64Value, udf::Int64Value, udf::Int64Value>>();
  udf_tester.ForInput(1, 2).Expect(2);
}

TEST(MathOps, basic_float64_multiply_test) {
  auto udf_tester =
      udf::UDFTester<MultiplyUDF<udf::Float64Value, udf::Float64Value, udf::Float64Value>>();
  udf_tester.ForInput(1.5, 5).Expect(7.5);
}

TEST(MathOps, basic_mixed_multiply_test) {
  auto udf_tester =
      udf::UDFTester<MultiplyUDF<udf::Float64Value, udf::Float64Value, udf::Int64Value>>();
  udf_tester.ForInput(1.5, 5).Expect(7.5);
}

TEST(MathOps, basic_modulo_test) {
  auto udf_tester = udf::UDFTester<ModuloUDF>();
  udf_tester.ForInput(10, 7).Expect(3);
}

TEST(MathOps, basic_bool_or_test) {
  auto udf_tester = udf::UDFTester<LogicalOrUDF<udf::BoolValue, udf::BoolValue>>();
  udf_tester.ForInput(true, false).Expect(true);
  udf_tester.ForInput(false, false).Expect(false);
}

TEST(MathOps, basic_int64_or_test) {
  auto udf_tester = udf::UDFTester<LogicalOrUDF<udf::Int64Value, udf::Int64Value>>();
  udf_tester.ForInput(4, 0).Expect(true);
  udf_tester.ForInput(0, 0).Expect(false);
}

TEST(MathOps, basic_bool_and_test) {
  auto udf_tester = udf::UDFTester<LogicalAndUDF<udf::BoolValue, udf::BoolValue>>();
  udf_tester.ForInput(true, false).Expect(false);
  udf_tester.ForInput(false, false).Expect(false);
  udf_tester.ForInput(true, true).Expect(true);
}

TEST(MathOps, basic_int64_and_test) {
  auto udf_tester = udf::UDFTester<LogicalAndUDF<udf::Int64Value, udf::Int64Value>>();
  udf_tester.ForInput(4, 0).Expect(false);
  udf_tester.ForInput(0, 0).Expect(false);
  udf_tester.ForInput(5, 3).Expect(true);
}

TEST(MathOps, basic_bool_not_test) {
  auto udf_tester = udf::UDFTester<LogicalNotUDF<udf::BoolValue>>();
  udf_tester.ForInput(true).Expect(false);
  udf_tester.ForInput(false).Expect(true);
}

TEST(MathOps, basic_int64_not_test) {
  auto udf_tester = udf::UDFTester<LogicalNotUDF<udf::Int64Value>>();
  udf_tester.ForInput(4).Expect(false);
  udf_tester.ForInput(0).Expect(true);
}

TEST(MathOps, basic_int64_negate_test) {
  auto udf_tester = udf::UDFTester<NegateUDF<udf::Int64Value>>();
  udf_tester.ForInput(-4).Expect(4);
  udf_tester.ForInput(0).Expect(0);
  udf_tester.ForInput(4).Expect(-4);
}

TEST(MathOps, basic_int64_equal_test) {
  auto udf_tester = udf::UDFTester<EqualUDF<udf::Int64Value, udf::Int64Value>>();
  udf_tester.ForInput(-4, -4).Expect(true);
  udf_tester.ForInput(5, 2).Expect(false);
}

TEST(MathOps, basic_string_equal_test) {
  auto udf_tester = udf::UDFTester<EqualUDF<udf::StringValue, udf::StringValue>>();
  udf_tester.ForInput("abc", "abc").Expect(true);
  udf_tester.ForInput("abc", "abb").Expect(false);
}

TEST(MathOps, basic_float64_approx_equal_test) {
  auto udf_tester = udf::UDFTester<ApproxEqualUDF<udf::Float64Value, udf::Float64Value>>();
  udf_tester.ForInput(-4.1234, -4.1234).Expect(true);
  udf_tester.ForInput(-4.003, -4.008).Expect(false);
}

TEST(MathOps, basic_int64_greater_than_test) {
  auto udf_tester = udf::UDFTester<GreaterThanUDF<udf::Int64Value, udf::Int64Value>>();
  udf_tester.ForInput(-4, -4).Expect(false);
  udf_tester.ForInput(5, 2).Expect(true);
  udf_tester.ForInput(2, 5).Expect(false);
}

TEST(MathOps, basic_float64_greater_than_test) {
  auto udf_tester = udf::UDFTester<GreaterThanUDF<udf::Float64Value, udf::Float64Value>>();
  udf_tester.ForInput(-4.64, -4.64).Expect(false);
  udf_tester.ForInput(5.1, 2.5).Expect(true);
  udf_tester.ForInput(2.5, 5.1).Expect(false);
}

TEST(MathOps, basic_mixed_greater_than_test) {
  auto udf_tester = udf::UDFTester<GreaterThanUDF<udf::Int64Value, udf::Float64Value>>();
  udf_tester.ForInput(-4, -4.64).Expect(true);
  udf_tester.ForInput(6, 2.5).Expect(true);
  udf_tester.ForInput(2, 5.1).Expect(false);
  udf_tester.ForInput(2, 2.0).Expect(false);
}

TEST(MathOps, basic_string_greater_than_test) {
  auto udf_tester = udf::UDFTester<GreaterThanUDF<udf::StringValue, udf::StringValue>>();
  udf_tester.ForInput("abc", "bcd").Expect(false);
  udf_tester.ForInput("bcd", "abc").Expect(true);
  udf_tester.ForInput("abc", "abc").Expect(false);
}

TEST(MathOps, basic_int64_less_than_test) {
  auto udf_tester = udf::UDFTester<LessThanUDF<udf::Int64Value, udf::Int64Value>>();
  udf_tester.ForInput(-4, -4).Expect(false);
  udf_tester.ForInput(5, 2).Expect(false);
  udf_tester.ForInput(2, 5).Expect(true);
}

TEST(MathOps, basic_float64_less_than_test) {
  auto udf_tester = udf::UDFTester<LessThanUDF<udf::Float64Value, udf::Float64Value>>();
  udf_tester.ForInput(-4.64, -4.64).Expect(false);
  udf_tester.ForInput(5.1, 2.5).Expect(false);
  udf_tester.ForInput(2.5, 5.1).Expect(true);
}

TEST(MathOps, basic_mixed_less_than_test) {
  auto udf_tester = udf::UDFTester<LessThanUDF<udf::Int64Value, udf::Float64Value>>();
  udf_tester.ForInput(-4, -4.64).Expect(false);
  udf_tester.ForInput(6, 2.5).Expect(false);
  udf_tester.ForInput(2, 5.1).Expect(true);
  udf_tester.ForInput(2, 2.0).Expect(false);
}

TEST(MathOps, basic_string_less_than_test) {
  auto udf_tester = udf::UDFTester<LessThanUDF<udf::StringValue, udf::StringValue>>();
  udf_tester.ForInput("abc", "bcd").Expect(true);
  udf_tester.ForInput("bcd", "abc").Expect(false);
  udf_tester.ForInput("abc", "abc").Expect(false);
}

TEST(MathOps, basic_float64_mean_uda_test) {
  auto inputs = std::vector<double>({1.234, 2.442, 1.04, 5.322, 6.333});
  uint64_t size = inputs.size();
  double expected_mean =
      std::accumulate(std::begin(inputs), std::end(inputs), 0.0,
                      [&](double memo, double val) { return memo + (val / size); });

  auto uda_tester = udf::UDATester<MeanUDA<udf::Float64Value>>();
  uda_tester.ForInput(1.234).ForInput(2.442).ForInput(1.04).ForInput(5.322).ForInput(6.333).Expect(
      expected_mean);
}

TEST(MathOps, basic_int64_mean_uda_test) {
  auto inputs = std::vector<uint64_t>({3, 6, 10, 5, 2});
  uint64_t size = inputs.size();
  double expected_mean =
      std::accumulate(std::begin(inputs), std::end(inputs), 0.0,
                      [&](double memo, double val) { return memo + (val / size); });

  auto uda_tester = udf::UDATester<MeanUDA<udf::Int64Value>>();
  uda_tester.ForInput(3).ForInput(6).ForInput(10).ForInput(5).ForInput(2).Expect(expected_mean);
}

TEST(MathOps, merge_mean_uda_test) {
  auto inputs = std::vector<uint64_t>({3, 6, 10, 5, 2, 1, 4, 5, 2, 8});
  uint64_t size = inputs.size();
  double expected_mean =
      std::accumulate(std::begin(inputs), std::end(inputs), 0.0,
                      [&](double memo, double val) { return memo + (val / size); });

  auto uda_tester = udf::UDATester<MeanUDA<udf::Int64Value>>();
  uda_tester.ForInput(3).ForInput(6).ForInput(10).ForInput(5).ForInput(2);

  auto other_uda_tester = udf::UDATester<MeanUDA<udf::Int64Value>>();
  other_uda_tester.ForInput(1).ForInput(4).ForInput(5).ForInput(2).ForInput(8);

  uda_tester.Merge(other_uda_tester).Expect(expected_mean);
}

TEST(MathOps, basic_float64_sum_uda_test) {
  auto inputs = std::vector<double>({1.234, 2.442, 1.04, 5.322, 6.333});
  double expected_sum = std::accumulate(std::begin(inputs), std::end(inputs), 0.0,
                                        [&](double memo, double val) { return memo + val; });

  auto uda_tester = udf::UDATester<SumUDA<udf::Float64Value>>();
  uda_tester.ForInput(1.234).ForInput(2.442).ForInput(1.04).ForInput(5.322).ForInput(6.333).Expect(
      expected_sum);
}

TEST(MathOps, basic_int64_sum_uda_test) {
  auto inputs = std::vector<uint64_t>({3, 6, 10, 5, 2});
  double expected_sum = std::accumulate(std::begin(inputs), std::end(inputs), 0.0,
                                        [&](double memo, double val) { return memo + val; });

  auto uda_tester = udf::UDATester<SumUDA<udf::Int64Value>>();
  uda_tester.ForInput(3).ForInput(6).ForInput(10).ForInput(5).ForInput(2).Expect(expected_sum);
}

TEST(MathOps, merge_sum_test) {
  auto inputs = std::vector<uint64_t>({3, 6, 10, 5, 2, 1, 4, 5, 2, 8});
  double expected_sum = std::accumulate(std::begin(inputs), std::end(inputs), 0.0,
                                        [&](double memo, double val) { return memo + val; });

  auto uda_tester = udf::UDATester<SumUDA<udf::Int64Value>>();
  uda_tester.ForInput(3).ForInput(6).ForInput(10).ForInput(5).ForInput(2);

  auto other_uda_tester = udf::UDATester<SumUDA<udf::Int64Value>>();
  other_uda_tester.ForInput(1).ForInput(4).ForInput(5).ForInput(2).ForInput(8);

  uda_tester.Merge(other_uda_tester).Expect(expected_sum);
}

TEST(MathOps, basic_int64_max_uda_test) {
  auto uda_tester = udf::UDATester<MaxUDA<udf::Int64Value>>();
  uda_tester.ForInput(3).Expect(3);
  uda_tester.ForInput(5).ForInput(2).ForInput(7).ForInput(1).Expect(7);
}

TEST(MathOps, merge_max_test) {
  auto uda_tester = udf::UDATester<MaxUDA<udf::Int64Value>>();
  uda_tester.ForInput(3).ForInput(6).ForInput(10).ForInput(5).ForInput(2);

  auto other_uda_tester = udf::UDATester<MaxUDA<udf::Int64Value>>();
  other_uda_tester.ForInput(1).ForInput(4).ForInput(5).ForInput(2).ForInput(11);

  uda_tester.Merge(other_uda_tester).Expect(11);

  auto another_uda_tester = udf::UDATester<MaxUDA<udf::Int64Value>>();
  another_uda_tester.Merge(uda_tester).Expect(11);
}

TEST(MathOps, basic_int64_min_uda_test) {
  auto uda_tester = udf::UDATester<MinUDA<udf::Int64Value>>();
  uda_tester.ForInput(3).Expect(3);
  uda_tester.ForInput(5).ForInput(2).ForInput(7).ForInput(1).Expect(1);
}

TEST(MathOps, merge_min_test) {
  auto uda_tester = udf::UDATester<MinUDA<udf::Int64Value>>();
  uda_tester.ForInput(3).ForInput(6).ForInput(10).ForInput(5).ForInput(2);

  auto other_uda_tester = udf::UDATester<MinUDA<udf::Int64Value>>();
  other_uda_tester.ForInput(1).ForInput(4).ForInput(5).ForInput(2).ForInput(11);

  uda_tester.Merge(other_uda_tester).Expect(1);

  auto another_uda_tester = udf::UDATester<MinUDA<udf::Int64Value>>();
  another_uda_tester.Merge(uda_tester).Expect(1);
}

TEST(MathOps, basic_int64_count_uda_test) {
  auto uda_tester = udf::UDATester<CountUDA<udf::Int64Value>>();
  uda_tester.ForInput(5).ForInput(2).ForInput(7).ForInput(1).Expect(4);
}

TEST(MathOps, merge_count_test) {
  auto uda_tester = udf::UDATester<CountUDA<udf::Int64Value>>();
  uda_tester.ForInput(3).ForInput(6).ForInput(10).ForInput(5).ForInput(2);

  auto other_uda_tester = udf::UDATester<CountUDA<udf::Int64Value>>();
  other_uda_tester.ForInput(1).ForInput(4).ForInput(5).ForInput(2);

  uda_tester.Merge(other_uda_tester).Expect(9);
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
