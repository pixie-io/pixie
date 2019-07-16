#pragma once

#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "src/common/base/base.h"
#include "src/shared/metadata/base_types.h"

namespace pl {
namespace md {

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

  int64_t start_time_ns() { return start_time_ns_; }
  int64_t stop_time_ns() { return stop_time_ns_; }

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

  /**
   * Start time of this K8s object.
   */
  int64_t start_time_ns_ = 0;

  /**
   * Stop time of this K8s object.
   * A value of 0 implies that the object is still active.
   */
  int64_t stop_time_ns_ = 0;
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
 *
 * Though this is not strictly a K8s object, it's state is tracked by K8s
 * so we include it here.
 */
struct ContainerInfo : public NotCopyable {
 public:
  explicit ContainerInfo(CID cid) : cid_(std::move(cid)) {}
  const CID& cid() { return cid_; }
  const absl::flat_hash_set<UPID>& pids() { return pids_; }

  int64_t start_time_ns() { return start_time_ns_; }
  int64_t stop_time_ns() { return stop_time_ns_; }

 private:
  CID cid_;
  absl::flat_hash_set<UPID> pids_;

  /**
   * Start time of this K8s object.
   */
  int64_t start_time_ns_ = 0;

  /**
   * Stop time of this K8s object.
   * A value of 0 implies that the object is still active.
   */
  int64_t stop_time_ns_ = 0;
};

}  // namespace md
}  // namespace pl
