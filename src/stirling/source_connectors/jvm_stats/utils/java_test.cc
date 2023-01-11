/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/stirling/source_connectors/jvm_stats/utils/java.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <string_view>

#include <absl/strings/match.h>

#include "src/common/exec/subprocess.h"
#include "src/common/testing/test_environment.h"
#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace java {

// Tests that values are calculated correctly.
TEST(StatsTest, CommonValues) {
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

TEST(HsperfdataPathTest, ResultIsAsExpected) {
  const std::string javaBinPath =
      testing::BazelRunfilePath("src/stirling/source_connectors/jvm_stats/testing/HelloWorld");

  SubProcess hello_world;
  ASSERT_OK(hello_world.Start({javaBinPath, "HelloWorld"}));

  // Give some time for the JVM process to start.
  std::string s;
  while (!absl::StrContains(s, "Hello, World")) {
    ASSERT_OK(hello_world.Stdout(&s));
  }

  ASSERT_OK_AND_ASSIGN(auto hsperfdata_path, HsperfdataPath(hello_world.child_pid()));
  EXPECT_TRUE(std::filesystem::exists(hsperfdata_path));
  hello_world.Kill();
  EXPECT_EQ(9, hello_world.Wait()) << "Server should have been killed.";
}

}  // namespace java
}  // namespace stirling
}  // namespace px
