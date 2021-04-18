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

#include <string>
#include <utility>

#include <absl/strings/str_format.h>
#include "src/common/base/base.h"
#include "src/common/perf/elapsed_timer.h"

namespace px {

/**
 * Times a particular function scope and prints the time to the log.
 * @tparam TTimer Can be any class that implements Start and ElapsedTime_us().
 */
template <class TTimer = ElapsedTimer>
class ScopedTimer : public NotCopyable {
 public:
  /**
   * Creates a scoped timer with the given name.
   * @param name
   */
  explicit ScopedTimer(std::string name) : name_(std::move(name)) { timer_.Start(); }

  /**
   * Writes to the log the elapsed time.
   */
  ~ScopedTimer() {
    double elapsed = timer_.ElapsedTime_us();
    LOG(INFO) << absl::StrFormat("Timer(%s) : %s", name_, PrettyDuration(1000 * elapsed));
  }

 private:
  TTimer timer_;
  std::string name_;
};

}  // namespace px
