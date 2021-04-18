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

namespace px {
namespace chrono {

// An std::chrono style clock that is based on CLOCK_BOOTTIME.
// This is in contrast to std::steady_clock which is based on CLOCK_MONOTONIC.
class boot_clock {
 public:
  typedef std::chrono::nanoseconds duration;
  typedef std::chrono::time_point<boot_clock, duration> time_point;

  static inline time_point now() {
    timespec ts;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    return time_point(std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec));
  }
};

}  // namespace chrono
}  // namespace px
