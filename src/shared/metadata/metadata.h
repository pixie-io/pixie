#pragma once

#include <memory>
#include <string>
#include <utility>

#include "absl/base/internal/spinlock.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "src/common/base/base.h"
#include "src/shared/metadata/base_types.h"
#include "src/shared/metadata/k8s_objects.h"
#include "src/shared/metadata/pids.h"

namespace pl {
namespace md {

using K8sMetadataObjectUPtr = std::unique_ptr<K8sMetadataObject>;
using ContainerInfoUPtr = std::unique_ptr<ContainerInfo>;
using PIDInfoUPtr = std::unique_ptr<PIDInfo>;

/**
 * This class contains all kubernetes relate metadata.
 */
class K8sMetadataState {
  const absl::flat_hash_map<std::string, UID>& pods_by_name() { return pods_by_name_; }

 private:
  // This stores K8s native objects (services, pods, etc).
  absl::flat_hash_map<UID, K8sMetadataObjectUPtr> k8s_objects_;

  /**
   * Mapping of pods by name.
   */
  absl::flat_hash_map<std::string, UID> pods_by_name_;

  /**
   * Mapping of containers by ID.
   */
  absl::flat_hash_map<CID, ContainerInfoUPtr> containers_by_id_;
};

class AgentMetadataState {
  uint32_t agent_id() { return agent_id_; }
  int64_t last_update_ts_ns() { return last_update_ts_ns_; }
  uint64_t epoch_id() { return epoch_id_; }

 private:
  /**
   * Tracks the time that this K8s metadata object was created. The object should be periodically
   * refreshed to get the latest version.
   */
  int64_t last_update_ts_ns_ = 0;

  /**
   * A monotonically increasing number that tracks the epoch of this K8s state.
   * The epoch is incremented everytime a new MetadataState is create. MetadataState objects after
   * creation should be immutable to allow concurrent read access without locks.
   */
  uint64_t epoch_id_ = 0;

  uint32_t agent_id_;

  K8sMetadataState k8s_metadata_state_;

  /**
   * Mapping of PIDs by UPID for active pods on the system.
   */
  absl::flat_hash_map<UPID, PIDInfoUPtr> pids_by_upid_;
};

/**
 * AgentMetadata has all the metadata that is tracked on a per agent basis.
 */
class AgentMetadataStateManager {
 public:
  explicit AgentMetadataStateManager(uint32_t agent_id) : agent_id_(agent_id) {}

  uint32_t agent_id() { return agent_id_; }

  /**
   * This returns the current valid K8sMetadataState. The state is periodically updated
   * and returned pointers should not be held for a long time to avoid memory leaks and
   * stale data.
   *
   * @return shared_ptr to the current AgentMetadataState.
   */
  std::shared_ptr<const AgentMetadataState> CurrentAgentMetadataState();

  /**
   * When called this function will perform a state update of the metadata.
   * This involves:
   *   1. Create a copy of the current metadata state.
   *   2. Drain the incoming update queue from the metadata service and apply the updates.
   *   3. Invoke a pull on the cgroup manager and get the new state of containers and PIDs.
   *   4. Create a diff and send to outgoing message Q.
   *   5. Replace the current agent_metdata_state_ ptr.
   *
   * This function is meant to be invoked from a thread and is thread-safe.
   *
   * @return Status::OK on success.
   */
  Status PerformMetadataStateUpdate() {
    absl::base_internal::SpinLockHolder lock(&agent_metadata_state_lock_);
    return Status::OK();
  }

 private:
  uint32_t agent_id_;
  std::shared_ptr<AgentMetadataState> agent_metadata_state_;
  absl::base_internal::SpinLock agent_metadata_state_lock_;
};

}  // namespace md
}  // namespace pl
