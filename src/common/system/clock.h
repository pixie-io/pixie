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

// An std::chrono style clock that is based on the chosen clock.
template <int TClockType>
class basic_clock {
 public:
  using duration = std::chrono::nanoseconds;
  using time_point = std::chrono::time_point<basic_clock<TClockType>, duration>;

  static inline time_point now() {
    timespec ts;
    clock_gettime(TClockType, &ts);
    return time_point(std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec));
  }
};

// For reference:
// std::chrono::steady_clock is based on CLOCK_MONOTONIC.
// std::chrono::system_clock is based on CLOCK_REALTIME.

// Clock based on CLOCK_BOOTTIME.
// Unlike CLOCK_MONOTONIC, this clock accounts for time spent while the machine is suspended.
// Note that Linux PID start times are based on this kind of clock.
using boot_clock = basic_clock<CLOCK_BOOTTIME>;

// Clock based on CLOCK_MONOTONIC_COARSE.
// This is faster, but lower resolution than CLOCK_MONOTONIC.
// However, its resolution is approx ~4 ms. and repeated calls to clock_gettime() can return
// the same timestamp if within the resolution window (i.e. if < 4 ms. have elapsed between calls).
using coarse_steady_clock = basic_clock<CLOCK_MONOTONIC_COARSE>;

}  // namespace chrono
}  // namespace px
