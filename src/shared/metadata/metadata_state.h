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
  using PodUpdate = pl::shared::k8s::metadatapb::PodUpdate;
  using ContainerUpdate = pl::shared::k8s::metadatapb::ContainerUpdate;

  // K8s names consist of both a namespace and name : <ns, name>.
  using K8sNameIdent = std::pair<std::string, std::string>;

  const absl::flat_hash_map<K8sNameIdent, UID>& pods_by_name() { return pods_by_name_; }

  /**
   * PodInfoByID gets an unowned pointer to the Pod. This pointer will remain active
   * for the lifetime of this metadata state instance.
   * @param pod_id the id of the POD.
   * @return Pointer to the PodInfo.
   */
  const PodInfo* PodInfoByID(UID pod_id) const;

  /**
   * PodIDByName returns the PodID for the pod of the given name.
   * @param pod_name the pod name
   * @return the pod id or empty string if the pod does not exist.
   */
  UID PodIDByName(K8sNameIdent pod_name) const;

  /**
   * ContainerInfoByID returns the container info by ID.
   * @param id The ID of the container.
   * @return ContainerInfo or nullptr if not found.
   */
  const ContainerInfo* ContainerInfoByID(const CID& id) const;

  std::unique_ptr<K8sMetadataState> Clone() const;

  Status HandlePodUpdate(const PodUpdate& update);
  Status HandleContainerUpdate(const ContainerUpdate& update);

 private:
  // This stores K8s native objects (services, pods, etc).
  absl::flat_hash_map<UID, K8sMetadataObjectUPtr> k8s_objects_;

  // TODO(zasgar/michelle): Add heterogeneous lookup from std::pair<string_view, string_view>.
  /**
   * Mapping of pods by name.
   */
  absl::flat_hash_map<K8sNameIdent, UID> pods_by_name_;

  /**
   * Mapping of containers by ID.
   */
  absl::flat_hash_map<CID, ContainerInfoUPtr> containers_by_id_;
};

class AgentMetadataState : NotCopyable {
 public:
  AgentMetadataState() = delete;
  explicit AgentMetadataState(uint32_t agent_id)
      : agent_id_(agent_id), k8s_metadata_state_(new K8sMetadataState()) {}

  uint32_t agent_id() const { return agent_id_; }

  int64_t last_update_ts_ns() const { return last_update_ts_ns_; }
  void set_last_update_ts_ns(int64_t last_update_ts_ns) { last_update_ts_ns_ = last_update_ts_ns; }

  uint64_t epoch_id() const { return epoch_id_; }
  void set_epoch_id(uint64_t id) { epoch_id_ = id; }

  // Returns an un-owned pointer to the underlying k8s state.
  K8sMetadataState* k8s_metadata_state() { return k8s_metadata_state_.get(); }

  std::shared_ptr<AgentMetadataState> CloneToShared() const;

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
