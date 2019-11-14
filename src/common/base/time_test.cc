#include <gtest/gtest.h>

#include "src/common/base/time.h"
#include "src/common/testing/testing.h"

namespace pl {

TEST(StringToRange, basic) {
  auto res = StringToTimeRange("5,20");
  EXPECT_OK(res);
  auto output_pair = res.ConsumeValueOrDie();
  EXPECT_EQ(5, output_pair.first);
  EXPECT_EQ(20, output_pair.second);
}

TEST(StringToRange, invalid_format) {
  auto res = StringToTimeRange("hi");
  EXPECT_FALSE(res.ok());
}

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
