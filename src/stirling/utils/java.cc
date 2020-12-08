#include "src/stirling/utils/java.h"

#include <absl/strings/match.h>
#include <map>
#include <string>
#include <vector>

#include "src/common/base/byte_utils.h"
#include "src/common/base/statusor.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/proc_parser.h"
#include "src/common/system/uid.h"
#include "src/stirling/utils/hsperfdata.h"

namespace pl {
namespace stirling {

using ::pl::stirling::hsperf::ParseHsperfData;
using ::pl::system::ProcParser;
using ::pl::utils::LEndianBytesToInt;

Stats::Stats(std::vector<Stat> stats) : stats_(std::move(stats)) {}
Stats::Stats(std::string hsperf_data_str) : hsperf_data_(std::move(hsperf_data_str)) {}

Status Stats::Parse() {
  hsperf::HsperfData hsperf_data = {};
  PL_RETURN_IF_ERROR(ParseHsperfData(hsperf_data_, &hsperf_data));
  for (const auto& entry : hsperf_data.data_entries) {
    if (entry.header->data_type != static_cast<uint8_t>(hsperf::DataType::kLong)) {
      continue;
    }
    auto value = LEndianBytesToInt<uint64_t>(entry.data);
    stats_.push_back({entry.name, value});
  }
  return Status::OK();
}

uint64_t Stats::YoungGCTimeNanos() const { return StatForSuffix("gc.collector.0.time"); }

uint64_t Stats::FullGCTimeNanos() const { return StatForSuffix("gc.collector.1.time"); }

uint64_t Stats::UsedHeapSizeBytes() const {
  return SumStatsForSuffixes({
      "gc.generation.0.space.0.used",
      "gc.generation.0.space.1.used",
      "gc.generation.0.space.2.used",
      "gc.generation.1.space.0.used",
  });
}

uint64_t Stats::TotalHeapSizeBytes() const {
  return SumStatsForSuffixes({
      "gc.generation.0.space.0.capacity",
      "gc.generation.0.space.1.capacity",
      "gc.generation.0.space.2.capacity",
      "gc.generation.1.space.0.capacity",
  });
}

uint64_t Stats::MaxHeapSizeBytes() const {
  return SumStatsForSuffixes({
      "gc.generation.0.maxCapacity",
      "gc.generation.1.maxCapacity",
  });
}

uint64_t Stats::StatForSuffix(std::string_view suffix) const {
  for (const auto& stat : stats_) {
    if (absl::EndsWith(stat.name, suffix)) {
      return stat.value;
    }
  }
  return 0;
}

uint64_t Stats::SumStatsForSuffixes(const std::vector<std::string_view>& suffixes) const {
  uint64_t sum = 0;
  for (const auto& suffix : suffixes) {
    sum += StatForSuffix(suffix);
  }
  return sum;
}

StatusOr<std::filesystem::path> ResolvePidPath(pid_t pid, const std::filesystem::path& path) {
  const auto& config = system::Config::GetInstance();
  const std::filesystem::path& host_path = config.host_path();

  ProcParser proc_parser(config);

  // Find the longest parent path that is accessible of the file, by resolving mount
  // point starting from the immediate parent through the root.
  for (const fs::PathSplit& path_split : fs::EnumerateParentPaths(path)) {
    auto resolved_mount_path_or = proc_parser.ResolveMountPoint(pid, path_split.parent);
    if (resolved_mount_path_or.ok()) {
      return fs::JoinPath({&host_path, &resolved_mount_path_or.ValueOrDie(), &path_split.child});
    }
  }

  return error::Internal("Could not resolve $0 for pid=$1", path.string(), pid);
}

StatusOr<std::filesystem::path> HsperfdataPath(pid_t pid) {
  ProcParser parser(system::Config::GetInstance());

  ProcParser::ProcUIDs uids;
  PL_RETURN_IF_ERROR(parser.ReadUIDs(pid, &uids));

  uid_t effective_uid = 0;
  if (!absl::SimpleAtoi(uids.effective, &effective_uid)) {
    return error::InvalidArgument("Invalid uid: '$0'", uids.effective);
  }

  const std::filesystem::path etc_passwd_path("/etc/passwd");
  PL_ASSIGN_OR_RETURN(std::filesystem::path passwd_path, ResolvePidPath(pid, etc_passwd_path));

  PL_ASSIGN_OR_RETURN(const std::string passwd_content, ReadFileToString(passwd_path));
  std::map<uid_t, std::string> uid_user_map = ParsePasswd(passwd_content);
  auto iter = uid_user_map.find(effective_uid);
  if (iter == uid_user_map.end()) {
    return error::InvalidArgument("PID=$0 effective_uid=$1 cannot be found in $2", pid,
                                  effective_uid, passwd_path.string());
  }
  const std::string& effective_user = iter->second;

  std::vector<std::string> ns_pids;
  PL_RETURN_IF_ERROR(parser.ReadNSPid(pid, &ns_pids));
  const char kHspefdataPrefix[] = "hsperfdata_";
  return std::filesystem::path("/tmp") / absl::StrCat(kHspefdataPrefix, effective_user) /
         // The right-most pid is the PID of the same process inside the most-nested namespace.
         // That will be the filename chosen by the running process.
         ns_pids.back();
}

}  // namespace stirling
}  // namespace pl
