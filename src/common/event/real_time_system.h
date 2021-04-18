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

#pragma once

#include <chrono>
#include <functional>
#include <memory>

#include "src/common/event/time_system.h"

namespace px {
namespace event {

/**
 * Real-world time implementation of TimeSource.
 */
class RealTimeSource : public TimeSource {
 public:
  SystemTimePoint SystemTime() const override { return std::chrono::system_clock::now(); }
  MonotonicTimePoint MonotonicTime() const override { return std::chrono::steady_clock::now(); }
};

class RealTimeSystem : public TimeSystem {
 public:
  SchedulerUPtr CreateScheduler(Scheduler* base_scheduler) override;

  SystemTimePoint SystemTime() const override { return time_source_.SystemTime(); }
  MonotonicTimePoint MonotonicTime() const override { return time_source_.MonotonicTime(); }

  const TimeSource& time_source() { return time_source_; }

 private:
  RealTimeSource time_source_;
};

}  // namespace event
}  // namespace px
