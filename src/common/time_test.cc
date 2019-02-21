#include <gtest/gtest.h>

#include "src/common/time.h"

namespace pl {

TEST(StringToTime, basic) {
  EXPECT_EQ(-120000, StringToTimeInt("-2m").ConsumeValueOrDie());
  EXPECT_EQ(120000, StringToTimeInt("2m").ConsumeValueOrDie());
  EXPECT_EQ(-10000, StringToTimeInt("-10s").ConsumeValueOrDie());
  EXPECT_EQ(864000000, StringToTimeInt("10d").ConsumeValueOrDie());
  EXPECT_EQ(18000000, StringToTimeInt("5h").ConsumeValueOrDie());
  EXPECT_EQ(10, StringToTimeInt("10ms").ConsumeValueOrDie());
}

TEST(StringToTime, invalid_format) { EXPECT_FALSE(StringToTimeInt("hello").ok()); }

}  // namespace pl
