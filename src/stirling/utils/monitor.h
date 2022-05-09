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

#include <absl/container/flat_hash_map.h>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/mixins.h"
#include "src/stirling/bpf_tools/bcc_bpf_intf/upid.h"

namespace px {
namespace stirling {

struct SourceStatusRecord {
  int64_t timestamp_ns = 0;
  std::string source_connector;
  std::string tracepoint;
  px::statuspb::Code status;
  std::string error;
  std::string info;
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
  void AppendStatusRecord(const std::string& source_connector, const std::string& tracepoint,
                          const Status& status, const std::string& info);
  std::vector<SourceStatusRecord> ConsumeStatusRecords();

  static constexpr auto kCrashWindow = std::chrono::seconds{5};

 private:
  using timestamp_t = std::chrono::time_point<std::chrono::steady_clock>;
  absl::flat_hash_map<struct upid_t, timestamp_t> java_proc_attach_times_;

  // Records of errors in source connectors.
  std::vector<SourceStatusRecord> source_status_records_;
};

}  // namespace stirling
}  // namespace px
