#pragma once

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include "src/common/system/proc_parser.h"
#include "src/shared/metadata/base_types.h"

namespace pl {
namespace stirling {

struct UPIDDelta {
  absl::flat_hash_set<md::UPID> new_upids;
  absl::flat_hash_set<md::UPID> deleted_upids;
};

/**
 * Keeps a list of UPIDs. Tracks newly-created and terminated process, each time when a system-wide
 * rescanning is requested, and update its internal list of UPIDs.
 */
// TODO(yzhao): Consider moving this into src/common/system. Today that cannot be done because
// md::UPID is from src/shared/metadata, which already depends on src/common/system. Having this
// inside src/common/system would cause circular dependency.
class ProcTracker : NotCopyMoveable {
 public:
  /**
   * Returns the list of processes from the proc filesystem.
   */
  static absl::flat_hash_set<md::UPID> ListUPIDs(const std::filesystem::path& proc_path);

  /**
   * Standardizes by setting ASID of UPIDs to 0.
   */
  static absl::flat_hash_set<md::UPID> Cleanse(const absl::flat_hash_set<md::UPID>& upids);

  /**
   * Accepts the list of all currently-running processes. Returns the list of newly-created
   * processes since last snapshot.
   */
  // TODO(yzhao): Consider calling ListUPIDs() directly inside TakeSnapshotAndDiff().
  UPIDDelta TakeSnapshotAndDiff(absl::flat_hash_set<md::UPID> upids);

  const auto& upids() const { return upids_; }

 private:
  absl::flat_hash_set<md::UPID> upids_;
};

}  // namespace stirling
}  // namespace pl
