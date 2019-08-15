#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>

#include "src/common/base/utils.h"

namespace pl {

using ::testing::StrEq;

TEST(ReprTest, ResultsAreAsExpected) {
  EXPECT_THAT(Repr("test\b"), StrEq(R"(test\x08)"));
  EXPECT_THAT(Repr("test\xab"), StrEq(R"(test\xAB)"));
  EXPECT_THAT(Repr("test\b", Radix::kBin, PrintConvPolicy::kToDigit),
              StrEq(R"(\b01110100\b01100101\b01110011\b01110100\b00001000)"));
}

TEST(Enumerate, LoopsThroughVectorWithIndex) {
  std::vector<int> vals = {0, 2, 4, 6, 8};

  for (const auto& [idx, val] : Enumerate(vals)) {
    EXPECT_EQ(idx * 2, val);
  }
}

}  // namespace pl
