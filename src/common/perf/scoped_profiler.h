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

#include "src/common/base/base.h"

namespace px {

/**
 * Scoped profiler is used to profile a block of code.
 * Usage:
 *    ScopedProfiler<profiler::CPU> profile(true, "output");
 * @tparam T The type of profiler.
 */
template <class T>
class ScopedProfiler {
 public:
  /**
   * Constructor for scoped profiler.
   * @param enabled true if profiling is enabled
   * @param output_path The output path of the dump.
   */
  ScopedProfiler(bool enabled, std::string output_path) : enabled_(enabled) {
    if (!enabled_) {
      return;
    }

    if (!T::ProfilerAvailable()) {
      LOG(ERROR) << "Profiler enabled, but not available";
      enabled_ = false;
      return;
    }

    CHECK(T::StartProfiler(output_path)) << "Failed to start profiler";
  }

  ~ScopedProfiler() {
    if (!enabled_) {
      return;
    }

    T::StopProfiler();
  }

 private:
  bool enabled_;
};

}  // namespace px
