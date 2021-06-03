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

namespace px {
namespace stirling {

bool SamplePushFrequencyManager::SamplingRequired() const {
  return px::chrono::coarse_steady_clock::now() > NextSamplingTime();
}

void SamplePushFrequencyManager::Sample() {
  last_sampled_ = px::chrono::coarse_steady_clock::now();
  ++sampling_count_;
}

bool SamplePushFrequencyManager::PushRequired(double occupancy_percentage,
                                              uint32_t occupancy) const {
  // Note: It's okay to exercise an early Push, by returning true before the final return,
  // but it is not okay to 'return false' in this function.

  if (static_cast<uint32_t>(100 * occupancy_percentage) > occupancy_pct_threshold_) {
    return true;
  }

  if (occupancy > occupancy_threshold_) {
    return true;
  }

  return px::chrono::coarse_steady_clock::now() > NextPushTime();
}

void SamplePushFrequencyManager::Push() {
  last_pushed_ = px::chrono::coarse_steady_clock::now();
  ++push_count_;
}

px::chrono::coarse_steady_clock::time_point SamplePushFrequencyManager::NextSamplingTime() const {
  return last_sampled_ + sampling_period_;
}

px::chrono::coarse_steady_clock::time_point SamplePushFrequencyManager::NextPushTime() const {
  return last_pushed_ + push_period_;
}

}  // namespace stirling
}  // namespace px
