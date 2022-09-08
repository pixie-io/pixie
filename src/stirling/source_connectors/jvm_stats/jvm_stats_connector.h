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

#include <map>
#include <memory>
#include <string_view>
#include <vector>

#include <absl/container/flat_hash_set.h>

#include "src/common/base/base.h"
#include "src/common/system/system.h"
#include "src/shared/upid/upid.h"
#include "src/stirling/core/source_connector.h"
#include "src/stirling/source_connectors/jvm_stats/jvm_stats_table.h"
#include "src/stirling/source_connectors/jvm_stats/utils/java.h"
#include "src/stirling/utils/proc_tracker.h"

namespace px {
namespace stirling {

// Reads and parses the hsperfdata file created by JVM, and exports them into a data table.
//
// Hsperfdata is a JVM feature that exports JVM performance stats into a memory-mapped file under
// /tmp directory. It's supported by almost all JVM from major vendors.
class JVMStatsConnector : public SourceConnector {
 public:
  static constexpr std::string_view kName = "jvm_stats";
  static constexpr auto kTables = MakeArray(kJVMStatsTable);
  static constexpr int kTableNum = SourceConnector::TableNum(kTables, kJVMStatsTable);
  static constexpr auto kSamplingPeriod = std::chrono::milliseconds{1000};
  static constexpr auto kPushPeriod = std::chrono::milliseconds{1000};

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new JVMStatsConnector(name));
  }

  Status InitImpl() override;
  Status StopImpl() override { return Status::OK(); }

  void TransferDataImpl(ConnectorContext* ctx) override;

 private:
  explicit JVMStatsConnector(std::string_view source_name)
      : SourceConnector(source_name, kTables) {}

  // Finds the UPIDs of newly-created processes as monitoring targets.
  void FindJavaUPIDs(const ConnectorContext& ctx);

  // Exports JVM performance metrics to data table.
  Status ExportStats(const md::UPID& upid, const std::filesystem::path& hsperf_data_path,
                     DataTable* data_table) const;

  // Keeps track of the currently-running processes. Used to find the newly-created processes.
  ProcTracker proc_tracker_;

  // Records the PIDs of previously scanned Java processes, and their hsperfdata file path.
  struct JavaProcInfo {
    // How many times we have failed to export stats for this process. Once this reaches a limit,
    // the process will no longer be monitored.
    int export_failure_count = 0;
    std::filesystem::path hsperf_data_path;
  };
  absl::flat_hash_map<md::UPID, JavaProcInfo> java_procs_;
};

}  // namespace stirling
}  // namespace px
