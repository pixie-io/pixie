#include <gtest/gtest.h>

#include <sole.hpp>

TEST(Sole, basic) {
  sole::uuid u4 = sole::uuid4();
  EXPECT_NE("", u4.str());
}
