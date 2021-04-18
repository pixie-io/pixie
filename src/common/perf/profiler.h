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
// Most of this file is borrowed/inspired by envoy.
// Profiling support is provided in the release tcmalloc, but not in the library
// that supplies the debug tcmalloc. So all the profiling code must be ifdef'd
// on PROFILER_AVAILABLE which is dependent on those two settings.
#if defined(TCMALLOC) && !defined(PL_MEMORY_DEBUG_ENABLED)
#define PROFILER_AVAILABLE
#endif

namespace px {
namespace profiler {

class CPU {
 public:
  /**
   * Returns if the profiler is available or not.
   * @return bool
   */
  static bool ProfilerAvailable();

  /*
   * Start profiler and store in the passed in path.
   * @return bool whether the call to start the profiler succeeded.
   */
  static bool StartProfiler(const std::string& output_path);

  /**
   * Stop the profiler.
   */
  static void StopProfiler();
};

/**
 * A memory utilization profiler.
 * Can be configured with enviornment variables to dump heap profiles at different times.
 * See: https://gperftools.github.io/gperftools/heapprofile.html
 */
class Heap {
 public:
  /**
   * Returns if the heap profiler is available or not.
   * @return bool
   */
  static bool ProfilerAvailable();

  /**
   * Check to see if profiler has been started.
   * @return bool true if profiler is started.
   */
  static bool IsProfilerStarted();

  /**
   * Start profiler and store in the passed in path.
   * @return bool whether the call to start the profiler succeeded.
   */
  static bool StartProfiler(const std::string& output_path);

  /**
   * Immediately dump the profiler results.
   * @return bool whether the call to dump succeeded.
   */
  static bool Dump();

  /**
   * Dump the results and stop the profiler.
   * @return bool if the profiler was stopped and file written.
   */
  static bool StopProfiler();

 private:
  static void ForceLink();
};

}  // namespace profiler
}  // namespace px
