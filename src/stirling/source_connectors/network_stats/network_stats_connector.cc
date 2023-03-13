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

#include "src/stirling/source_connectors/network_stats/network_stats_connector.h"

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

Status NetworkStatsConnector::InitImpl() {
  sampling_freq_mgr_.set_period(kSamplingPeriod);
  push_freq_mgr_.set_period(kPushPeriod);
  return Status::OK();
}

Status NetworkStatsConnector::StopImpl() { return Status::OK(); }

void NetworkStatsConnector::TransferDataImpl(ConnectorContext* ctx) {
  DCHECK_EQ(data_tables_.size(), 1U) << "NetworkStatsConnector only has one data table.";

  if (data_tables_[kNetStatsTableNum] != nullptr) {
    TransferNetworkStatsTable(ctx, data_tables_[kNetStatsTableNum]);
  }
}

void NetworkStatsConnector::TransferNetworkStatsTable(ConnectorContext* ctx,
                                                      DataTable* data_table) {
  const md::K8sMetadataState& k8s_md = ctx->GetK8SMetadata();

  int64_t timestamp = AdjustedSteadyClockNowNS();

  for (const auto& [pod_name, pod_id] : k8s_md.pods_by_name()) {
    PX_UNUSED(pod_name);

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

Status NetworkStatsConnector::GetNetworkStatsForPod(const system::ProcParser& proc_parser,
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

    // We expect the UPIDs to be start-time ordered.
    // This is so we prioritize picking the oldest UPID to get the network
    // stats. Normally, any PID will do, since they are all in the same network namespace. However,
    // there can be issues if any of the processes switch namespaces. This happens with Stirling's
    // own Java Attacher, which can cause network stats to suddenly switch to the stats of a
    // different namespace, producing incorrect results.
    CHECK(std::is_sorted(container_info->active_upids().begin(),
                         container_info->active_upids().end(), md::UPIDStartTSCompare()));

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
