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
