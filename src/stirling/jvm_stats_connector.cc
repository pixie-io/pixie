#ifdef __linux__

#include "src/stirling/jvm_stats_connector.h"

#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/base/byte_utils.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/proc_parser.h"
#include "src/stirling/jvm_stats_table.h"
#include "src/stirling/obj_tools/proc_path_tools.h"
#include "src/stirling/utils/hsperfdata.h"
#include "src/stirling/utils/java.h"

namespace pl {
namespace stirling {

using ::pl::fs::Exists;
using ::pl::stirling::obj_tools::ResolveProcessPath;
using ::pl::system::ListProcPidPaths;
using ::pl::utils::LEndianBytesToInt;

absl::flat_hash_set<md::UPID> JVMStatsConnector::FindNewUPIDs(const ConnectorContext& ctx) {
  absl::flat_hash_set<md::UPID> new_upids;

  std::filesystem::path proc_path = system::Config::GetInstance().proc_path();
  const uint32_t asid = ctx.GetASID();

  absl::flat_hash_set<md::UPID> upids = ctx.GetMdsUpids();
  if (upids.empty()) {
    // Fall back to scanning proc filesystem.
    for (const auto& [pid, pid_path] : system::ListProcPidPaths(proc_path)) {
      upids.emplace(asid, pid, system::GetPIDStartTimeTicks(pid_path));
    }
  }
  for (const auto& upid : upids) {
    if (prev_scanned_upids_.contains(upid)) {
      continue;
    }
    new_upids.insert(upid);
    prev_scanned_upids_.insert(upid);
  }
  return new_upids;
}

namespace {

StatusOr<std::string> ReadHsperfData(pid_t pid) {
  PL_ASSIGN_OR_RETURN(const std::filesystem::path hsperf_data_path, HsperfdataPath(pid));

  std::filesystem::path proc_pid_path =
      std::filesystem::path(system::Config::GetInstance().proc_path()) / std::to_string(pid);
  PL_ASSIGN_OR_RETURN(const std::filesystem::path hsperf_data_container_path,
                      ResolveProcessPath(proc_pid_path, hsperf_data_path));
  // TODO(yzhao): Combine with ResolveProcessPath() to get path inside container.
  PL_RETURN_IF_ERROR(Exists(hsperf_data_container_path));
  PL_ASSIGN_OR_RETURN(std::string hsperf_data_str, ReadFileToString(hsperf_data_container_path));
  return hsperf_data_str;
}

}  // namespace

Status JVMStatsConnector::ExportStats(const md::UPID& upid, DataTable* data_table) const {
  PL_ASSIGN_OR_RETURN(std::string hsperf_data_str, ReadHsperfData(upid.pid()));
  Stats stats(std::move(hsperf_data_str));
  if (!stats.Parse().ok()) {
    // Assumes this is a transient failure.
    return Status::OK();
  }
  RecordBuilder<&kJVMStatsTable> r(data_table);
  r.Append<kTimeIdx>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count() +
                     ClockRealTimeOffset());
  // TODO(yzhao): Figure out how to get the start time of the pid.
  system::ProcParser proc_parser(system::Config::GetInstance());
  r.Append<kUPIDIdx>(upid.value());
  r.Append<kYoungGCTimeIdx>(stats.YoungGCTimeNanos());
  r.Append<kFullGCTimeIdx>(stats.FullGCTimeNanos());
  r.Append<kUsedHeapSizeIdx>(stats.UsedHeapSizeBytes());
  r.Append<kTotalHeapSizeIdx>(stats.TotalHeapSizeBytes());
  r.Append<kMaxHeapSizeIdx>(stats.MaxHeapSizeBytes());
  return Status::OK();
}

void JVMStatsConnector::TransferDataImpl(ConnectorContext* ctx, uint32_t table_num,
                                         DataTable* data_table) {
  DCHECK_LT(table_num, num_tables())
      << absl::Substitute("Trying to access unexpected table: table_num=$0", table_num);

  absl::flat_hash_set<md::UPID> scanned_java_upids;

  // First scan the previously-scanned JVM processes.
  for (const md::UPID& upid : prev_scanned_java_upids_) {
    if (ExportStats(upid, data_table).ok()) {
      scanned_java_upids.insert(upid);
    }
  }
  for (const md::UPID& upid : FindNewUPIDs(*ctx)) {
    if (ExportStats(upid, data_table).ok()) {
      scanned_java_upids.insert(upid);
    }
  }
  prev_scanned_java_upids_ = std::move(scanned_java_upids);
}

}  // namespace stirling
}  // namespace pl

#endif
