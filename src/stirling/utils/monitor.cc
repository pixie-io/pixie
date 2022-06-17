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
#include "src/common/base/base.h"
#include "src/common/metrics/metrics.h"

DEFINE_bool(stirling_profiler_java_symbols, gflags::BoolFromEnv("PL_PROFILER_JAVA_SYMBOLS", true),
            "Whether to symbolize Java binaries.");

namespace px {
namespace stirling {

namespace {
constexpr char kJavaProcCrashedDuringAttach[] = "java_proc_crashed_during_attach";
}

StirlingMonitor::StirlingMonitor()
    : java_proc_crashed_during_attach_(
          BuildCounter(kJavaProcCrashedDuringAttach,
                       "Count of Java process crashes during symbolization agent attach.")) {}

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
      java_proc_crashed_during_attach_.Increment();
      FLAGS_stirling_profiler_java_symbols = false;
      LOG(WARNING) << absl::Substitute(
          "Detected Java process crash, pid: $0, within $1 seconds of symbolization agent attach. "
          "Disabling Java symbolization.",
          upid.pid, delta.count());
    }
  }
}

void StirlingMonitor::AppendSourceStatusRecord(const std::string& source_connector,
                                               const Status& status, const std::string& context) {
  absl::base_internal::SpinLockHolder lock(&source_status_lock_);
  source_status_records_.push_back(
      {CurrentTimeNS(), source_connector, status.code(), status.msg(), context});
}

void StirlingMonitor::AppendProbeStatusRecord(const std::string& source_connector,
                                              const std::string& tracepoint, const Status& status,
                                              const std::string& info) {
  absl::base_internal::SpinLockHolder lock(&probe_status_lock_);
  probe_status_records_.push_back(
      {CurrentTimeNS(), source_connector, tracepoint, status.code(), status.msg(), info});
}

std::vector<SourceStatusRecord> StirlingMonitor::ConsumeSourceStatusRecords() {
  absl::base_internal::SpinLockHolder lock(&source_status_lock_);
  return std::move(source_status_records_);
}

std::vector<ProbeStatusRecord> StirlingMonitor::ConsumeProbeStatusRecords() {
  absl::base_internal::SpinLockHolder lock(&probe_status_lock_);
  return std::move(probe_status_records_);
}

}  // namespace stirling
}  // namespace px
