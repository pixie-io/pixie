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

#include "src/stirling/utils/monitor.h"

DEFINE_bool(stirling_profiler_java_symbols, gflags::BoolFromEnv("PL_PROFILER_JAVA_SYMBOLS", false),
            "Whether to symbolize Java binaries.");

namespace px {
namespace stirling {

void StirlingMonitor::ResetJavaProcessAttachTrackers() { java_proc_attach_times_.clear(); }

void StirlingMonitor::NotifyJavaProcessAttach(const struct upid_t& upid) {
  DCHECK(java_proc_attach_times_.find(upid) == java_proc_attach_times_.end());
  java_proc_attach_times_[upid] = std::chrono::steady_clock::now();
}

void StirlingMonitor::NotifyJavaProcessCrashed(const struct upid_t& upid) {
  const auto iter = java_proc_attach_times_.find(upid);
  if (iter != java_proc_attach_times_.end()) {
    const auto& t_attach = iter->second;
    const auto t_now = std::chrono::steady_clock::now();
    const auto delta = std::chrono::duration_cast<std::chrono::seconds>(t_now - t_attach);
    if (delta < kCrashWindow) {
      FLAGS_stirling_profiler_java_symbols = false;
      LOG(WARNING) << absl::Substitute(
          "Detected Java process crash, pid: $0, within $1 seconds of symbolization agent attach. "
          "Disabling Java symbolization.",
          upid.pid, delta.count());
    }
  }
}

}  // namespace stirling
}  // namespace px
