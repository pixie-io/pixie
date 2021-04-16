#include <gtest/gtest.h>

#include "src/common/base/time.h"
#include "src/common/testing/testing.h"

namespace px {

TEST(StringToRange, basic) {
  ASSERT_OK_AND_ASSIGN(auto output_pair, StringToTimeRange("5,20"));
  EXPECT_EQ(5, output_pair.first);
  EXPECT_EQ(20, output_pair.second);
}

TEST(StringToRange, invalid_format) { ASSERT_NOT_OK(StringToTimeRange("hi")); }

TEST(StringToTime, basic) {
  EXPECT_OK_AND_EQ(StringToTimeInt("-2m"), -120000000000);
  EXPECT_OK_AND_EQ(StringToTimeInt("2m"), 120000000000);
  EXPECT_OK_AND_EQ(StringToTimeInt("-10s"), -10000000000);
  EXPECT_OK_AND_EQ(StringToTimeInt("10d"), 864000000000000);
  EXPECT_OK_AND_EQ(StringToTimeInt("5h"), 18000000000000);
  EXPECT_OK_AND_EQ(StringToTimeInt("10ms"), 10000000);
}

TEST(StringToTime, invalid_format) { EXPECT_FALSE(StringToTimeInt("hello").ok()); }

TEST(PrettyDuration, strings) {
  EXPECT_EQ("1.23 \u03BCs", PrettyDuration(1230));
  EXPECT_EQ("14.56 ms", PrettyDuration(14561230));
  EXPECT_EQ("14.56 s", PrettyDuration(14561230000));
}

}  // namespace px
