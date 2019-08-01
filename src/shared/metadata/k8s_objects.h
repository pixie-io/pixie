#pragma once

#include <memory>
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
class K8sMetadataObject {
 public:
  K8sMetadataObject() = delete;
  virtual ~K8sMetadataObject() = default;

  K8sMetadataObject(K8sObjectType type, UID uid, std::string_view ns, std::string_view name)
      : type_(type), uid_(std::move(uid)), ns_(ns), name_(name) {}

  K8sObjectType type() { return type_; }

  const UID& uid() const { return uid_; }

  const std::string& name() const { return name_; }
  const std::string& ns() const { return ns_; }

  int64_t start_time_ns() const { return start_time_ns_; }
  void set_start_time_ns(int64_t start_time_ns) { start_time_ns_ = start_time_ns; }

  int64_t stop_time_ns() const { return stop_time_ns_; }
  void set_stop_time_ns(int64_t stop_time_ns) { stop_time_ns_ = stop_time_ns; }

  virtual std::unique_ptr<K8sMetadataObject> Clone() const = 0;
  virtual std::string DebugString(int indent = 0) const = 0;

 protected:
  K8sMetadataObject(const K8sMetadataObject& other) = default;
  K8sMetadataObject& operator=(const K8sMetadataObject& other) = delete;

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
   * The namespace for this object.
   */

  std::string ns_;

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

enum class PodQOSClass : uint8_t { kUnknown = 0, kGuaranteed, kBestEffort, kBurstable };

/**
 * PodInfo contains information about K8s pods.
 */
class PodInfo : public K8sMetadataObject {
 public:
  PodInfo(UID uid, std::string_view ns, std::string_view name, PodQOSClass qos_class)
      : K8sMetadataObject(K8sObjectType::kPod, std::move(uid), std::move(ns), std::move(name)),
        qos_class_(qos_class) {}
  virtual ~PodInfo() = default;

  void AddContainer(CIDView cid) { containers_.emplace(cid); }
  void RmContainer(CIDView cid) { containers_.erase(cid); }
  PodQOSClass qos_class() const { return qos_class_; }

  const absl::flat_hash_set<std::string>& containers() const { return containers_; }

  std::unique_ptr<K8sMetadataObject> Clone() const override {
    return std::unique_ptr<PodInfo>(new PodInfo(*this));
  }

  std::string DebugString(int indent = 0) const override;

 protected:
  PodInfo(const PodInfo& other) = default;
  PodInfo& operator=(const PodInfo& other) = delete;

 private:
  PodQOSClass qos_class_;
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
class ContainerInfo {
 public:
  ContainerInfo() = delete;
  explicit ContainerInfo(CID cid, int64_t start_time_ns)
      : cid_(std::move(cid)), start_time_ns_(start_time_ns), stop_time_ns_(0) {}

  const CID& cid() const { return cid_; }

  void set_pod_id(std::string_view pod_id) { pod_id_ = pod_id; }
  const UID& pod_id() const { return pod_id_; }

  void AddUPID(UPID upid) { active_upids_.emplace(upid); }
  void DeactivateUPID(UPID upid) {
    auto it = active_upids_.find(upid);
    if (it != active_upids_.end()) {
      inactive_upids_.emplace(*it);
      active_upids_.erase(it);
    }
  }

  bool HasActiveUPID(UPID upid) const { return active_upids_.contains(upid); }
  bool HasInActiveUPID(UPID upid) const { return inactive_upids_.contains(upid); }
  bool HasUPID(UPID upid) const { return HasActiveUPID(upid) || HasInActiveUPID(upid); }

  const absl::flat_hash_set<UPID>& active_upids() const { return active_upids_; }
  const absl::flat_hash_set<UPID>& inactive_upids() const { return inactive_upids_; }

  int64_t start_time_ns() const { return start_time_ns_; }

  int64_t stop_time_ns() const { return stop_time_ns_; }
  void set_stop_time_ns(int64_t stop_time_ns) { stop_time_ns_ = stop_time_ns; }

  std::unique_ptr<ContainerInfo> Clone() const {
    return std::unique_ptr<ContainerInfo>(new ContainerInfo(*this));
  }

  std::string DebugString(int indent = 0) const;

 protected:
  ContainerInfo(const ContainerInfo& other) = default;
  ContainerInfo& operator=(const ContainerInfo& other) = delete;

 private:
  const CID cid_;
  UID pod_id_ = "";

  /**
   * The set of UPIDs that are running on this container.
   */
  absl::flat_hash_set<UPID> active_upids_;

  /**
   * The set of UPIDs that used to run on this container but have since been killed.
   * We maintain them for a while so that they remain queryable.
   */
  absl::flat_hash_set<UPID> inactive_upids_;

  /**
   * Start time of this K8s object.
   */
  const int64_t start_time_ns_ = 0;

  /**
   * Stop time of this K8s object.
   * A value of 0 implies that the object is still active.
   */
  int64_t stop_time_ns_ = 0;
};

}  // namespace md
}  // namespace pl
