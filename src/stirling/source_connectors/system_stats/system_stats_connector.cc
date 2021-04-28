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

#ifdef __linux__
#include "src/stirling/source_connectors/system_stats/system_stats_connector.h"

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

Status SystemStatsConnector::InitImpl() {
  sample_push_freq_mgr_.set_sampling_period(kSamplingPeriod);
  sample_push_freq_mgr_.set_push_period(kPushPeriod);
  return Status::OK();
}

Status SystemStatsConnector::StopImpl() { return Status::OK(); }

void SystemStatsConnector::TransferProcessStatsTable(ConnectorContext* ctx, DataTable* data_table) {
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
    auto s1 = proc_parser_->ParseProcPIDStat(pid, &stats);
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

void SystemStatsConnector::TransferNetworkStatsTable(ConnectorContext* ctx, DataTable* data_table) {
  const md::K8sMetadataState& k8s_md = ctx->GetK8SMetadata();

  int64_t timestamp = AdjustedSteadyClockNowNS();

  for (const auto& [pod_name, pod_id] : k8s_md.pods_by_name()) {
    PL_UNUSED(pod_name);

    auto* pod_info = k8s_md.PodInfoByID(pod_id);
    // TODO(zasgar): Fix condition for dead pods after helper function is added.
    if (pod_info == nullptr || pod_info->stop_time_ns() > 0) {
      continue;
    }

    ProcParser::NetworkStats stats;
    auto s = GetNetworkStatsForPod(*proc_parser_, *pod_info, k8s_md, &stats);

    if (!s.ok()) {
      VLOG(1) << absl::StrCat("Failed to get Pod network stats: ", s.msg());
      continue;
    }

    DataTable::RecordBuilder<&kNetworkStatsTable> r(data_table, timestamp);

    r.Append<r.ColIndex("time_")>(timestamp);
    r.Append<r.ColIndex("pod_id")>(std::string(pod_id));
    r.Append<r.ColIndex("rx_bytes")>(stats.rx_bytes);
    r.Append<r.ColIndex("rx_packets")>(stats.rx_packets);
    r.Append<r.ColIndex("rx_errors")>(stats.rx_errs);
    r.Append<r.ColIndex("rx_drops")>(stats.rx_drops);
    r.Append<r.ColIndex("tx_bytes")>(stats.tx_bytes);
    r.Append<r.ColIndex("tx_packets")>(stats.tx_packets);
    r.Append<r.ColIndex("tx_errors")>(stats.tx_errs);
    r.Append<r.ColIndex("tx_drops")>(stats.tx_drops);
  }
}

void SystemStatsConnector::TransferDataImpl(ConnectorContext* ctx, uint32_t table_num,
                                            DataTable* data_table) {
  DCHECK_LT(table_num, num_tables())
      << absl::Substitute("Trying to access unexpected table: table_num=$0", table_num);

  switch (table_num) {
    case 0:
      TransferProcessStatsTable(ctx, data_table);
      break;
    case 1:
      TransferNetworkStatsTable(ctx, data_table);
      break;
    default:
      LOG(ERROR) << "Unknown table: " << table_num;
  }
}

void SystemStatsConnector::TransferDataImpl(ConnectorContext* ctx,
                                            const std::vector<DataTable*>& data_tables) {
  DCHECK_EQ(data_tables.size(), 2);

  if (data_tables[kProcStatsTableNum] != nullptr) {
    TransferProcessStatsTable(ctx, data_tables[kProcStatsTableNum]);
  }

  if (data_tables[kNetStatsTableNum] != nullptr) {
    TransferNetworkStatsTable(ctx, data_tables[kNetStatsTableNum]);
  }
}

Status SystemStatsConnector::GetNetworkStatsForPod(const system::ProcParser& proc_parser,
                                                   const md::PodInfo& pod_info,
                                                   const md::K8sMetadataState& k8s_metadata_state,
                                                   system::ProcParser::NetworkStats* stats) {
  DCHECK(stats != nullptr);
  // Since all the containers running in a K8s pod use the same network
  // namespace, we only need to pull stats from a single PID. The stats
  // themselves are the same for each PID since Linux only tracks networks
  // stats at a namespace level.
  //
  // In case the read fails we try another file. This should not normally
  // be required, but will make the code more robust to cases where the PID
  // is killed between when we update the pid list but before the network
  // data is requested.
  for (const auto& container_id : pod_info.containers()) {
    auto* container_info = k8s_metadata_state.ContainerInfoByID(container_id);
    // TODO(zasgar): Fix condition for dead pods after helper function is added.
    if (container_info == nullptr || container_info->stop_time_ns() > 0) {
      // Container has died or does not exist.
      continue;
    }

    for (const auto& upid : container_info->active_upids()) {
      auto s = proc_parser.ParseProcPIDNetDev(upid.pid(), stats);
      if (s.ok()) {
        // Since we just need to read one pid, we can bail on the first successful read.
        return s;
      }
      VLOG(1) << absl::Substitute("Failed to read network stats for pod=$0, using upid=$1",
                                  upid.String(), pod_info.uid());
    }
  }

  return error::Internal("Failed to get networks stats for pod_id=$0", pod_info.uid());
}

}  // namespace stirling
}  // namespace px

#endif
