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

#include "src/common/clock/clock_conversion.h"

namespace px {
namespace clock {

// Utility function to convert time as recorded by in monotonic clock (aka steady_clock)
// to real time (aka system_clock).
void DefaultMonoToRealtimeConverter::Update() {
  struct timespec time, real_time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  clock_gettime(CLOCK_REALTIME, &real_time);

  uint64_t mono_time_val = kSecToNanosecFactor * time.tv_sec + time.tv_nsec;
  uint64_t real_time_val = kSecToNanosecFactor * real_time.tv_sec + real_time.tv_nsec;
  mono_to_realtime_.Emplace(mono_time_val, real_time_val);
}

DefaultMonoToRealtimeConverter::DefaultMonoToRealtimeConverter() {
  // We poll once on construction. Its up to the caller to call the Poll function periodically.
  Update();
}

uint64_t DefaultMonoToRealtimeConverter::Convert(uint64_t monotonic_time) const {
  return mono_to_realtime_.Get(monotonic_time);
}

}  // namespace clock
}  // namespace px
