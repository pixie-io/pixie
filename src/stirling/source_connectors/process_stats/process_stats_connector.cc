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

#include "src/stirling/source_connectors/process_stats/process_stats_connector.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

#include "src/common/base/base.h"
#include "src/common/system/proc_parser.h"
#include "src/shared/metadata/metadata.h"

namespace px {
namespace stirling {

using system::ProcParser;

Status ProcessStatsConnector::InitImpl() {
  sampling_freq_mgr_.set_period(kSamplingPeriod);
  push_freq_mgr_.set_period(kPushPeriod);
  return Status::OK();
}

Status ProcessStatsConnector::StopImpl() { return Status::OK(); }

void ProcessStatsConnector::TransferProcessStatsTable(ConnectorContext* ctx,
                                                      DataTable* data_table) {
  const absl::flat_hash_map<md::UPID, md::PIDInfoUPtr>& pid_info_by_upid = ctx->GetPIDInfoMap();

  int64_t timestamp = AdjustedSteadyClockNowNS();

  for (const auto& [upid, pid_info] : pid_info_by_upid) {
    // TODO(zasgar): Fix condition for dead pids after helper function is added.
    if (pid_info == nullptr || pid_info->stop_time_ns() > 0) {
      // PID has been stopped.
      continue;
    }

    ProcParser::ProcessStats stats;
    int32_t pid = upid.pid();
    // TODO(zasgar): We should double check the process start time to make sure it still the same
    // PID.
    auto s1 =
        proc_parser_->ParseProcPIDStat(pid, system::Config::GetInstance().PageSizeBytes(),
                                       system::Config::GetInstance().KernelTickTimeNS(), &stats);
    if (!s1.ok()) {
      VLOG(1) << absl::Substitute(
          "Failed to fetch cpu stat info for PID ($0). Error=\"$1\" skipping.", pid, s1.msg());
      continue;
    }

    auto s2 = proc_parser_->ParseProcPIDStatIO(pid, &stats);
    if (!s2.ok()) {
      VLOG(1) << absl::Substitute(
          "Failed to fetch IO stat info for PID ($0). Error=\"$1\" skipping.", pid, s2.msg());
      continue;
    }

    DataTable::RecordBuilder<&kProcessStatsTable> r(data_table, timestamp);
    // TODO(oazizi): Enable version below, once rest of the agent supports tabletization.
    //  DataTable::RecordBuilder<&kProcessStatsTable> r(data_table, upid.value(), timestamp);
    r.Append<r.ColIndex("time_")>(timestamp);
    // Tabletization key must also be appended as a column value.
    // See note in RecordBuilder class.
    r.Append<r.ColIndex("upid")>(upid.value());
    r.Append<r.ColIndex("major_faults")>(stats.major_faults);
    r.Append<r.ColIndex("minor_faults")>(stats.minor_faults);
    r.Append<r.ColIndex("cpu_utime_ns")>(stats.utime_ns);
    r.Append<r.ColIndex("cpu_ktime_ns")>(stats.ktime_ns);
    r.Append<r.ColIndex("num_threads")>(stats.num_threads);
    r.Append<r.ColIndex("vsize_bytes")>(stats.vsize_bytes);
    r.Append<r.ColIndex("rss_bytes")>(stats.rss_bytes);
    r.Append<r.ColIndex("rchar_bytes")>(stats.rchar_bytes);
    r.Append<r.ColIndex("wchar_bytes")>(stats.wchar_bytes);
    r.Append<r.ColIndex("read_bytes")>(stats.read_bytes);
    r.Append<r.ColIndex("write_bytes")>(stats.write_bytes);
  }
}

void ProcessStatsConnector::TransferDataImpl(ConnectorContext* ctx) {
  DCHECK_EQ(data_tables_.size(), 1U);

  auto* data_table = data_tables_[0];

  if (data_table == nullptr) {
    return;
  }

  TransferProcessStatsTable(ctx, data_table);
}

}  // namespace stirling
}  // namespace px
