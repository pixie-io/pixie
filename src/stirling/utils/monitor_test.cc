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

#include "src/common/testing/testing.h"
#include "src/stirling/testing/common.h"
#include "src/stirling/utils/monitor.h"

DECLARE_bool(stirling_profiler_java_symbols);

namespace px {
namespace stirling {

TEST(MonitorTest, DisablesJavaIfCrashInWindow) {
  PX_SET_FOR_SCOPE(FLAGS_stirling_profiler_java_symbols, true);
  StirlingMonitor& monitor = *StirlingMonitor::GetInstance();
  constexpr struct upid_t kUPID = {{0}, 0};
  monitor.NotifyJavaProcessAttach(kUPID);

  EXPECT_TRUE(FLAGS_stirling_profiler_java_symbols);
  monitor.NotifyJavaProcessCrashed(kUPID);
  EXPECT_FALSE(FLAGS_stirling_profiler_java_symbols);
}

TEST(MonitorTest, DoesNotDisableAfterTrackerClear) {
  PX_SET_FOR_SCOPE(FLAGS_stirling_profiler_java_symbols, true);
  StirlingMonitor& monitor = *StirlingMonitor::GetInstance();

  constexpr struct upid_t kUPID = {{1}, 0};
  monitor.NotifyJavaProcessAttach(kUPID);

  EXPECT_TRUE(FLAGS_stirling_profiler_java_symbols);
  monitor.ResetJavaProcessAttachTrackers();
  monitor.NotifyJavaProcessCrashed(kUPID);
  EXPECT_TRUE(FLAGS_stirling_profiler_java_symbols);
}

TEST(MonitorTest, DoesNotDisableJavaIfNotInCrashInWindow) {
  PX_SET_FOR_SCOPE(FLAGS_stirling_profiler_java_symbols, true);
  StirlingMonitor& monitor = *StirlingMonitor::GetInstance();

  constexpr struct upid_t kUPID = {{2}, 0};
  monitor.NotifyJavaProcessAttach(kUPID);

  // TODO(jps): Switch over to clock injection method.
  sleep(StirlingMonitor::kCrashWindow.count());

  EXPECT_TRUE(FLAGS_stirling_profiler_java_symbols);
  monitor.NotifyJavaProcessCrashed(kUPID);
  EXPECT_TRUE(FLAGS_stirling_profiler_java_symbols);
}

}  // namespace stirling
}  // namespace px
