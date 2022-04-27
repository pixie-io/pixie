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

#include <prometheus/counter.h>

#include <absl/container/flat_hash_map.h>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/mixins.h"
#include "src/stirling/upid/upid.h"

namespace px {
namespace stirling {

// Status of Stirling Source Connectors.
struct SourceStatusRecord {
  int64_t timestamp_ns = 0;
  std::string source_connector = "";
  px::statuspb::Code status;
  std::string error = "";
  std::string context = "";
};

// Status of ebpf probes.
struct ProbeStatusRecord {
  int64_t timestamp_ns = 0;
  std::string source_connector;
  std::string tracepoint = "";
  px::statuspb::Code status;
  std::string error = "";
  std::string info = "";
};

class StirlingMonitor : NotCopyMoveable {
 public:
  static StirlingMonitor* GetInstance() {
    static StirlingMonitor singleton;
    return &singleton;
  }

  // Java Process.
  void NotifyJavaProcessAttach(const struct upid_t& upid);
  void NotifyJavaProcessCrashed(const struct upid_t& upid);
  void ResetJavaProcessAttachTrackers();

  // Stirling Error Reporting.
  void AppendProbeStatusRecord(const std::string& source_connector, const std::string& tracepoint,
                               const Status& status, const std::string& info);
  void AppendSourceStatusRecord(const std::string& source_connector, const Status& status,
                                const std::string& context);
  std::vector<ProbeStatusRecord> ConsumeProbeStatusRecords();
  std::vector<SourceStatusRecord> ConsumeSourceStatusRecords();

  static constexpr auto kCrashWindow = std::chrono::seconds{5};

 private:
  StirlingMonitor();
  using timestamp_t = std::chrono::time_point<std::chrono::steady_clock>;
  absl::flat_hash_map<struct upid_t, timestamp_t> java_proc_attach_times_;

  // Records of probe deployment status.
  std::vector<ProbeStatusRecord> probe_status_records_ ABSL_GUARDED_BY(probe_status_lock_);
  // Records of Stirling Source Connector status.
  std::vector<SourceStatusRecord> source_status_records_ ABSL_GUARDED_BY(source_status_lock_);

  // Lock to protect probe and source records.
  absl::base_internal::SpinLock probe_status_lock_;
  absl::base_internal::SpinLock source_status_lock_;

  prometheus::Counter& java_proc_crashed_during_attach_;
};

}  // namespace stirling
}  // namespace px
