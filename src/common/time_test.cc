#include <gtest/gtest.h>

#include "src/common/time.h"

namespace pl {

TEST(StringToTime, basic) {
  EXPECT_EQ(-120000000000, StringToTimeInt("-2m").ConsumeValueOrDie());
  EXPECT_EQ(120000000000, StringToTimeInt("2m").ConsumeValueOrDie());
  EXPECT_EQ(-10000000000, StringToTimeInt("-10s").ConsumeValueOrDie());
  EXPECT_EQ(864000000000000, StringToTimeInt("10d").ConsumeValueOrDie());
  EXPECT_EQ(18000000000000, StringToTimeInt("5h").ConsumeValueOrDie());
  EXPECT_EQ(10000000, StringToTimeInt("10ms").ConsumeValueOrDie());
}

TEST(StringToTime, invalid_format) { EXPECT_FALSE(StringToTimeInt("hello").ok()); }

TEST(PrettyDuration, strings) {
  EXPECT_EQ("1.23 \u03BCs", PrettyDuration(1230));
  EXPECT_EQ("14.56 ms", PrettyDuration(14561230));
  EXPECT_EQ("14.56 s", PrettyDuration(14561230000));
}

}  // namespace pl
