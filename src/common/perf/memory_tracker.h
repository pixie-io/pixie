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
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

namespace px {

struct MemoryStats {
  struct Measurement {
    // Bytes that are in use by the tracked program.
    uint64_t allocated = 0;
    // Total bytes in physical memory. This includes allocated above, as well as extra bytes that
    // tcmalloc hasn't yet released to the system.
    uint64_t physical = 0;
  };
  Measurement start;
  Measurement max;
  Measurement end;
};

// MemoryTracker keeps track of memory usage by continually polling the tcmalloc API to get memory
// statistics. It takes one measurement at the start, one at the end and then polls to determine the
// max measurement between the two. Note that this tracker will return measurements of 0 bytes, if
// the allocator is not tcmalloc.
class MemoryTracker {
  static constexpr std::chrono::milliseconds kDefaultSleepPeriod = std::chrono::milliseconds{1};

 public:
  explicit MemoryTracker(bool enable, std::chrono::milliseconds sleep_period = kDefaultSleepPeriod)
      : enable_(enable), sleep_period_(sleep_period) {}

  // Start tracking memory usage. A memory measurement will be taken for the `start` measurement.
  // Then a thread will be launched to poll the tcmalloc API to update the max measurement.
  void Start();
  // Stop tracking memory usage. Return a struct with start, max and end measurements.
  MemoryStats End();

 private:
  void RunPolling();

  bool enable_;
  std::atomic<bool> done_ = false;
  std::unique_ptr<std::thread> poll_thread_ = nullptr;
  std::chrono::milliseconds sleep_period_;

  MemoryStats stats_;
};

}  // namespace px
