#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <vector>

#include "src/carnot/funcs/builtins/collections.h"
#include "src/carnot/udf/test_utils.h"

namespace pl {
namespace carnot {
namespace builtins {

TEST(CollectionsTest, AnyUDA) {
  auto uda_tester = udf::UDATester<AnyUDA<types::Float64Value>>();
  std::vector<types::Float64Value> vals = {
      1.234, 2.442, 1.04, 5.322, 6.333,
  };

  for (const auto& val : vals) {
    uda_tester.ForInput(val);
  }

  EXPECT_THAT(vals, ::testing::Contains(uda_tester.Result()));
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
