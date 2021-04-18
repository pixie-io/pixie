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
#include <iostream>

#include "src/common/base/base.h"

namespace px {

/**
 * Timing class.
 *
 * Allows for measurement with high resolution timer with clocks that can be started and stopped.
 */
class ElapsedTimer : public NotCopyable {
 public:
  /**
   * Start the timer.
   */
  void Start() {
    Reset();
    Resume();
  }

  /**
   * Stop the timer.
   */
  void Stop() {
    DCHECK(timer_running_) << "Stop called when timer is not running";
    timer_running_ = false;
    elapsed_time_us_ += TimeDiff();
  }

  /**
   * Resume the timer.
   */
  void Resume() {
    DCHECK(!timer_running_) << "Timer already running";
    timer_running_ = true;
    start_time_ = std::chrono::high_resolution_clock::now();
  }

  /**
   * Reset the timer.
   */
  void Reset() {
    timer_running_ = false;
    elapsed_time_us_ = 0;
  }

  /**
   * @return the elapsed time in us.
   */
  uint64_t ElapsedTime_us() const { return elapsed_time_us_ + (timer_running_ ? TimeDiff() : 0); }

 private:
  uint64_t TimeDiff() const {
    auto current = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(current - start_time_).count();
  }
  bool timer_running_ = false;
  std::chrono::high_resolution_clock::time_point start_time_;
  uint64_t elapsed_time_us_ = 0;
};

}  // namespace px
