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

// Defines basic execution environment related stuff.
#pragma once

#include <sys/resource.h>
#include <sys/time.h>

#include <chrono>
#include <optional>
#include <string>

#include "src/common/base/logging.h"

namespace px {

// A RAII-style wrapper of initializing and shutting down the process execution environment
// (such as flags and logging).
class EnvironmentGuard {
 public:
  EnvironmentGuard(int* argc, char** argv);
  ~EnvironmentGuard();
};

// A class to monitor CPU usage of the process.
// Instantiate this at the beginning of your program,
// and it will print out stats when it falls out of scope, at exit.
class ProcessStatsMonitor {
 public:
  ProcessStatsMonitor() { Reset(); }

  ~ProcessStatsMonitor() { PrintCPUTime(); }

  void Reset();

  void PrintCPUTime();

 private:
  std::chrono::time_point<std::chrono::steady_clock> start_time_;
  struct rusage start_usage_;
};

// Returns the value of the env var. Or nullopt if it's not set.
std::optional<std::string> GetEnv(const std::string& env_var);

void ChDirPixieRoot();

}  // namespace px
