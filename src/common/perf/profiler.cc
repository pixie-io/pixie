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

#include "src/common/perf/profiler.h"

#include <string>

#ifdef PROFILER_AVAILABLE

#include "gperftools/heap-profiler.h"
#include "gperftools/profiler.h"

namespace px {
namespace profiler {

bool CPU::ProfilerAvailable() { return ProfilingIsEnabledForAllThreads() == 0; }

bool CPU::StartProfiler(const std::string& output_path) {
  return ProfilerStart(output_path.c_str()) != 0;
}

void CPU::StopProfiler() { return ProfilerStop(); }

bool Heap::ProfilerAvailable() { return true; }

bool Heap::IsProfilerStarted() { return IsHeapProfilerRunning() != 0; }

bool Heap::StartProfiler(const std::string& output_path) {
  HeapProfilerStart(output_path.c_str());
  return true;
}

bool Heap::Dump() {
  if (IsHeapProfilerRunning() == 0) {
    return false;
  }
  HeapProfilerDump("dump");
  return true;
}

bool Heap::StopProfiler() {
  if (IsHeapProfilerRunning() == 0) {
    return false;
  }
  HeapProfilerDump("stop and dump");
  HeapProfilerStop();
  return true;
}

void Heap::ForceLink() {
  // Currently this is here to force the inclusion of the heap profiler during static linking.
  // Without this call the heap profiler will not be included and cannot be started via env
  // variable.
  HeapProfilerDump("");
}

}  // namespace profiler
}  // namespace px

#else  // !PROFILER_AVAILABLE

namespace px {
namespace profiler {

bool CPU::ProfilerAvailable() { return false; }
bool CPU::StartProfiler(const std::string& /*output_path*/) { return false; }
void CPU::StopProfiler() {}

bool Heap::ProfilerAvailable() { return false; }
bool Heap::IsProfilerStarted() { return false; }
bool Heap::StartProfiler(const std::string& /*output_path*/) { return false; }
bool Heap::StopProfiler() { return false; }

}  // namespace profiler
}  // namespace px

#endif  // PROFILER_AVAILABLE
