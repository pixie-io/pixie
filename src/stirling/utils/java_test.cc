#include "src/stirling/utils/java.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace pl {
namespace stirling {

// Tests that values are calculated correctly.
TEST(StatsTest, CommoneValues) {
  std::vector<Stats::Stat> stat_vec = {
      {"sun.gc.collector.0.time", 1},
      {"sun.gc.collector.1.time", 1},
      {"sun.gc.generation.0.space.0.used", 1},
      {"sun.gc.generation.0.space.1.used", 1},
      {"sun.gc.generation.0.space.2.used", 1},
      {"sun.gc.generation.1.space.0.used", 1},
      {"sun.gc.generation.0.space.0.capacity", 1},
      {"sun.gc.generation.0.space.1.capacity", 1},
      {"sun.gc.generation.0.space.2.capacity", 1},
      {"sun.gc.generation.1.space.0.capacity", 1},
      {"sun.gc.generation.0.maxCapacity", 1},
      {"sun.gc.generation.1.maxCapacity", 1},
  };
  Stats stats(std::move(stat_vec));
  EXPECT_EQ(1, stats.YoungGCTimeNanos());
  EXPECT_EQ(1, stats.FullGCTimeNanos());
  EXPECT_EQ(4, stats.UsedHeapSizeBytes());
  EXPECT_EQ(4, stats.TotalHeapSizeBytes());
  EXPECT_EQ(2, stats.MaxHeapSizeBytes());
}

}  // namespace stirling
}  // namespace pl
