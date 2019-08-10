#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>

#include "src/common/base/utils.h"

namespace pl {

using ::testing::StrEq;

TEST(ToHexStringTest, ResultsAreAsExpected) {
  EXPECT_THAT(ToHexString("test\b"), StrEq(R"(test\x08)"));
  EXPECT_THAT(ToHexString("test\xab"), StrEq(R"(test\xAB)"));
}

TEST(Enumerate, LoopsThroughVectorWithIndex) {
  std::vector<int> vals = {0, 2, 4, 6, 8};

  for (const auto& [idx, val] : Enumerate(vals)) {
    EXPECT_EQ(idx * 2, val);
  }
}

}  // namespace pl
