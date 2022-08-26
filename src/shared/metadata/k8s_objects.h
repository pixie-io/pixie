/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/btree_set.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include "src/common/base/base.h"
#include "src/shared/k8s/metadatapb/metadata.pb.h"
#include "src/shared/upid/upid.h"

namespace px {
namespace md {

/**
 * Data structure for owner reference object.
 */
struct OwnerReference {
  UID uid;
  std::string name;
  std::string kind;
  friend bool operator==(const OwnerReference l, const OwnerReference r) { return l.uid == r.uid; }

  template <typename H>
  friend H AbslHashValue(H h, const OwnerReference& owner_reference) {
    return H::combine(std::move(h), owner_reference.uid);
  }
};

/**
 * Enum with all the different metadata types.
 */
enum class K8sObjectType { kUnknown, kPod, kService, kNamespace, kReplicaSet, kDeployment };

/**
 * Base class for all K8s metadata objects.
 */
class K8sMetadataObject {
 public:
  K8sMetadataObject() = delete;
  virtual ~K8sMetadataObject() = default;

  K8sMetadataObject(K8sObjectType type, UID uid, std::string_view ns, std::string_view name,
                    int64_t start_time_ns = 0, int64_t stop_time_ns = 0)
      : type_(type),
        uid_(std::move(uid)),
        ns_(ns),
        name_(name),
        start_time_ns_(start_time_ns),
        stop_time_ns_(stop_time_ns) {}

  K8sObjectType type() const { return type_; }

  const UID& uid() const { return uid_; }

  const std::string& name() const { return name_; }
  const std::string& ns() const { return ns_; }

  int64_t start_time_ns() const { return start_time_ns_; }
  void set_start_time_ns(int64_t start_time_ns) { start_time_ns_ = start_time_ns; }

  int64_t stop_time_ns() const { return stop_time_ns_; }
  void set_stop_time_ns(int64_t stop_time_ns) { stop_time_ns_ = stop_time_ns; }

  const absl::flat_hash_set<OwnerReference>& owner_references() const { return owner_references_; }
  void AddOwnerReference(UID uid, std::string name, std::string kind) {
    owner_references_.emplace(OwnerReference{uid, name, kind});
  }
  void RmOwnerReference(UID uid) { owner_references_.erase(OwnerReference{uid, "", ""}); }

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

  /**
   * The set of owners which control this pod. K8s allows
   * multiple owners to control the same pod.
   *
   * Should point to owner Info object via the data structure containing this pod.
   */
  absl::flat_hash_set<OwnerReference> owner_references_;
};

enum class PodQOSClass : uint8_t { kUnknown = 0, kGuaranteed, kBestEffort, kBurstable };

inline PodQOSClass ConvertToPodQOsClass(px::shared::k8s::metadatapb::PodQOSClass pb_enum) {
  using qos_pb = px::shared::k8s::metadatapb::PodQOSClass;
  switch (pb_enum) {
    case qos_pb::QOS_CLASS_BURSTABLE:
      return PodQOSClass::kBurstable;
    case qos_pb::QOS_CLASS_BEST_EFFORT:
      return PodQOSClass::kBestEffort;
    case qos_pb::QOS_CLASS_GUARANTEED:
      return PodQOSClass::kGuaranteed;
    default:
      return PodQOSClass::kUnknown;
  }
}

enum class PodPhase : uint8_t {
  kUnknown = 0,
  kPending,
  kRunning,
  kSucceeded,
  kFailed,
  kTerminated
};

inline PodPhase ConvertToPodPhase(px::shared::k8s::metadatapb::PodPhase pb_enum) {
  using phase_pb = px::shared::k8s::metadatapb::PodPhase;
  switch (pb_enum) {
    case phase_pb::PENDING:
      return PodPhase::kPending;
    case phase_pb::RUNNING:
      return PodPhase::kRunning;
    case phase_pb::SUCCEEDED:
      return PodPhase::kSucceeded;
    case phase_pb::FAILED:
      return PodPhase::kFailed;
    case phase_pb::TERMINATED:
      return PodPhase::kTerminated;
    default:
      return PodPhase::kUnknown;
  }
}

enum class PodConditionType : uint8_t {
  kUnknown = 0,
  kPodScheduled,
  kReady,
  kInitialized,
  kUnschedulable,
  kContainersReady
};
enum class ConditionStatus : uint8_t { kUnknown = 0, kTrue, kFalse };

inline PodConditionType ConvertToPodConditionType(
    px::shared::k8s::metadatapb::PodConditionType pb_enum) {
  using type_pb = px::shared::k8s::metadatapb::PodConditionType;
  switch (pb_enum) {
    case type_pb::POD_SCHEDULED:
      return PodConditionType::kPodScheduled;
    case type_pb::READY:
      return PodConditionType::kReady;
    case type_pb::INITIALIZED:
      return PodConditionType::kInitialized;
    case type_pb::UNSCHEDULABLE:
      return PodConditionType::kUnschedulable;
    case type_pb::CONTAINERS_READY:
      return PodConditionType::kContainersReady;
    default:
      return PodConditionType::kUnknown;
  }
}

inline ConditionStatus ConvertToConditionStatus(
    px::shared::k8s::metadatapb::ConditionStatus pb_enum) {
  using status_pb = px::shared::k8s::metadatapb::ConditionStatus;
  switch (pb_enum) {
    case status_pb::CONDITION_STATUS_TRUE:
      return ConditionStatus::kTrue;
    case status_pb::CONDITION_STATUS_FALSE:
      return ConditionStatus::kFalse;
    default:
      return ConditionStatus::kUnknown;
  }
}

using PodConditions = absl::flat_hash_map<PodConditionType, ConditionStatus>;

inline PodConditions ConvertToPodConditions(
    const google::protobuf::RepeatedPtrField<px::shared::k8s::metadatapb::PodCondition>&
        pod_conditions) {
  PodConditions conditions;
  for (const auto& condition : pod_conditions) {
    conditions.try_emplace(ConvertToPodConditionType(condition.type()),
                           ConvertToConditionStatus(condition.status()));
  }
  return conditions;
}

enum class ContainerType : uint8_t { kUnknown = 0, kDocker, kCRIO, kContainerd };

enum class ContainerState : uint8_t { kUnknown = 0, kRunning, kTerminated, kWaiting };

inline ContainerType ConvertToContainerType(px::shared::k8s::metadatapb::ContainerType pb_enum) {
  using state_pb = px::shared::k8s::metadatapb::ContainerType;
  switch (pb_enum) {
    case state_pb::CONTAINER_TYPE_DOCKER:
      return ContainerType::kDocker;
    case state_pb::CONTAINER_TYPE_CRIO:
      return ContainerType::kCRIO;
    case state_pb::CONTAINER_TYPE_CONTAINERD:
      return ContainerType::kContainerd;
    default:
      return ContainerType::kUnknown;
  }
}

inline ContainerState ConvertToContainerState(px::shared::k8s::metadatapb::ContainerState pb_enum) {
  using state_pb = px::shared::k8s::metadatapb::ContainerState;
  switch (pb_enum) {
    case state_pb::CONTAINER_STATE_RUNNING:
      return ContainerState::kRunning;
    case state_pb::CONTAINER_STATE_TERMINATED:
      return ContainerState::kTerminated;
    case state_pb::CONTAINER_STATE_WAITING:
      return ContainerState::kWaiting;
    case state_pb::CONTAINER_STATE_UNKNOWN:
    default:
      return ContainerState::kUnknown;
  }
}

/**
 * PodInfo contains information about K8s pods.
 */
class PodInfo : public K8sMetadataObject {
 public:
  PodInfo(UID uid, std::string_view ns, std::string_view name, PodQOSClass qos_class,
          PodPhase phase, PodConditions conditions, std::string_view phase_message,
          std::string_view phase_reason, std::string_view node_name, std::string_view hostname,
          std::string_view pod_ip, int64_t start_timestamp_ns = 0, int64_t stop_timestamp_ns = 0)
      : K8sMetadataObject(K8sObjectType::kPod, uid, ns, name, start_timestamp_ns,
                          stop_timestamp_ns),
        qos_class_(qos_class),
        phase_(phase),
        conditions_(conditions),
        phase_message_(phase_message),
        phase_reason_(phase_reason),
        node_name_(node_name),
        hostname_(hostname),
        pod_ip_(pod_ip) {}

  explicit PodInfo(const px::shared::k8s::metadatapb::PodUpdate& pod_update_info)
      : PodInfo(pod_update_info.uid(), pod_update_info.namespace_(), pod_update_info.name(),
                ConvertToPodQOsClass(pod_update_info.qos_class()),
                ConvertToPodPhase(pod_update_info.phase()),
                ConvertToPodConditions(pod_update_info.conditions()), pod_update_info.message(),
                pod_update_info.reason(), pod_update_info.node_name(), pod_update_info.hostname(),
                pod_update_info.pod_ip(), pod_update_info.start_timestamp_ns(),
                pod_update_info.stop_timestamp_ns()) {}

  virtual ~PodInfo() = default;

  void AddContainer(CIDView cid) { containers_.emplace(cid); }
  void RmContainer(CIDView cid) { containers_.erase(cid); }

  void AddService(UIDView uid) { services_.emplace(uid); }
  void RmService(UIDView uid) { services_.erase(uid); }

  PodQOSClass qos_class() const { return qos_class_; }
  PodPhase phase() const { return phase_; }
  void set_phase(PodPhase phase) { phase_ = phase; }

  PodConditions conditions() const { return conditions_; }
  void set_conditions(PodConditions conditions) { conditions_ = conditions; }

  const std::string& phase_message() const { return phase_message_; }
  void set_phase_message(std::string_view phase_message) { phase_message_ = phase_message; }

  const std::string& phase_reason() const { return phase_reason_; }
  void set_phase_reason(std::string_view phase_reason) { phase_reason_ = phase_reason; }

  void set_node_name(std::string_view node_name) { node_name_ = node_name; }
  void set_hostname(std::string_view hostname) { hostname_ = hostname; }
  void set_pod_ip(std::string_view pod_ip) { pod_ip_ = pod_ip; }
  void set_pod_labels(std::string labels) { labels_ = labels; }

  const std::string& node_name() const { return node_name_; }
  const std::string& hostname() const { return hostname_; }
  const std::string& pod_ip() const { return pod_ip_; }
  const std::string& labels() const { return labels_; }

  const absl::flat_hash_set<std::string>& containers() const { return containers_; }
  const absl::flat_hash_set<std::string>& services() const { return services_; }

  std::unique_ptr<K8sMetadataObject> Clone() const override {
    return std::unique_ptr<PodInfo>(new PodInfo(*this));
  }

  std::string DebugString(int indent = 0) const override;

 protected:
  PodInfo(const PodInfo& other) = default;
  PodInfo& operator=(const PodInfo& other) = delete;

 private:
  PodQOSClass qos_class_;
  PodPhase phase_;
  PodConditions conditions_;
  // The message for why the pod is in its current status.
  std::string phase_message_;
  // A brief CamelCase message indicating details about why the pod is in this state.
  std::string phase_reason_;
  /**
   * Set of containers that are running on this pod.
   *
   * The ContainerInformation is located in containers in the K8s state.
   */
  absl::flat_hash_set<CID> containers_;
  /**
   * The set of services that associate with this pod. K8s allows
   * multiple services from exposing the same pod.
   *
   * Should point to ServiceInfo via the data structure containing this pod.
   */
  absl::flat_hash_set<UID> services_;

  std::string node_name_;
  std::string hostname_;
  std::string pod_ip_;
  std::string labels_;
};

struct UPIDStartTSCompare {
  bool operator()(const UPID& a, const UPID& b) const {
    if (a.start_ts() != b.start_ts()) {
      return a.start_ts() < b.start_ts();
    }
    if (a.asid() != b.asid()) {
      return a.asid() < b.asid();
    }
    return a.pid() < b.pid();
  }
};

using StartTimeOrderedUPIDSet = absl::btree_set<UPID, UPIDStartTSCompare>;

/**
 * Store information about containers.
 *
 * Though this is not strictly a K8s object, it's state is tracked by K8s
 * so we include it here.
 */
class ContainerInfo {
 public:
  ContainerInfo() = delete;
  ContainerInfo(CID cid, std::string_view name, ContainerState state, ContainerType type,
                std::string_view state_message, std::string_view state_reason,
                int64_t start_time_ns, int64_t stop_time_ns = 0)
      : cid_(std::move(cid)),
        name_(std::string(name)),
        state_(state),
        type_(type),
        state_message_(state_message),
        state_reason_(state_reason),
        start_time_ns_(start_time_ns),
        stop_time_ns_(stop_time_ns) {}

  explicit ContainerInfo(const px::shared::k8s::metadatapb::ContainerUpdate& container_update_info)
      : ContainerInfo(container_update_info.cid(), container_update_info.name(),
                      ConvertToContainerState(container_update_info.container_state()),
                      ConvertToContainerType(container_update_info.container_type()),
                      container_update_info.message(), container_update_info.reason(),
                      container_update_info.start_timestamp_ns(),
                      container_update_info.stop_timestamp_ns()) {}

  const CID& cid() const { return cid_; }
  const std::string& name() const { return name_; }
  ContainerType type() const { return type_; }

  void set_pod_id(std::string_view pod_id) { pod_id_ = pod_id; }
  const UID& pod_id() const { return pod_id_; }

  const StartTimeOrderedUPIDSet& active_upids() const { return active_upids_; }
  StartTimeOrderedUPIDSet* mutable_active_upids() { return &active_upids_; }

  int64_t start_time_ns() const { return start_time_ns_; }

  int64_t stop_time_ns() const { return stop_time_ns_; }
  void set_stop_time_ns(int64_t stop_time_ns) { stop_time_ns_ = stop_time_ns; }

  ContainerState state() const { return state_; }
  void set_state(ContainerState state) { state_ = state; }

  const std::string& state_message() const { return state_message_; }
  void set_state_message(std::string_view state_message) { state_message_ = state_message; }

  const std::string& state_reason() const { return state_reason_; }
  void set_state_reason(std::string_view state_reason) { state_reason_ = state_reason; }

  std::unique_ptr<ContainerInfo> Clone() const {
    return std::unique_ptr<ContainerInfo>(new ContainerInfo(*this));
  }

  std::string DebugString(int indent = 0) const;

 protected:
  ContainerInfo(const ContainerInfo& other) = default;
  ContainerInfo& operator=(const ContainerInfo& other) = delete;

 private:
  const CID cid_;
  const std::string name_;
  UID pod_id_ = "";

  /**
   * The set of UPIDs that are running on this container.
   */
  StartTimeOrderedUPIDSet active_upids_;

  /**
   * Current state of the container, such as RUNNING, WAITING.
   */
  ContainerState state_;
  /**
   * Type of the container, such as DOCKER, CRIO.
   */
  ContainerType type_;
  // The message for why the container is in its current state.
  std::string state_message_;
  // A more detailed message for why the container is in its current state.
  std::string state_reason_;

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

/**
 * ServiceInfo contains information about K8s services.
 */
class ServiceInfo : public K8sMetadataObject {
 public:
  ServiceInfo(UID uid, std::string_view ns, std::string_view name)
      : K8sMetadataObject(K8sObjectType::kService, std::move(uid), std::move(ns), std::move(name)) {
  }
  virtual ~ServiceInfo() = default;

  std::unique_ptr<K8sMetadataObject> Clone() const override {
    return std::unique_ptr<ServiceInfo>(new ServiceInfo(*this));
  }

  std::string DebugString(int indent = 0) const override;

  void set_cluster_ip(std::string_view cluster_ip) { cluster_ip_ = cluster_ip; }
  const std::string& cluster_ip() const { return cluster_ip_; }

  void set_external_ips(const std::vector<std::string>& external_ips) {
    external_ips_ = external_ips;
  }
  const std::vector<std::string>& external_ips() const { return external_ips_; }

 protected:
  ServiceInfo(const ServiceInfo& other) = default;
  ServiceInfo& operator=(const ServiceInfo& other) = delete;

 private:
  std::string cluster_ip_;
  std::vector<std::string> external_ips_;
};

/**
 * NamespaceInfo contains information about K8s namespaces.
 */
class NamespaceInfo : public K8sMetadataObject {
 public:
  NamespaceInfo(UID uid, std::string_view ns, std::string_view name)
      : K8sMetadataObject(K8sObjectType::kNamespace, std::move(uid), std::move(ns),
                          std::move(name)) {}
  virtual ~NamespaceInfo() = default;

  std::unique_ptr<K8sMetadataObject> Clone() const override {
    return std::unique_ptr<NamespaceInfo>(new NamespaceInfo(*this));
  }

  std::string DebugString(int indent = 0) const override;

 protected:
  NamespaceInfo(const NamespaceInfo& other) = default;
  NamespaceInfo& operator=(const NamespaceInfo& other) = delete;
};

using ReplicaSetConditions = absl::flat_hash_map<std::string, ConditionStatus>;

inline ReplicaSetConditions ConvertToReplicaSetConditions(
    const google::protobuf::RepeatedPtrField<px::shared::k8s::metadatapb::ReplicaSetCondition>&
        replica_set_conditions) {
  ReplicaSetConditions conditions;
  for (const auto& condition : replica_set_conditions) {
    conditions.try_emplace(condition.type(), ConvertToConditionStatus(condition.status()));
  }
  return conditions;
}

/**
 * ReplicaSetInfo contains information about K8s replica sets.
 */
class ReplicaSetInfo : public K8sMetadataObject {
 public:
  ReplicaSetInfo(UID uid, std::string_view ns, std::string_view name, int32_t replicas,
                 int32_t fully_labeled_replicas, int32_t ready_replicas, int32_t available_replicas,
                 int32_t observed_generation, int32_t requested_replicas,
                 ReplicaSetConditions conditions, int64_t start_timestamp_ns = 0,
                 int64_t stop_timestamp_ns = 0)
      : K8sMetadataObject(K8sObjectType::kReplicaSet, uid, ns, name, start_timestamp_ns,
                          stop_timestamp_ns),
        replicas_(replicas),
        fully_labeled_replicas_(fully_labeled_replicas),
        ready_replicas_(ready_replicas),
        available_replicas_(available_replicas),
        observed_generation_(observed_generation),
        requested_replicas_(requested_replicas),
        conditions_(conditions) {}

  explicit ReplicaSetInfo(
      const px::shared::k8s::metadatapb::ReplicaSetUpdate& replica_set_update_info)
      : ReplicaSetInfo(replica_set_update_info.uid(), replica_set_update_info.namespace_(),
                       replica_set_update_info.name(), replica_set_update_info.replicas(),
                       replica_set_update_info.fully_labeled_replicas(),
                       replica_set_update_info.ready_replicas(),
                       replica_set_update_info.available_replicas(),
                       replica_set_update_info.observed_generation(),
                       replica_set_update_info.requested_replicas(),
                       ConvertToReplicaSetConditions(replica_set_update_info.conditions()),
                       replica_set_update_info.start_timestamp_ns(),
                       replica_set_update_info.stop_timestamp_ns()) {}

  virtual ~ReplicaSetInfo() = default;

  int32_t replicas() const { return replicas_; }
  int32_t fully_labeled_replicas() const { return fully_labeled_replicas_; }
  int32_t ready_replicas() const { return ready_replicas_; }
  int32_t available_replicas() const { return available_replicas_; }
  int32_t observed_generation() const { return observed_generation_; }
  int32_t requested_replicas() const { return requested_replicas_; }

  void set_replicas(int32_t replicas) { replicas_ = replicas; }
  void set_fully_labeled_replicas(int32_t fully_labeled_replicas) {
    fully_labeled_replicas_ = fully_labeled_replicas;
  }
  void set_ready_replicas(int32_t ready_replicas) { ready_replicas_ = ready_replicas; }
  void set_available_replicas(int32_t available_replicas) {
    available_replicas_ = available_replicas;
  }
  void set_observed_generation(int32_t observed_generation) {
    observed_generation_ = observed_generation;
  }
  void set_requested_replicas(int32_t requested_replicas) {
    requested_replicas_ = requested_replicas;
  }

  ReplicaSetConditions conditions() const { return conditions_; }
  void set_conditions(ReplicaSetConditions conditions) { conditions_ = conditions; }

  std::unique_ptr<K8sMetadataObject> Clone() const override {
    return std::unique_ptr<ReplicaSetInfo>(new ReplicaSetInfo(*this));
  }

  std::string DebugString(int indent = 0) const override;

 protected:
  ReplicaSetInfo(const ReplicaSetInfo& other) = default;
  ReplicaSetInfo& operator=(const ReplicaSetInfo& other) = delete;

 private:
  int32_t replicas_;
  int32_t fully_labeled_replicas_;
  int32_t ready_replicas_;
  int32_t available_replicas_;
  int32_t observed_generation_;
  int32_t requested_replicas_;
  ReplicaSetConditions conditions_;
};

enum class DeploymentConditionType : uint8_t {
  kTypeUnknown = 0,
  kAvailable,
  kProgressing,
  kReplicaFailure,
};

using DeploymentConditions = absl::flat_hash_map<DeploymentConditionType, ConditionStatus>;

inline DeploymentConditionType ConvertToDeploymentConditionType(
    px::shared::k8s::metadatapb::DeploymentConditionType condition_type) {
  using type_pb = px::shared::k8s::metadatapb::DeploymentConditionType;
  switch (condition_type) {
    case type_pb::DEPLOYMENT_CONDITION_AVAILABLE:
      return DeploymentConditionType::kAvailable;
    case type_pb::DEPLOYMENT_CONDITION_PROGRESSING:
      return DeploymentConditionType::kProgressing;
    case type_pb::DEPLOYMENT_CONDITION_REPLICA_FAILURE:
      return DeploymentConditionType::kReplicaFailure;
    default:
      return DeploymentConditionType::kTypeUnknown;
  }
}

inline DeploymentConditions ConvertToDeploymentConditions(
    const google::protobuf::RepeatedPtrField<px::shared::k8s::metadatapb::DeploymentCondition>&
        deployment_conditions) {
  DeploymentConditions conditions;
  for (const auto& condition : deployment_conditions) {
    conditions.try_emplace(ConvertToDeploymentConditionType(condition.type()),
                           ConvertToConditionStatus(condition.status()));
  }
  return conditions;
}

/**
 * DeploymentInfo contains information about K8s deployments.
 */
class DeploymentInfo : public K8sMetadataObject {
 public:
  DeploymentInfo(UID uid, std::string_view ns, std::string_view name, int32_t observed_generation,
                 int32_t replicas, int32_t updated_replicas, int32_t ready_replicas,
                 int32_t available_replicas, int32_t unavailable_replicas,
                 int32_t requested_replicas, DeploymentConditions conditions,
                 int64_t start_timestamp_ns = 0, int64_t stop_timestamp_ns = 0)
      : K8sMetadataObject(K8sObjectType::kDeployment, uid, ns, name, start_timestamp_ns,
                          stop_timestamp_ns),
        observed_generation_(observed_generation),
        replicas_(replicas),
        updated_replicas_(updated_replicas),
        ready_replicas_(ready_replicas),
        available_replicas_(available_replicas),
        unavailable_replicas_(unavailable_replicas),
        requested_replicas_(requested_replicas),
        conditions_(conditions) {}

  explicit DeploymentInfo(
      const px::shared::k8s::metadatapb::DeploymentUpdate& deployment_update_info)
      : DeploymentInfo(deployment_update_info.uid(), deployment_update_info.namespace_(),
                       deployment_update_info.name(), deployment_update_info.observed_generation(),
                       deployment_update_info.replicas(), deployment_update_info.updated_replicas(),
                       deployment_update_info.ready_replicas(),
                       deployment_update_info.available_replicas(),
                       deployment_update_info.unavailable_replicas(),
                       deployment_update_info.requested_replicas(),
                       ConvertToDeploymentConditions(deployment_update_info.conditions()),
                       deployment_update_info.start_timestamp_ns(),
                       deployment_update_info.stop_timestamp_ns()) {}

  virtual ~DeploymentInfo() = default;

  int32_t observed_generation() const { return observed_generation_; }
  int32_t replicas() const { return replicas_; }
  int32_t updated_replicas() const { return updated_replicas_; }
  int32_t ready_replicas() const { return ready_replicas_; }
  int32_t available_replicas() const { return available_replicas_; }
  int32_t unavailable_replicas() const { return unavailable_replicas_; }
  int32_t requested_replicas() const { return requested_replicas_; }

  void set_observed_generation(int32_t observed_generation) {
    observed_generation_ = observed_generation;
  }
  void set_replicas(int32_t replicas) { replicas_ = replicas; }
  void set_updated_replicas(int32_t updated_replicas) { updated_replicas_ = updated_replicas; }
  void set_ready_replicas(int32_t ready_replicas) { ready_replicas_ = ready_replicas; }
  void set_available_replicas(int32_t available_replicas) {
    available_replicas_ = available_replicas;
  }
  void set_unavailable_replicas(int32_t unavailable_replicas) {
    unavailable_replicas_ = unavailable_replicas;
  }

  void set_requested_replicas(int32_t requested_replicas) {
    requested_replicas_ = requested_replicas;
  }

  DeploymentConditions conditions() const { return conditions_; }
  void set_conditions(DeploymentConditions conditions) { conditions_ = conditions; }

  std::unique_ptr<K8sMetadataObject> Clone() const override {
    return std::unique_ptr<DeploymentInfo>(new DeploymentInfo(*this));
  }

  std::string DebugString(int indent = 0) const override;

 protected:
  DeploymentInfo(const DeploymentInfo& other) = default;
  DeploymentInfo& operator=(const DeploymentInfo& other) = delete;

 private:
  int32_t observed_generation_;
  int32_t replicas_;
  int32_t updated_replicas_;
  int32_t ready_replicas_;
  int32_t available_replicas_;
  int32_t unavailable_replicas_;
  int32_t requested_replicas_;
  DeploymentConditions conditions_;
};
}  // namespace md
}  // namespace px
