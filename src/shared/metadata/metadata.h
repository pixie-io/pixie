#pragma once

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/numeric/int128.h"
#include "src/common/base/base.h"

namespace pl {
namespace md {

/**
 * UID refers to the unique IDs used by K8s. These IDs are
 * unique in both space and time.
 *
 * K8s stores UID as both regular strings and UUIDs.
 */
using UID = std::string;

/**
 * CID refers to a unique container ID. This ID is unique in both
 * space and time.
 */
using CID = std::string;

/**
 * Unique PIDs refers to uniquefied pids. They are unique in both
 * space and time. The format for a unique PID is:
 *
 * ------------------------------------------------------------
 * | 32-bit Agent ID |  32-bit PID  |     64-bit Start TS     |
 * ------------------------------------------------------------
 */
using UPID = absl::uint128;

/**
 * Enum with all the different metadata types.
 */
enum class K8sObjectType { kUnknown, kPod, kService };

/**
 * Base class for all K8s metadata objects.
 */
class K8sMetadataObject : public NotCopyable {
 public:
  K8sMetadataObject() = delete;
  virtual ~K8sMetadataObject() = default;

  K8sMetadataObject(K8sObjectType type, UID uid, std::string_view name)
      : type_(type), uid_(std::move(uid)), name_(name) {}

  K8sObjectType type() { return type_; }

  const UID& uid() { return uid_; }

  const std::string name() { return name_; }

 private:
  /**
   * The type of this object.
   */
  const K8sObjectType type_ = K8sObjectType::kUnknown;

  /**
   * The ID assigned by K8s that is unique in both space and time.
   */
  const UID uid_ = 0;

  /**
   * The name which is unique in space but not time.
   */
  std::string name_;
};

/**
 * PodInfo contains information about K8s pods.
 */
class PodInfo : public K8sMetadataObject {
 public:
  PodInfo(UID uid, std::string_view name)
      : K8sMetadataObject(K8sObjectType::kPod, std::move(uid), std::move(name)) {}
  virtual ~PodInfo() = default;

  const absl::flat_hash_set<std::string>& containers() { return containers_; }

 private:
  /**
   * Set of containers that are running on this pod.
   *
   * The ContainerInformation is located in containers in the K8s state.
   */
  absl::flat_hash_set<CID> containers_;
};

/**
 * Store information about containers.
 */
struct ContainerInfo {
 public:
  explicit ContainerInfo(CID cid) : cid_(std::move(cid)) {}
  const CID& cid() { return cid_; }
  const absl::flat_hash_set<UPID>& pids() { return pids_; }

 private:
  CID cid_;
  absl::flat_hash_set<UPID> pids_;
};

/**
 * Store information about PIDs.
 */
struct PIDInfo {
  UPID upid;
  int64_t pid;
};

using K8sMetadataObjectUPtr = std::unique_ptr<K8sMetadataObject>;
using ContainerInfoUPtr = std::unique_ptr<ContainerInfo>;
using PIDInfoUPtr = std::unique_ptr<PIDInfo>;

/**
 * This class contains all kubernetes relate metadata.
 */
class K8sMetadataState {
  const absl::flat_hash_map<UID, K8sMetadataObjectUPtr>& k8s_objects() { return k8s_objects_; }

  const absl::flat_hash_map<std::string, UID>& pods_by_name() { return pods_by_name_; }

  const absl::flat_hash_map<CID, ContainerInfoUPtr>& containers_by_id() {
    return containers_by_id_;
  }

  const absl::flat_hash_map<UPID, PIDInfoUPtr>& pids_by_upid() { return pids_by_upid_; }

 private:
  absl::flat_hash_map<UID, K8sMetadataObjectUPtr> k8s_objects_;

  /**
   * Mapping of pods by name.
   */
  absl::flat_hash_map<std::string, UID> pods_by_name_;

  /**
   * Mapping of containers by ID.
   */
  absl::flat_hash_map<CID, ContainerInfoUPtr> containers_by_id_;

  /**
   * Mapping of PIDs by UPID.
   */
  absl::flat_hash_map<UPID, PIDInfoUPtr> pids_by_upid_;
};

/**
 * AgentMetadata has all the metadata that is tracked on a per agent basis.
 */
class AgentMetadataState {
 public:
  explicit AgentMetadataState(uint32_t agent_id) : agent_id_(agent_id) {}

  uint32_t agent_id() { return agent_id_; }

  K8sMetadataState& k8s_metadata_state() { return k8s_metadata_state_; }

 private:
  uint32_t agent_id_;
  K8sMetadataState k8s_metadata_state_;
};

}  // namespace md
}  // namespace pl
