#include "src/stirling/utils/proc_tracker.h"

#include <string>

#include "src/common/system/proc_parser.h"

namespace pl {
namespace stirling {

absl::flat_hash_set<md::UPID> ProcTracker::ListUPIDs(const std::filesystem::path& proc_path) {
  absl::flat_hash_set<md::UPID> pids;
  for (const auto& p : std::filesystem::directory_iterator(proc_path)) {
    uint32_t pid = 0;
    if (!absl::SimpleAtoi(p.path().filename().string(), &pid)) {
      continue;
    }
    pids.emplace(/*asid*/ 0, pid, system::GetPIDStartTimeTicks(p.path()));
  }
  return pids;
}

absl::flat_hash_set<md::UPID> ProcTracker::Cleanse(const absl::flat_hash_set<md::UPID>& upids) {
  absl::flat_hash_set<md::UPID> res;
  for (const md::UPID& upid : upids) {
    res.emplace(/*asid*/ 0, upid.pid(), upid.start_ts());
  }
  return res;
}

absl::flat_hash_set<md::UPID> ProcTracker::TakeSnapshotAndDiff(
    absl::flat_hash_set<md::UPID> upids) {
  absl::flat_hash_set<md::UPID> new_upids;
  for (const auto& upid : upids) {
    if (upids_.contains(upid)) {
      continue;
    }
    new_upids.emplace(upid);
  }
  upids_.swap(upids);
  return new_upids;
}

}  // namespace stirling
}  // namespace pl
