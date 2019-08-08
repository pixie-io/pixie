#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/utils.h"

namespace pl {

using ::testing::StrEq;

TEST(ToHexStringTest, ResultsAreAsExpected) {
  EXPECT_THAT(ToHexString("test\b"), StrEq(R"(test\x08)"));
  EXPECT_THAT(ToHexString("test\xab"), StrEq(R"(test\xAB)"));
}

}  // namespace pl
