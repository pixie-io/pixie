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

#include "src/stirling/core/frequency_manager.h"

#include <chrono>

#include <gtest/gtest.h>

namespace px {
namespace stirling {

// Tests that the default constructed FrequencyManager works as expected.
TEST(FrequencyManagerTest, CheckInit) {
  FrequencyManager mgr;
  EXPECT_EQ(mgr.period(), std::chrono::milliseconds(0));
  std::chrono::steady_clock::time_point zero;
  EXPECT_EQ(mgr.next(), zero);
  EXPECT_EQ(mgr.count(), static_cast<uint32_t>(0));
  EXPECT_TRUE(mgr.Expired(std::chrono::steady_clock::now()));
}

// Tests that the sequence of setting period, checking time, and check expiration work as expected.
TEST(FrequencyManagerTest, SetPeriodEndAndCheck) {
  FrequencyManager mgr;
  mgr.set_period(std::chrono::milliseconds{10000});
  EXPECT_EQ(mgr.period(), std::chrono::milliseconds{10000});
  EXPECT_TRUE(mgr.Expired(std::chrono::steady_clock::now()));

  mgr.Reset(std::chrono::steady_clock::now());
  EXPECT_FALSE(mgr.Expired(std::chrono::steady_clock::now()));
  auto computed_period = mgr.next() - std::chrono::steady_clock::now();
  EXPECT_LE(computed_period, std::chrono::milliseconds{10000});
  EXPECT_GE(computed_period, std::chrono::milliseconds{9990});
}

}  // namespace stirling
}  // namespace px
