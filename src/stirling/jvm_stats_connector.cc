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
#include "src/stirling/utils/proc_tracker.h"

namespace pl {
namespace stirling {

using ::pl::fs::Exists;
using ::pl::stirling::obj_tools::ResolveProcessPath;
using ::pl::utils::LEndianBytesToInt;

absl::flat_hash_set<md::UPID> JVMStatsConnector::FindJavaUPIDs(const ConnectorContext& ctx) {
  std::filesystem::path proc_path = system::Config::GetInstance().proc_path();

  absl::flat_hash_map<md::UPID, std::filesystem::path> upid_proc_path_map =
      ProcTracker::Cleanse(proc_path, ctx.GetMdsUpids());

  if (upid_proc_path_map.empty()) {
    upid_proc_path_map = ProcTracker::ListUPIDs(proc_path);
  }

  absl::flat_hash_map<md::UPID, std::filesystem::path> new_upid_proc_path_map =
      proc_tracker_.TakeSnapshotAndDiff(std::move(upid_proc_path_map));

  absl::flat_hash_set<md::UPID> java_upids = prev_scanned_java_upids_;
  for (const auto& [upid, proc_pid_path] : new_upid_proc_path_map) {
    java_upids.insert(upid);
  }

  return java_upids;
}

namespace {

StatusOr<std::string> ReadHsperfDataOverlayFS(pid_t pid) {
  PL_ASSIGN_OR_RETURN(const std::filesystem::path hsperf_data_path, HsperfdataPath(pid));

  const std::filesystem::path proc_pid_path =
      system::Config::GetInstance().proc_path() / std::to_string(pid);
  PL_ASSIGN_OR_RETURN(std::filesystem::path hsperf_data_container_path,
                      ResolveProcessPath(proc_pid_path, hsperf_data_path));

  const std::filesystem::path host_path = system::Config::GetInstance().host_path();
  hsperf_data_container_path = fs::JoinPath({&host_path, &hsperf_data_container_path});

  PL_RETURN_IF_ERROR(Exists(hsperf_data_container_path));

  return ReadFileToString(hsperf_data_container_path);
}

StatusOr<std::string> ReadHsperfDataMountInfo(pid_t pid) {
  PL_ASSIGN_OR_RETURN(const std::filesystem::path hsperf_data_path, HsperfdataPath(pid));

  const auto& config = system::Config::GetInstance();

  system::ProcParser proc_parser(config);

  // Find the longest parent path that is accessible of the hsperfdata file, by resolving mount
  // point starting from the immediate parent through the root.
  for (const fs::PathSplit& path_split : fs::EnumerateParentPaths(hsperf_data_path)) {
    auto resolved_mount_path_or = proc_parser.ResolveMountPoint(pid, path_split.parent);
    if (resolved_mount_path_or.ok()) {
      const std::filesystem::path host_path = system::Config::GetInstance().host_path();
      auto tmp =
          fs::JoinPath({&host_path, &resolved_mount_path_or.ValueOrDie(), &path_split.child});
      return ReadFileToString(tmp);
    }
  }
  return error::Internal("Could not resolve hsperfdata file for pid=$0", pid);
}

}  // namespace

Status JVMStatsConnector::ExportStats(const md::UPID& upid, DataTable* data_table) const {
  // TODO(yzhao): Read everything from /proc/[pid]/mountinfo. So that we do not need to read two
  // proc files.
  auto hsperf_data_str_or = ReadHsperfDataOverlayFS(upid.pid());
  if (!hsperf_data_str_or.ok()) {
    hsperf_data_str_or = ReadHsperfDataMountInfo(upid.pid());
  }
  PL_ASSIGN_OR_RETURN(std::string hsperf_data_str, hsperf_data_str_or);

  if (hsperf_data_str.empty()) {
    // Assumes only file reading failed, and is transient.
    return Status::OK();
  }

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

  for (const md::UPID& upid : FindJavaUPIDs(*ctx)) {
    md::UPID upid_with_asid(ctx->GetASID(), upid.pid(), upid.start_ts());
    if (ExportStats(upid_with_asid, data_table).ok()) {
      scanned_java_upids.insert(upid);
    }
  }
  prev_scanned_java_upids_.swap(scanned_java_upids);
}

}  // namespace stirling
}  // namespace pl

#endif
