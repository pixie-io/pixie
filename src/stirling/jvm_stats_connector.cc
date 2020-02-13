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

void JVMStatsConnector::TransferDataImpl(ConnectorContext* ctx, uint32_t table_num,
                                         DataTable* data_table) {
  DCHECK_LT(table_num, num_tables())
      << absl::Substitute("Trying to access unexpected table: table_num=$0", table_num);

  // TODO(yzhao): Figure out a way to be able to not list all processes.
  for (const auto& [pid, path] : ListProcPidPaths(sysconfig_.proc_path())) {
    auto hsperf_data_str_or = ReadHsperfData(pid);
    if (!hsperf_data_str_or.ok()) {
      VLOG(1) << absl::Substitute("Failed to read hsperfdata data, error: '$0'",
                                  hsperf_data_str_or.status().ToString());
      continue;
    }
    Stats stats(hsperf_data_str_or.ConsumeValueOrDie());
    auto parse_status = stats.Parse();
    if (!parse_status.ok()) {
      VLOG(1) << absl::Substitute("Failed to parse hsperfdata data, error: '$0'",
                                  hsperf_data_str_or.status().ToString());
      continue;
    }
    RecordBuilder<&kJVMStatsTable> r(data_table);
    r.Append<kTimeIdx>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count() +
                       ClockRealTimeOffset());
    // TODO(yzhao): Figure out how to get the start time of the pid.
    system::ProcParser proc_parser(system::Config::GetInstance());
    md::UPID upid(ctx->AgentMetadataState()->asid(), pid, proc_parser.GetPIDStartTimeTicks(pid));
    r.Append<kUPIDIdx>(upid.value());
    r.Append<kYoungGCTimeIdx>(stats.YoungGCTimeNanos());
    r.Append<kFullGCTimeIdx>(stats.FullGCTimeNanos());
    r.Append<kUsedHeapSizeIdx>(stats.UsedHeapSizeBytes());
    r.Append<kTotalHeapSizeIdx>(stats.TotalHeapSizeBytes());
    r.Append<kMaxHeapSizeIdx>(stats.MaxHeapSizeBytes());
  }
}

}  // namespace stirling
}  // namespace pl

#endif
