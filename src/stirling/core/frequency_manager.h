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

#include "src/common/system/clock.h"

namespace px {
namespace stirling {

/**
 * Manages the frequency of periodical action.
 */
class FrequencyManager {
  using time_point = std::chrono::steady_clock::time_point;

 public:
  /**
   * Returns true if the current cycle has expired.
   */
  bool Expired(const time_point now) const { return now >= next_; }

  /**
   * Ends the current cycle, and starts the next one.
   */
  void Reset(const time_point now);

  void set_period(std::chrono::milliseconds period) { period_ = period; }
  const auto& period() const { return period_; }
  const auto& next() const { return next_; }
  uint32_t count() const { return count_; }

 private:
  // The cycle's period.
  std::chrono::milliseconds period_ = {};

  // When the current cycle should end.
  std::chrono::steady_clock::time_point next_ = {};

  // The count of expired cycle so far.
  uint32_t count_ = 0;
};

}  // namespace stirling
}  // namespace px
