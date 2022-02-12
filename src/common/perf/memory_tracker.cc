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

#include "src/common/perf/memory_tracker.h"
#include <utility>

#ifdef TCMALLOC
#include <gperftools/malloc_extension.h>
#endif

namespace px {

namespace {

MemoryStats::Measurement MeasureMemory() {
  MemoryStats::Measurement measurement;
#ifdef TCMALLOC
  auto instance = MallocExtension::instance();
  instance->GetNumericProperty("generic.current_allocated_bytes", &measurement.allocated);
  instance->GetNumericProperty("generic.total_physical_bytes", &measurement.physical);
#endif
  return measurement;
}

void MergeMax(const MemoryStats::Measurement& measurement, MemoryStats::Measurement* max) {
  if (measurement.physical > max->physical) {
    max->physical = measurement.physical;
  }
  if (measurement.allocated > max->allocated) {
    max->allocated = measurement.allocated;
  }
}

}  // namespace

void MemoryTracker::Start() {
  if (!enable_) {
    return;
  }
  stats_.start = MeasureMemory();
  done_ = false;
  poll_thread_ = std::make_unique<std::thread>([&]() { RunPolling(); });
}

MemoryStats MemoryTracker::End() {
  if (!enable_) {
    return MemoryStats{};
  }
  done_ = true;
  if (poll_thread_ != nullptr) {
    poll_thread_->join();
    poll_thread_.reset();
  }
  auto stats = stats_;
  // Reset MemoryStats so that the same MemoryTracker can be used for multiple Start()/End()
  // iterations.
  stats_ = MemoryStats{};

  stats.end = MeasureMemory();
  return stats;
}

void MemoryTracker::RunPolling() {
  while (!done_.load()) {
    auto measurement = MeasureMemory();
    MergeMax(measurement, &stats_.max);
    std::this_thread::sleep_for(sleep_period_);
  }
}

}  // namespace px
