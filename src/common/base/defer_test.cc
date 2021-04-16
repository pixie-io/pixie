#include <gtest/gtest.h>
#include <random>

#include "src/common/base/defer.h"

namespace px {

TEST(DeferTest, Basic) {
  std::default_random_engine rng;
  std::uniform_int_distribution<int> dist(0, 1);

  for (uint32_t i = 0; i < 100; ++i) {
    int x;
    {
      x = 10;
      DEFER(x = i;);

      if (dist(rng) == 0) {
        x = 11;
      } else {
        x = 12;
      }
    }
    EXPECT_EQ(x, i);
  }
}

}  // namespace px
