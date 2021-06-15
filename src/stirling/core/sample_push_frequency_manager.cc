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

#include "src/stirling/core/sample_push_frequency_manager.h"

namespace px {
namespace stirling {

namespace {

// Data push threshold, based on percentage of buffer that is filled.
constexpr uint32_t kDefaultOccupancyPctThreshold = 100;

// Data push threshold, based number of records after which a push.
constexpr uint32_t kDefaultOccupancyThreshold = 1024;

}  // namespace

bool SamplePushFrequencyManager::PushRequired(double occupancy_percentage,
                                              uint32_t occupancy) const {
  if (push_freq_mgr_.Expired()) {
    return true;
  }
  if (static_cast<uint32_t>(100 * occupancy_percentage) > kDefaultOccupancyPctThreshold) {
    return true;
  }
  if (occupancy > kDefaultOccupancyThreshold) {
    return true;
  }
  return false;
}

void FrequencyManager::Reset() {
  next_ = px::chrono::coarse_steady_clock::now() + period_;
  ++count_;
}

}  // namespace stirling
}  // namespace px
