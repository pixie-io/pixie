#include <gtest/gtest.h>
#include <numeric>
#include <type_traits>
#include <vector>

#include "src/carnot/builtins/math_ops.h"
#include "src/utils/macros.h"
#include "src/utils/status.h"

namespace pl {
namespace carnot {
namespace builtins {

TEST(MathOps, basic_float64_mean_uda_test) {
  udf::FunctionContext ctx;
  auto meanUDA = MeanUDA<udf::Float64Value>();
  auto inputs = std::vector<double>({1.234, 2.442, 1.04, 5.322, 6.333});
  for (auto& val : inputs) {
    meanUDA.Update(&ctx, val);
  }

  uint64_t size = inputs.size();
  double expected_mean =
      std::accumulate(std::begin(inputs), std::end(inputs), 0.0,
                      [&](double memo, double val) { return memo + (val / size); });

  EXPECT_DOUBLE_EQ(meanUDA.Finalize(&ctx).val, expected_mean);
}

TEST(MathOps, basic_int64_mean_uda_test) {
  udf::FunctionContext ctx;
  auto meanUDA = MeanUDA<udf::Int64Value>();
  auto inputs = std::vector<uint64_t>({3, 6, 10, 5, 2});
  for (auto& val : inputs) {
    meanUDA.Update(&ctx, val);
  }

  uint64_t size = inputs.size();
  double expected_mean =
      std::accumulate(std::begin(inputs), std::end(inputs), 0.0,
                      [&](double memo, double val) { return memo + (val / size); });
  EXPECT_DOUBLE_EQ(meanUDA.Finalize(&ctx).val, expected_mean);
}

TEST(MathOps, merge_mean_uda_test) {
  udf::FunctionContext ctx;
  auto meanUDA1 = MeanUDA<udf::Int64Value>();
  auto inputs1 = std::vector<uint64_t>({3, 6, 10, 5, 2});
  for (auto& val : inputs1) {
    meanUDA1.Update(&ctx, val);
  }

  auto meanUDA2 = MeanUDA<udf::Int64Value>();
  auto inputs2 = std::vector<uint64_t>({1, 4, 5, 2, 8});
  for (auto& val : inputs2) {
    meanUDA2.Update(&ctx, val);
  }
  meanUDA1.Merge(&ctx, meanUDA2);

  auto inputs = std::vector<uint64_t>({3, 6, 10, 5, 2, 1, 4, 5, 2, 8});
  uint64_t size = inputs.size();
  double expected_mean =
      std::accumulate(std::begin(inputs), std::end(inputs), 0.0,
                      [&](double memo, double val) { return memo + (val / size); });

  EXPECT_DOUBLE_EQ(meanUDA1.Finalize(&ctx).val, expected_mean);
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
