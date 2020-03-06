#include "src/stirling/utils/proc_tracker.h"

#include <string>

#include "src/common/system/proc_parser.h"

namespace pl {
namespace stirling {

absl::flat_hash_map<md::UPID, std::filesystem::path> ProcTracker::ListUPIDs(
    const std::filesystem::path& proc_path) {
  absl::flat_hash_map<md::UPID, std::filesystem::path> pids;
  for (const auto& p : std::filesystem::directory_iterator(proc_path)) {
    uint32_t pid = 0;
    if (!absl::SimpleAtoi(p.path().filename().string(), &pid)) {
      continue;
    }
    md::UPID upid(/*asid*/ 0, pid, system::GetPIDStartTimeTicks(p.path()));
    pids[upid] = p.path();
  }
  return pids;
}

absl::flat_hash_map<md::UPID, std::filesystem::path> ProcTracker::Cleanse(
    const std::filesystem::path& proc_path, const absl::flat_hash_set<md::UPID>& upids) {
  absl::flat_hash_map<md::UPID, std::filesystem::path> res;
  for (const md::UPID& upid : upids) {
    md::UPID local_upid(/*asid*/ 0, upid.pid(), upid.start_ts());
    res[local_upid] = proc_path / std::to_string(local_upid.pid());
  }
  return res;
}

absl::flat_hash_map<md::UPID, std::filesystem::path> ProcTracker::TakeSnapshotAndDiff(
    absl::flat_hash_map<md::UPID, std::filesystem::path> upids) {
  absl::flat_hash_map<md::UPID, std::filesystem::path> new_upid_paths;
  for (const auto& [upid, pid_path] : upids) {
    if (upids_.contains(upid)) {
      continue;
    }
    new_upid_paths[upid] = pid_path;
  }
  upids_.swap(upids);
  return new_upid_paths;
}

}  // namespace stirling
}  // namespace pl
