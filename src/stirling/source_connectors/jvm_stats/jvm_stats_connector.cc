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

#include "src/stirling/source_connectors/jvm_stats/jvm_stats_connector.h"

#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/base/byte_utils.h"
#include "src/stirling/source_connectors/jvm_stats/jvm_stats_table.h"
#include "src/stirling/source_connectors/jvm_stats/utils/java.h"
#include "src/stirling/utils/detect_application.h"
#include "src/stirling/utils/proc_tracker.h"

DEFINE_int32(
    stirling_java_process_monitoring_attempts, 3,
    "The number of attempts to monitor a potential Java process for collecting JVM stats.");

namespace px {
namespace stirling {

Status JVMStatsConnector::InitImpl() {
  sampling_freq_mgr_.set_period(kSamplingPeriod);
  push_freq_mgr_.set_period(kPushPeriod);
  return Status::OK();
}

void JVMStatsConnector::FindJavaUPIDs(const ConnectorContext& ctx) {
  proc_tracker_.Update(ctx.GetUPIDs());
  const auto& upid_pidinfo_map = ctx.GetPIDInfoMap();

  for (const auto& upid : proc_tracker_.new_upids()) {
    // The host PID 1 is not a Java app. However, when later invoking HsperfdataPath(), it could be
    // confused to conclude that there is a hsperfdata file for PID 1, because of the limitations
    // of ResolveMountPoint().
    const uint32_t pid = upid.pid();

    if (pid == 1) {
      continue;
    }

    auto iter = upid_pidinfo_map.find(upid);
    if (iter == upid_pidinfo_map.end() || iter->second == nullptr) {
      continue;
    }
    const std::filesystem::path proc_exe = iter->second->exe_path();
    if (DetectApplication(proc_exe) != Application::kJava) {
      continue;
    }
    PX_ASSIGN_OR(auto hsperf_data_path, java::HsperfdataPath(pid), continue);
    java_procs_[upid].hsperf_data_path = hsperf_data_path;
  }
}

Status JVMStatsConnector::ExportStats(const md::UPID& upid,
                                      const std::filesystem::path& hsperf_data_path,
                                      DataTable* data_table) const {
  PX_ASSIGN_OR_RETURN(std::string hsperf_data_str, ReadFileToString(hsperf_data_path));

  if (hsperf_data_str.empty()) {
    // Assume this is a transient failure.
    return Status::OK();
  }

  java::Stats stats(std::move(hsperf_data_str));
  if (!stats.Parse().ok()) {
    // Assumes this is a transient failure.
    return Status::OK();
  }

  uint64_t time = AdjustedSteadyClockNowNS();

  DataTable::RecordBuilder<&kJVMStatsTable> r(data_table, time);
  r.Append<kTimeIdx>(time);
  r.Append<kUPIDIdx>(upid.value());
  r.Append<kYoungGCTimeIdx>(stats.YoungGCTimeNanos());
  r.Append<kFullGCTimeIdx>(stats.FullGCTimeNanos());
  r.Append<kUsedHeapSizeIdx>(stats.UsedHeapSizeBytes());
  r.Append<kTotalHeapSizeIdx>(stats.TotalHeapSizeBytes());
  r.Append<kMaxHeapSizeIdx>(stats.MaxHeapSizeBytes());
  return Status::OK();
}

void JVMStatsConnector::TransferDataImpl(ConnectorContext* ctx) {
  DCHECK_EQ(data_tables_.size(), 1U) << "JVMStats only has one data table.";

  DataTable* data_table = data_tables_[0];

  if (data_table == nullptr) {
    return;
  }

  FindJavaUPIDs(*ctx);

  for (auto iter = java_procs_.begin(); iter != java_procs_.end();) {
    const md::UPID& upid = iter->first;
    JavaProcInfo& java_proc = iter->second;

    md::UPID upid_with_asid(ctx->GetASID(), upid.pid(), upid.start_ts());
    auto status = ExportStats(upid_with_asid, java_proc.hsperf_data_path, data_table);
    if (!status.ok()) {
      ++java_proc.export_failure_count;
    }
    if (java_proc.export_failure_count >= FLAGS_stirling_java_process_monitoring_attempts) {
      java_procs_.erase(iter++);
    } else {
      ++iter;
    }
  }
}

}  // namespace stirling
}  // namespace px
