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

#include <gtest/gtest.h>

#include "src/common/system/clock.h"
#include "src/common/testing/testing.h"
#include "src/stirling/testing/common.h"
#include "src/stirling/utils/run_core_stats.h"

namespace px {
namespace stirling {

TEST(RunCoreStatsTest, RunCoreStatsTest) {
  RunCoreStats stats;

  // An iteration with nothing happening.
  // We expect the "no work" count & histogram to be incremented.
  stats.EndIter(std::chrono::milliseconds{1000});
  EXPECT_EQ(1, stats.num_main_loop_iters());
  EXPECT_EQ(0, stats.num_push_data());
  EXPECT_EQ(0, stats.num_transfer_data());
  EXPECT_EQ(1, stats.num_no_work_iters());
  EXPECT_EQ(1, stats.SleepCountForDuration(std::chrono::milliseconds{1000}));
  EXPECT_EQ(1, stats.NoWorkCountForDuration(std::chrono::milliseconds{1000}));

  // A normal iteration.
  stats.IncrementPushDataCount();
  stats.IncrementTransferDataCount();
  stats.EndIter(std::chrono::milliseconds{1000});
  EXPECT_EQ(2, stats.num_main_loop_iters());
  EXPECT_EQ(1, stats.num_push_data());
  EXPECT_EQ(1, stats.num_transfer_data());
  EXPECT_EQ(1, stats.num_no_work_iters());
  EXPECT_EQ(2, stats.SleepCountForDuration(std::chrono::milliseconds{1000}));
  EXPECT_EQ(1, stats.NoWorkCountForDuration(std::chrono::milliseconds{1000}));

  // Another normal iteration, with a different sleep duration.
  stats.IncrementPushDataCount();
  stats.EndIter(std::chrono::milliseconds{10});
  EXPECT_EQ(3, stats.num_main_loop_iters());
  EXPECT_EQ(2, stats.num_push_data());
  EXPECT_EQ(1, stats.num_transfer_data());
  EXPECT_EQ(1, stats.num_no_work_iters());
  EXPECT_EQ(2, stats.SleepCountForDuration(std::chrono::milliseconds{1000}));
  EXPECT_EQ(1, stats.NoWorkCountForDuration(std::chrono::milliseconds{1000}));
  EXPECT_EQ(1, stats.SleepCountForDuration(std::chrono::milliseconds{10}));
  EXPECT_EQ(0, stats.SleepCountForDuration(std::chrono::milliseconds{1}));
  EXPECT_EQ(0, stats.NoWorkCountForDuration(std::chrono::milliseconds{2}));

  // Now we can admire the printout.
  stats.LogStats();
}

}  // namespace stirling
}  // namespace px
