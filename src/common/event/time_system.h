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

#include "src/common/event/timer.h"

namespace px {
namespace event {

// Alias to make it easier to reference.
using SystemTimePoint = std::chrono::time_point<std::chrono::system_clock>;
using MonotonicTimePoint = std::chrono::time_point<std::chrono::steady_clock>;

// Scheduler needs to be forward declared to prevent a circular dependency.
class Scheduler;
using SchedulerUPtr = std::unique_ptr<Scheduler>;

/**
 * Captures a system-time source, capable of computing both monotonically increasing
 * and real time.
 */
class TimeSource {
 public:
  virtual ~TimeSource() = default;

  /**
   * @return the current system time; not guaranteed to be monotonically increasing.
   */
  virtual SystemTimePoint SystemTime() const = 0;

  /**
   * @return the current monotonic time.
   */
  virtual MonotonicTimePoint MonotonicTime() const = 0;
};

class TimeSystem : public TimeSource {
 public:
  ~TimeSystem() override = default;

  using Duration = MonotonicTimePoint::duration;

  /**
   * Creates a timer factory. This indirection enables thread-local timer-queue management,
   * so servers can have a separate timer-factory in each thread.
   */
  virtual SchedulerUPtr CreateScheduler(Scheduler* base_scheduler) = 0;
};

}  // namespace event
}  // namespace px
