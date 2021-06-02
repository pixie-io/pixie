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

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include "src/common/base/base.h"
#include "src/shared/k8s/metadatapb/metadata.pb.h"
#include "src/shared/upid/upid.h"

namespace px {
namespace md {

/**
 * Enum with all the different metadata types.
 */
enum class K8sObjectType { kUnknown, kPod, kService, kNamespace };

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

enum class PodPhase : uint8_t { kUnknown = 0, kPending, kRunning, kSucceeded, kFailed };

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
enum class PodConditionStatus : uint8_t { kUnknown = 0, kTrue, kFalse };

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

inline PodConditionStatus ConvertToPodConditionStatus(
    px::shared::k8s::metadatapb::PodConditionStatus pb_enum) {
  using status_pb = px::shared::k8s::metadatapb::PodConditionStatus;
  switch (pb_enum) {
    case status_pb::STATUS_TRUE:
      return PodConditionStatus::kTrue;
    case status_pb::STATUS_FALSE:
      return PodConditionStatus::kFalse;
    default:
      return PodConditionStatus::kUnknown;
  }
}

using PodConditions = absl::flat_hash_map<PodConditionType, PodConditionStatus>;

inline PodConditions ConvertToPodConditions(
    const google::protobuf::RepeatedPtrField<px::shared::k8s::metadatapb::PodCondition>&
        pod_conditions) {
  PodConditions conditions;
  for (const auto& condition : pod_conditions) {
    conditions.try_emplace(ConvertToPodConditionType(condition.type()),
                           ConvertToPodConditionStatus(condition.status()));
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
  const std::string& node_name() const { return node_name_; }
  const std::string& hostname() const { return hostname_; }
  const std::string& pod_ip() const { return pod_ip_; }

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

  const absl::flat_hash_set<UPID>& active_upids() const { return active_upids_; }
  absl::flat_hash_set<UPID>* mutable_active_upids() { return &active_upids_; }

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
  absl::flat_hash_set<UPID> active_upids_;

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

 protected:
  ServiceInfo(const ServiceInfo& other) = default;
  ServiceInfo& operator=(const ServiceInfo& other) = delete;
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

}  // namespace md
}  // namespace px
