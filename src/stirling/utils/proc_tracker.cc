#include "src/stirling/utils/proc_tracker.h"

#include <utility>

#include "src/common/system/proc_parser.h"

namespace pl {
namespace stirling {

void ProcTracker::Update(absl::flat_hash_set<md::UPID> upids) {
  new_upids_.clear();
  for (const auto& upid : upids) {
    auto iter = upids_.find(upid);
    if (iter != upids_.end()) {
      upids_.erase(iter);
      continue;
    }
    new_upids_.emplace(upid);
  }
  deleted_upids_ = std::move(upids_);
  upids_ = std::move(upids);
}

}  // namespace stirling
}  // namespace pl
