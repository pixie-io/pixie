#pragma once

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"

#include "src/common/base/base.h"
#include "src/shared/k8s/metadatapb/metadata.pb.h"
#include "src/shared/metadata/base_types.h"
#include "src/shared/metadata/k8s_objects.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/shared/metadata/pids.h"

namespace pl {
namespace md {

using K8sMetadataObjectUPtr = std::unique_ptr<K8sMetadataObject>;
using ContainerInfoUPtr = std::unique_ptr<ContainerInfo>;
using PIDInfoUPtr = std::unique_ptr<PIDInfo>;

/**
 * This class contains all kubernetes relate metadata.
 */
class K8sMetadataState : NotCopyable {
 public:
  const absl::flat_hash_map<std::string, UID>& pods_by_name() { return pods_by_name_; }

  std::unique_ptr<K8sMetadataState> Clone() const {
    auto other = std::make_unique<K8sMetadataState>();

    other->k8s_objects_.reserve(k8s_objects_.size());
    for (const auto& [k, v] : k8s_objects_) {
      other->k8s_objects_[k] = v->Clone();
    }

    other->pods_by_name_.reserve(pods_by_name_.size());
    for (const auto& [k, v] : pods_by_name_) {
      other->pods_by_name_[k] = v;
    }

    other->containers_by_id_.reserve(containers_by_id_.size());
    for (const auto& [k, v] : containers_by_id_) {
      other->containers_by_id_[k] = v->Clone();
    }
    return other;
  }

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

class AgentMetadataState : NotCopyable {
 public:
  AgentMetadataState() = delete;
  explicit AgentMetadataState(uint32_t agent_id) : agent_id_(agent_id) {}

  uint32_t agent_id() const { return agent_id_; }

  int64_t last_update_ts_ns() const { return last_update_ts_ns_; }
  void set_last_update_ts_ns(int64_t last_update_ts_ns) { last_update_ts_ns_ = last_update_ts_ns; }

  uint64_t epoch_id() const { return epoch_id_; }
  void set_epoch_id(uint64_t id) { epoch_id_ = id; }

  std::shared_ptr<AgentMetadataState> CloneToShared() const {
    std::shared_ptr<AgentMetadataState> state;
    state->last_update_ts_ns_ = last_update_ts_ns_;
    state->epoch_id_ = epoch_id_;
    state->agent_id_ = agent_id_;
    state->k8s_metadata_state_ = k8s_metadata_state_->Clone();
    state->pids_by_upid_.reserve(pids_by_upid_.size());

    for (const auto& [k, v] : pids_by_upid_) {
      state->pids_by_upid_[k] = v->Clone();
    }
    return state;
  }

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

  std::unique_ptr<K8sMetadataState> k8s_metadata_state_;

  /**
   * Mapping of PIDs by UPID for active pods on the system.
   */
  absl::flat_hash_map<UPID, PIDInfoUPtr> pids_by_upid_;
};

}  // namespace md
}  // namespace pl
