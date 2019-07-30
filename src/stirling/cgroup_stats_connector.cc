#ifdef __linux__

#include <experimental/filesystem>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "src/common/base/base.h"
#include "src/common/system/proc_parser.h"
#include "src/stirling/cgroup_stats_connector.h"
#include "src/stirling/cgroups/cgroup_manager.h"

namespace pl {
namespace stirling {

using system::ProcParser;

Status CGroupStatsConnector::InitImpl() { return cgroup_mgr_->UpdateCGroupInfo(); }

Status CGroupStatsConnector::StopImpl() { return Status::OK(); }

void CGroupStatsConnector::TransferCGroupStatsTable(DataTable* data_table) {
  auto status = cgroup_mgr_->UpdateCGroupInfo();
  if (!status.ok()) {
    LOG(ERROR) << "Failed to get the latest cgroup stat files: " << status.msg();
    return;
  }

  auto now = std::chrono::steady_clock::now();
  int64_t timestamp =
      std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count() +
      ClockRealTimeOffset();

  for (const auto& [pod, pod_info] : cgroup_mgr_->cgroup_info()) {
    for (const auto& [container, process_info] : pod_info.container_info_by_name) {
      for (const auto& pid : process_info.pids) {
        ProcParser::ProcessStats stats;
        auto s = cgroup_mgr_->GetProcessStats(pid, &stats);
        if (!s.ok()) {
          LOG(ERROR) << absl::Substitute(
              "Failed to fetch info for PID ($0). Error=\"$1\" skipping.", pid, s.msg());
          continue;
        }

        RecordBuilder<&kCPUTable> r(data_table);
        r.Append<r.ColIndex("time_")>(timestamp);
        r.Append<r.ColIndex("qos")>(CGroupManager::CGroupQoSToString(pod_info.qos));
        r.Append<r.ColIndex("pod")>(std::string(pod));
        r.Append<r.ColIndex("container")>(std::string(container));
        r.Append<r.ColIndex("process_name")>(std::move(stats.process_name));
        r.Append<r.ColIndex("pid")>(stats.pid);
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
  }
}

void CGroupStatsConnector::TransferNetStatsTable(DataTable* data_table) {
  auto status = cgroup_mgr_->UpdateCGroupInfo();
  if (!status.ok()) {
    LOG(ERROR) << "Failed to get the latest cgroup stat files: " << status.msg();
    return;
  }

  auto now = std::chrono::steady_clock::now();
  int64_t timestamp =
      std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count() +
      ClockRealTimeOffset();

  for (const auto& [pod, pod_info] : cgroup_mgr_->cgroup_info()) {
    PL_UNUSED(pod_info);
    ProcParser::NetworkStats stats;
    auto s = cgroup_mgr_->GetNetworkStatsForPod(pod, &stats);
    if (!s.ok()) {
      LOG(ERROR) << absl::Substitute(
          "Failed to fetch network stats for pod \"$0\". Error=\"$1\" skipping.", pod, s.msg());
      continue;
    }

    RecordBuilder<&kNetworkTable> r(data_table);

    r.Append<r.ColIndex("time_")>(timestamp);
    r.Append<r.ColIndex("pod")>(std::string(pod));

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

void CGroupStatsConnector::TransferDataImpl(uint32_t table_num, DataTable* data_table) {
  CHECK_LT(table_num, num_tables())
      << absl::Substitute("Trying to access unexpected table: table_num=$0", table_num);

  switch (table_num) {
    case 0:
      TransferCGroupStatsTable(data_table);
      break;
    case 1:
      TransferNetStatsTable(data_table);
      break;
    default:
      LOG(ERROR) << "Unknown table: " << table_num;
  }
}

}  // namespace stirling
}  // namespace pl

#endif
