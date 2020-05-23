#include "src/stirling/utils/proc_tracker.h"

#include <string>
#include <utility>

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

UPIDDelta ProcTracker::TakeSnapshotAndDiff(absl::flat_hash_set<md::UPID> upids) {
  UPIDDelta result;
  for (const auto& upid : upids) {
    auto iter = upids_.find(upid);
    if (iter != upids_.end()) {
      upids_.erase(iter);
      continue;
    }
    result.new_upids.emplace(upid);
  }
  result.deleted_upids = std::move(upids_);
  upids_ = std::move(upids);
  return result;
}

}  // namespace stirling
}  // namespace pl
