#include <gtest/gtest.h>
#include <vector>

#include <taskflow/taskflow.hpp>

// TODO(zasgar): This can be removed when we have code using cpp taskflow.
TEST(cpp_taskflow, simple_test) {
  tf::Taskflow tf;
  std::vector<int> data{5, 4, 3, 2};
  auto tmin = std::numeric_limits<int>::max();
  tf.reduce(data.begin(), data.end(), tmin,
            [](const auto& l, const auto& r) { return std::min(l, r); });
  tf.wait_for_all();
  EXPECT_EQ(2, tmin);
}
