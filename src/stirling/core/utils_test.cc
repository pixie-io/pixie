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

#include "src/stirling/core/utils.h"

#include <chrono>

#include <gtest/gtest.h>

namespace px {
namespace stirling {

// Tests that the default constructed FrequencyManager works as expected.
TEST(FrequencyManagerTest, CheckInit) {
  FrequencyManager mgr;
  EXPECT_EQ(mgr.period(), std::chrono::milliseconds(0));
  px::chrono::coarse_steady_clock::time_point zero;
  EXPECT_EQ(mgr.next(), zero);
  EXPECT_EQ(mgr.count(), static_cast<uint32_t>(0));
  EXPECT_TRUE(mgr.Expired());
}

// Tests that the sequence of setting period, checking time, and check expiration work as expected.
TEST(FrequencyManagerTest, SetPeriodEndAndCheck) {
  FrequencyManager mgr;
  mgr.set_period(std::chrono::milliseconds{10000});
  EXPECT_EQ(mgr.period(), std::chrono::milliseconds{10000});
  EXPECT_TRUE(mgr.Expired());

  mgr.Reset();
  EXPECT_FALSE(mgr.Expired());
  auto computed_period = mgr.next() - px::chrono::coarse_steady_clock::now();
  EXPECT_LE(computed_period, std::chrono::milliseconds{10000});
  EXPECT_GE(computed_period, std::chrono::milliseconds{9990});
}

// Tests that the sampling period were set correctly for SamplePushFrequencyManager.
TEST(SamplePushFrequencyManagerTest, SetSamplingPeriodEndAndCheck) {
  SamplePushFrequencyManager mgr;
  mgr.set_sampling_period(std::chrono::milliseconds{10000});
  EXPECT_EQ(mgr.sampling_period(), std::chrono::milliseconds{10000});
  EXPECT_TRUE(mgr.SamplingRequired());

  mgr.Sample();
  EXPECT_FALSE(mgr.SamplingRequired());
  auto computed_period = mgr.NextSamplingTime() - px::chrono::coarse_steady_clock::now();
  EXPECT_LE(computed_period, std::chrono::milliseconds{10000});
  EXPECT_GE(computed_period, std::chrono::milliseconds{9990});
}

// Tests that the push period were set correctly for SamplePushFrequencyManager.
TEST(SamplePushFrequencyManagerTest, SetPushPeriodEndAndCheck) {
  SamplePushFrequencyManager mgr;
  mgr.set_push_period(std::chrono::milliseconds{10000});
  EXPECT_EQ(mgr.push_period(), std::chrono::milliseconds{10000});
  EXPECT_TRUE(mgr.PushRequired(0.5, 0));

  mgr.Push();
  EXPECT_FALSE(mgr.PushRequired(0.5, 0));
  auto computed_period = mgr.NextPushTime() - px::chrono::coarse_steady_clock::now();
  EXPECT_LE(computed_period, std::chrono::milliseconds{10000});
  EXPECT_GE(computed_period, std::chrono::milliseconds{9990});
}

}  // namespace stirling
}  // namespace px
