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

#include <unistd.h>

#include <absl/synchronization/mutex.h>
#include <atomic>
#include <memory>
#include <thread>

#include "src/common/clock/interpolating_lookup_table.h"

namespace px {
namespace clock {

/**
 * ClockConverter is the interface for conversion utilities that convert monotonic time to adjusted
 * real time.
 */
class ClockConverter {
 public:
  virtual ~ClockConverter() = default;
  virtual uint64_t Convert(uint64_t monotonic_time) const = 0;
  virtual void Update() = 0;
  virtual std::chrono::milliseconds UpdatePeriod() const = 0;
  // The max history is chosen as an even multiple of the default polling period, and longer than 5
  // minutes. This was chosen based on a conservative estimate that Stirling will process all
  // incoming data within 5 minutes of it being given a timestamp by BPF. With the current polling
  // period we only have to store 80 bytes of data to achieve this window size, so this number could
  // easily be increased if need be.
  static constexpr std::chrono::seconds kMaxHistory = std::chrono::seconds{320};
  static constexpr size_t BufferCapacity(std::chrono::seconds update_period) {
    return kMaxHistory / update_period;
  }
};

/**
 * DefaultMonoToRealtimeConverter keeps a mapping of monotonic to realtime. It's up to the caller to
 * call DefaultMonoToRealtimeConverter::Update(), every
 * DefaultMonoToRealtimeConverter::UpdatePeriod() milliseconds in order to keep the mapping up to
 * date.
 */
class DefaultMonoToRealtimeConverter : public ClockConverter {
 public:
  // The update period was chosen as half the default NTP update interval. At twice the frequency
  // of NTP updates, we should most of the time be able to linearly interpolate between polls
  // without missing too many NTP offets.
  static constexpr std::chrono::seconds kUpdatePeriod = std::chrono::seconds{32};

  static constexpr uint64_t kSecToNanosecFactor = 1000 * 1000 * 1000;

  DefaultMonoToRealtimeConverter();

  uint64_t Convert(uint64_t monotonic_time) const override;
  void Update() override;
  std::chrono::milliseconds UpdatePeriod() const override { return kUpdatePeriod; }

 private:
  InterpolatingLookupTable<ClockConverter::BufferCapacity(kUpdatePeriod)> mono_to_realtime_;
};

}  // namespace clock
}  // namespace px
