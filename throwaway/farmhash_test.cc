#include <farmhash.h>
#include <gtest/gtest.h>

#include <string>

TEST(farmhash, basic) {
  const char *t = "test string";
  EXPECT_EQ(util::Hash64(t, sizeof(t)), util::Hash64(t, sizeof(t)));
}
