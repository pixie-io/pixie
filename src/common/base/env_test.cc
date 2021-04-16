#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/env.h"

namespace px {

using ::testing::IsEmpty;
using ::testing::Not;

TEST(GetEnvTest, ResultsAreAsExpected) {
  const std::string rand_string = "abcdefglkljadkfjadkfj";
  EXPECT_EQ(std::nullopt, GetEnv(rand_string));

  ASSERT_EQ(0, setenv(rand_string.c_str(), "test", /*overwrite*/ 1));
  auto value_or = GetEnv(rand_string);
  ASSERT_TRUE(value_or.has_value());
  EXPECT_EQ("test", value_or.value());
}

}  // namespace px
