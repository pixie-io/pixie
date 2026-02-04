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

#include <memory>
#include <string>
#include <utility>

#include <absl/container/flat_hash_set.h>

#include "src/shared/metadata/metadata_state.h"

namespace px {
namespace md {

const K8sMetadataObject* K8sMetadataState::K8sMetadataObjectByID(UIDView id,
                                                                 K8sObjectType type) const {
  auto it = k8s_objects_by_id_.find(id);

  if (it == k8s_objects_by_id_.end()) {
    return nullptr;
  }

  if (it->second->type() != type) {
    return nullptr;
  }

  return it->second.get();
}

const PodInfo* K8sMetadataState::PodInfoByID(UIDView pod_id) const {
  auto type = K8sObjectType::kPod;
  return static_cast<const PodInfo*>(K8sMetadataObjectByID(pod_id, type));
}

const ServiceInfo* K8sMetadataState::ServiceInfoByID(UIDView service_id) const {
  auto type = K8sObjectType::kService;
  return static_cast<const ServiceInfo*>(K8sMetadataObjectByID(service_id, type));
}

const NamespaceInfo* K8sMetadataState::NamespaceInfoByID(UIDView ns_id) const {
  auto type = K8sObjectType::kNamespace;
  return static_cast<const NamespaceInfo*>(K8sMetadataObjectByID(ns_id, type));
}

const ReplicaSetInfo* K8sMetadataState::ReplicaSetInfoByID(UIDView replica_set_id) const {
  auto type = K8sObjectType::kReplicaSet;
  return static_cast<const ReplicaSetInfo*>(K8sMetadataObjectByID(replica_set_id, type));
}

const DeploymentInfo* K8sMetadataState::DeploymentInfoByID(UIDView deployment_id) const {
  auto type = K8sObjectType::kDeployment;

  return static_cast<const DeploymentInfo*>(K8sMetadataObjectByID(deployment_id, type));
}

const ContainerInfo* K8sMetadataState::ContainerInfoByID(CIDView id) const {
  auto it = containers_by_id_.find(id);

  if (it == containers_by_id_.end()) {
    return nullptr;
  }

  return it->second.get();
}

UID K8sMetadataState::PodIDByName(K8sNameIdentView pod_name) const {
  auto it = pods_by_name_.find(pod_name);
  return (it == pods_by_name_.end()) ? "" : it->second;
}

UID K8sMetadataState::PodIDByIP(std::string_view pod_ip) const {
  auto it = pods_by_ip_.find(pod_ip);
  return (it == pods_by_ip_.end()) ? "" : it->second;
}

UID K8sMetadataState::PodIDByIPAtTime(std::string_view pod_ip, int64_t ts) const {
  auto it = pods_by_ip_and_start_time_.find(pod_ip);
  if (it == pods_by_ip_and_start_time_.end()) {
    return "";
  }
  auto up = it->second.upper_bound({"", ts});
  if (up == it->second.begin()) {
    return "";
  }
  --up;
  return up->first;
}

UID K8sMetadataState::ServiceIDByClusterIP(std::string_view cluster_ip) const {
  auto it = services_by_cluster_ip_.find(cluster_ip);
  return (it == services_by_cluster_ip_.end()) ? "" : it->second;
}

CID K8sMetadataState::ContainerIDByName(std::string_view container_name) const {
  auto it = containers_by_name_.find(container_name);
  return (it == containers_by_name_.end()) ? "" : it->second;
}

UID K8sMetadataState::ServiceIDByName(K8sNameIdentView service_name) const {
  auto it = services_by_name_.find(service_name);
  return (it == services_by_name_.end()) ? "" : it->second;
}

UID K8sMetadataState::NamespaceIDByName(K8sNameIdentView namespace_name) const {
  auto it = namespaces_by_name_.find(namespace_name);
  return (it == namespaces_by_name_.end()) ? "" : it->second;
}

UID K8sMetadataState::ReplicaSetIDByName(K8sNameIdentView replica_set_name) const {
  auto it = replica_sets_by_name_.find(replica_set_name);
  return (it == replica_sets_by_name_.end()) ? "" : it->second;
}

UID K8sMetadataState::DeploymentIDByName(K8sNameIdentView deployment_name) const {
  auto it = deployments_by_name_.find(deployment_name);
  return (it == deployments_by_name_.end()) ? "" : it->second;
}

const ReplicaSetInfo* K8sMetadataState::OwnerReplicaSetInfo(
    const K8sMetadataObject* obj_info) const {
  for (const auto& owner_reference : obj_info->owner_references()) {
    if (owner_reference.kind != "ReplicaSet") {
      continue;
    }
    auto rs_info = ReplicaSetInfoByID(owner_reference.uid);
    if (rs_info != nullptr) {
      return rs_info;
    }
  }
  return nullptr;
}

const DeploymentInfo* K8sMetadataState::OwnerDeploymentInfo(
    const K8sMetadataObject* obj_info) const {
  for (const auto& owner_reference : obj_info->owner_references()) {
    if (owner_reference.kind != "Deployment") {
      continue;
    }
    auto dep_info = DeploymentInfoByID(owner_reference.uid);
    if (dep_info != nullptr) {
      return dep_info;
    }
  }
  return nullptr;
}

std::unique_ptr<K8sMetadataState> K8sMetadataState::Clone() const {
  auto other = std::make_unique<K8sMetadataState>();

  other->pod_cidrs_ = pod_cidrs_;
  other->service_cidr_ = service_cidr_;

  other->k8s_objects_by_id_.reserve(k8s_objects_by_id_.size());
  for (const auto& [k, v] : k8s_objects_by_id_) {
    other->k8s_objects_by_id_[k] = v->Clone();
  }

  other->containers_by_id_.reserve(containers_by_id_.size());
  for (const auto& [k, v] : containers_by_id_) {
    other->containers_by_id_[k] = v->Clone();
  }

  other->pods_by_name_ = pods_by_name_;
  other->services_by_name_ = services_by_name_;
  other->namespaces_by_name_ = namespaces_by_name_;
  other->replica_sets_by_name_ = replica_sets_by_name_;
  other->deployments_by_name_ = deployments_by_name_;
  other->containers_by_name_ = containers_by_name_;
  other->pods_by_ip_ = pods_by_ip_;
  other->pods_by_ip_and_start_time_ = pods_by_ip_and_start_time_;
  other->services_by_cluster_ip_ = services_by_cluster_ip_;

  return other;
}

std::string K8sMetadataState::DebugString(int indent_level) const {
  std::string str;
  std::string prefix = Indent(indent_level);

  str += prefix + "K8s Objects:\n";
  for (const auto& it : k8s_objects_by_id_) {
    str += absl::Substitute("$0\n", it.second->DebugString(indent_level + 1));
  }
  str += "\n";
  str += prefix + "Containers:\n";
  for (const auto& it : containers_by_id_) {
    str += absl::Substitute("$0\n", it.second->DebugString(indent_level + 1));
  }
  str += "\n";
  str += prefix + "Name Based Maps:\n";
  for (const auto& [k, v] : namespaces_by_name_) {
    str += absl::Substitute("namespace_id: $0, ns: $1, name: $2\n", v, k.first, k.second);
  }
  for (const auto& [k, v] : pods_by_name_) {
    str += absl::Substitute("pod_id: $0, ns: $1, name: $2\n", v, k.first, k.second);
  }
  for (const auto& [k, v] : services_by_name_) {
    str += absl::Substitute("service_id: $0, ns: $1, name: $2\n", v, k.first, k.second);
  }
  for (const auto& [k, v] : replica_sets_by_name_) {
    str += absl::Substitute("replicaset_id: $0, ns: $1, name: $2\n", v, k.first, k.second);
  }
  for (const auto& [k, v] : deployments_by_name_) {
    str += absl::Substitute("deployment_id: $0, ns: $1, name: $2\n", v, k.first, k.second);
  }
  for (const auto& [k, v] : containers_by_name_) {
    str += absl::Substitute("cid: $0, name: $1\n", v, k);
  }
  str += "\n";
  str += prefix + "IPs By Time:\n";
  for (const auto& [k, v] : pods_by_ip_and_start_time_) {
    str += absl::Substitute("ip: $0\n", k);
    for (const auto& [id, ts] : v) {
      str += absl::Substitute("\tpod_id: $0, start_time: $1\n", id, ts);
    }
  }
  str += "\n";
  str += prefix + "IPs:\n";
  for (const auto& [k, v] : pods_by_ip_) {
    str += absl::Substitute("pod_id: $0, ip: $1\n", v, k);
  }
  for (const auto& [k, v] : services_by_cluster_ip_) {
    str += absl::Substitute("service_id: $0, cluster_ip: $1\n", v, k);
  }
  str += prefix + absl::Substitute("PodCIDRs($0): ", pod_cidrs_.size());
  for (const auto& cidr : pod_cidrs_) {
    str += absl::Substitute("$0,", ToString(cidr));
  }
  str += "\n";

  return str;
}

Status K8sMetadataState::HandlePodUpdate(const PodUpdate& update) {
  const UID& object_uid = update.uid();
  const std::string& name = update.name();
  const std::string& ns = update.namespace_();

  auto it = k8s_objects_by_id_.find(object_uid);
  if (it == k8s_objects_by_id_.end()) {
    auto pod = std::make_unique<PodInfo>(update);
    VLOG(1) << "Adding Pod: " << pod->DebugString();
    it = k8s_objects_by_id_.try_emplace(object_uid, std::move(pod)).first;
  }
  auto pod_info = static_cast<PodInfo*>(it->second.get());

  // We always just add to the container set even if the container is stopped.
  // We expect all cleanup to happen periodically to allow stale objects to be queried for some
  // time. Also, because we expect eventual consistency container ID may or may not be available
  // before the container state is available. Upstream code using this needs to be aware that the
  // state might be periodically inconsistent.

  for (const auto& cid : update.container_ids()) {
    if (containers_by_id_.find(cid) == containers_by_id_.end()) {
      // We should be resilient to the case where we happened to miss a pod update
      // in the stream of events. If we did miss a pod update, just skip adding the
      // pod to this particular service to avoid dangling references.
      LOG(INFO) << absl::Substitute("Didn't find container ID $0 for pod $1/$2", cid, ns, name);
      continue;
    }

    pod_info->AddContainer(cid);
    containers_by_id_[cid]->set_pod_id(object_uid);
  }

  for (const auto& owner_ref : update.owner_references()) {
    pod_info->AddOwnerReference(owner_ref.uid(), owner_ref.name(), owner_ref.kind());
  }

  pod_info->set_start_time_ns(update.start_timestamp_ns());
  pod_info->set_stop_time_ns(update.stop_timestamp_ns());
  pod_info->set_node_name(update.node_name());
  pod_info->set_hostname(update.hostname());
  pod_info->set_pod_ip(update.pod_ip());
  pod_info->set_phase(ConvertToPodPhase(update.phase()));
  pod_info->set_conditions(ConvertToPodConditions(update.conditions()));
  pod_info->set_phase_message(update.message());
  pod_info->set_phase_reason(update.reason());
  pod_info->set_pod_labels(update.labels());

  pods_by_name_[{ns, name}] = object_uid;
  // Filter out daemonsets which don't have their own, unique podIP.
  if (update.host_ip() != update.pod_ip() && update.pod_ip() != "") {
    pods_by_ip_[update.pod_ip()] = object_uid;
    if (update.start_timestamp_ns() > 0) {
      pods_by_ip_and_start_time_[update.pod_ip()].insert({object_uid, update.start_timestamp_ns()});
    }
  }

  return Status::OK();
}

Status K8sMetadataState::HandleContainerUpdate(const ContainerUpdate& update) {
  const CID& cid = update.cid();

  auto it = containers_by_id_.find(cid);
  if (it == containers_by_id_.end()) {
    auto container = std::make_unique<ContainerInfo>(update);
    VLOG(1) << "Adding Container: " << container->DebugString();
    it = containers_by_id_.try_emplace(cid, std::move(container)).first;
  }
  VLOG(1) << "container update: " << update.name();

  auto* container_info = it->second.get();
  container_info->set_stop_time_ns(update.stop_timestamp_ns());
  container_info->set_state(ConvertToContainerState(update.container_state()));
  container_info->set_state_message(update.message());
  container_info->set_state_reason(update.reason());

  containers_by_name_[update.name()] = cid;

  return Status::OK();
}

Status K8sMetadataState::HandleServiceUpdate(const ServiceUpdate& update) {
  const UID& service_uid = update.uid();
  const std::string& name = update.name();
  const std::string& ns = update.namespace_();

  auto it = k8s_objects_by_id_.find(service_uid);
  if (it == k8s_objects_by_id_.end()) {
    auto service = std::make_unique<ServiceInfo>(service_uid, ns, name);
    VLOG(1) << "Adding Service: " << service->DebugString();
    it = k8s_objects_by_id_.try_emplace(service_uid, std::move(service)).first;
  }
  auto service_info = static_cast<ServiceInfo*>(it->second.get());

  for (const auto& uid : update.pod_ids()) {
    if (k8s_objects_by_id_.find(uid) == k8s_objects_by_id_.end()) {
      // We should be resilient to the case where we happened to miss a pod update
      // in the stream of events. If we did miss a pod update, just skip adding the
      // pod to this particular service to avoid dangling references.
      LOG(INFO) << absl::Substitute("Didn't find pod UID $0 for service $1/$2", uid, ns, name);
      continue;
    }
    ECHECK(k8s_objects_by_id_[uid]->type() == K8sObjectType::kPod);
    // We add the service uid to the pod. Lifetime of service still handled by the service object.
    PodInfo* pod_info = static_cast<PodInfo*>(k8s_objects_by_id_[uid].get());
    pod_info->AddService(service_uid);
  }
  if (update.start_timestamp_ns() != 0) {
    service_info->set_start_time_ns(update.start_timestamp_ns());
  }
  if (update.stop_timestamp_ns() != 0) {
    service_info->set_stop_time_ns(update.stop_timestamp_ns());
  }
  if (update.cluster_ip() != "") {
    services_by_cluster_ip_[update.cluster_ip()] = service_uid;
    service_info->set_cluster_ip(update.cluster_ip());
  }
  if (update.external_ips().size()) {
    std::vector<std::string> external_ips(update.external_ips().begin(),
                                          update.external_ips().end());
    service_info->set_external_ips(external_ips);
  }

  VLOG(1) << "service update: " << update.name();
  services_by_name_[{ns, name}] = service_uid;
  return Status::OK();
}

Status K8sMetadataState::HandleNamespaceUpdate(const NamespaceUpdate& update) {
  const UID& namespace_uid = update.uid();
  const std::string& name = update.name();
  const std::string& ns = update.name();

  auto it = k8s_objects_by_id_.find(namespace_uid);
  if (it == k8s_objects_by_id_.end()) {
    auto ns_obj = std::make_unique<NamespaceInfo>(namespace_uid, ns, name);
    VLOG(1) << "Adding Namespace: " << ns_obj->DebugString();
    it = k8s_objects_by_id_.try_emplace(namespace_uid, std::move(ns_obj)).first;
  }
  auto ns_info = static_cast<NamespaceInfo*>(it->second.get());

  ns_info->set_start_time_ns(update.start_timestamp_ns());
  ns_info->set_stop_time_ns(update.stop_timestamp_ns());

  VLOG(1) << "namespace update: " << update.name();

  namespaces_by_name_[{ns, name}] = namespace_uid;
  return Status::OK();
}

Status K8sMetadataState::HandleNodeUpdate(const NodeUpdate& update) {
  // We currently do not use node updates in the PEM.
  VLOG(1) << "node update: " << update.name();

  return Status::OK();
}

Status K8sMetadataState::HandleReplicaSetUpdate(const ReplicaSetUpdate& update) {
  const UID& replica_set_uid = update.uid();
  const std::string& name = update.name();
  const std::string& ns = update.namespace_();

  auto it = k8s_objects_by_id_.find(replica_set_uid);
  if (it == k8s_objects_by_id_.end()) {
    auto replica_set = std::make_unique<ReplicaSetInfo>(update);
    VLOG(1) << "Adding ReplicaSet: " << replica_set->DebugString();
    it = k8s_objects_by_id_.try_emplace(replica_set_uid, std::move(replica_set)).first;
  }
  auto replica_set_info = static_cast<ReplicaSetInfo*>(it->second.get());

  for (const auto& owner_ref : update.owner_references()) {
    replica_set_info->AddOwnerReference(owner_ref.uid(), owner_ref.name(), owner_ref.kind());
  }

  replica_set_info->set_start_time_ns(update.start_timestamp_ns());
  replica_set_info->set_stop_time_ns(update.stop_timestamp_ns());
  replica_set_info->set_conditions(ConvertToReplicaSetConditions(update.conditions()));
  replica_set_info->set_replicas(update.replicas());
  replica_set_info->set_fully_labeled_replicas(update.fully_labeled_replicas());
  replica_set_info->set_ready_replicas(update.ready_replicas());
  replica_set_info->set_available_replicas(update.available_replicas());
  replica_set_info->set_observed_generation(update.observed_generation());
  replica_set_info->set_requested_replicas(update.requested_replicas());

  VLOG(1) << "replica set update: " << update.name();

  replica_sets_by_name_[{ns, name}] = replica_set_uid;
  return Status::OK();
}

Status K8sMetadataState::HandleDeploymentUpdate(const DeploymentUpdate& update) {
  const UID& deployment_uid = update.uid();
  const std::string& name = update.name();
  const std::string& ns = update.namespace_();

  auto it = k8s_objects_by_id_.find(deployment_uid);
  if (it == k8s_objects_by_id_.end()) {
    auto deployment = std::make_unique<DeploymentInfo>(update);
    VLOG(1) << "Adding Deployment: " << deployment->DebugString();
    it = k8s_objects_by_id_.try_emplace(deployment_uid, std::move(deployment)).first;
  }
  auto deployment_info = static_cast<DeploymentInfo*>(it->second.get());

  deployment_info->set_start_time_ns(update.start_timestamp_ns());
  deployment_info->set_stop_time_ns(update.stop_timestamp_ns());
  deployment_info->set_observed_generation(update.observed_generation());
  deployment_info->set_replicas(update.replicas());
  deployment_info->set_updated_replicas(update.updated_replicas());
  deployment_info->set_ready_replicas(update.ready_replicas());
  deployment_info->set_available_replicas(update.available_replicas());
  deployment_info->set_unavailable_replicas(update.unavailable_replicas());
  deployment_info->set_requested_replicas(update.requested_replicas());
  deployment_info->set_conditions(ConvertToDeploymentConditions(update.conditions()));

  VLOG(1) << "deployment update: " << update.name();

  deployments_by_name_[{ns, name}] = deployment_uid;
  return Status::OK();
}

template <typename T>
bool IsExpired(const T& obj, int64_t retention_time, int64_t now) {
  if (obj.stop_time_ns() == 0) {
    // Stop time of zero means it has not stopped yet.
    return false;
  }

  int64_t expiry_time = obj.stop_time_ns() + retention_time;
  return now > expiry_time;
}

Status K8sMetadataState::CleanupExpiredMetadata(int64_t now, int64_t retention_time_ns) {
  for (auto iter = k8s_objects_by_id_.begin(); iter != k8s_objects_by_id_.end();) {
    const auto& k8s_object = iter->second;

    if (!IsExpired(*k8s_object, retention_time_ns, now)) {
      ++iter;
      continue;
    }

    switch (k8s_object->type()) {
      case K8sObjectType::kPod: {
        if (PodIDByName(std::make_pair(k8s_object->ns(), k8s_object->name())) ==
            k8s_object->uid()) {
          pods_by_name_.erase({k8s_object->ns(), k8s_object->name()});
        }
        auto pod_ip = static_cast<PodInfo*>(k8s_object.get())->pod_ip();
        // There could be a new pod assigned to the podIP now, we should only
        // delete the IP from the map if it belongs to the terminated pod.
        if (PodIDByIP(pod_ip) == k8s_object->uid()) {
          pods_by_ip_.erase(pod_ip);
        }

        auto it = pods_by_ip_and_start_time_.find(pod_ip);
        if (it != pods_by_ip_and_start_time_.end()) {
          auto& pod_set = it->second;
          auto erase_end = pod_set.upper_bound({"", now - retention_time_ns});

          if (erase_end != pod_set.begin()) {
            // Check if the last pod we might erase is still running.
            // If the last of the pods being erased is running though it's
            // before the expiration time, leave it alone.
            auto prev_obj = k8s_objects_by_id_.find(std::prev(erase_end)->first);
            if (prev_obj != k8s_objects_by_id_.end()) {
              auto prev_pod = static_cast<PodInfo*>(prev_obj->second.get());
              if (prev_pod->phase() == PodPhase::kRunning || prev_pod->stop_time_ns() == 0) {
                --erase_end;
              }
            }
          }
          pod_set.erase(pod_set.begin(), erase_end);
        }
        break;
      }
      case K8sObjectType::kNamespace:
        if (NamespaceIDByName(std::make_pair(k8s_object->ns(), k8s_object->name())) ==
            k8s_object->uid()) {
          namespaces_by_name_.erase({k8s_object->ns(), k8s_object->name()});
        }
        break;
      case K8sObjectType::kService:
        if (ServiceIDByName(std::make_pair(k8s_object->ns(), k8s_object->name())) ==
            k8s_object->uid()) {
          services_by_name_.erase({k8s_object->ns(), k8s_object->name()});
        }
        break;
      case K8sObjectType::kReplicaSet:
        if (ReplicaSetIDByName(std::make_pair(k8s_object->ns(), k8s_object->name())) ==
            k8s_object->uid()) {
          replica_sets_by_name_.erase({k8s_object->ns(), k8s_object->name()});
        }
        break;
      case K8sObjectType::kDeployment:
        if (DeploymentIDByName(std::make_pair(k8s_object->ns(), k8s_object->name())) ==
            k8s_object->uid()) {
          deployments_by_name_.erase({k8s_object->ns(), k8s_object->name()});
        }
        break;
      default:
        LOG(DFATAL) << absl::Substitute("Unexpected object type: $0",
                                        static_cast<int>(k8s_object->type()));
    }

    k8s_objects_by_id_.erase(iter++);
  }

  for (auto iter = containers_by_id_.begin(); iter != containers_by_id_.end();) {
    const auto& cinfo = iter->second;

    if (!IsExpired(*cinfo, retention_time_ns, now)) {
      ++iter;
      continue;
    }

    containers_by_name_.erase(cinfo->name());
    containers_by_id_.erase(iter++);
  }

  return Status::OK();
}

std::shared_ptr<AgentMetadataState> AgentMetadataState::CloneToShared() const {
  auto state =
      std::make_shared<AgentMetadataState>(hostname_, asid_, pid_, agent_id_, pod_name_, vizier_id_,
                                           vizier_name_, vizier_namespace_, time_system_);
  state->last_update_ts_ns_ = last_update_ts_ns_;
  state->epoch_id_ = epoch_id_;
  state->k8s_metadata_state_ = k8s_metadata_state_->Clone();
  state->pids_by_upid_.reserve(pids_by_upid_.size());
  for (const auto& [k, v] : pids_by_upid_) {
    state->pids_by_upid_[k] = v->Clone();
  }
  state->upids_ = upids_;
  return state;
}

std::string AgentMetadataState::DebugString(int indent_level) const {
  std::string str;
  std::string prefix = Indent(indent_level);
  str += prefix + "--------------------------------------------\n";
  str += prefix + "Agent Metadata State:\n";
  str += prefix + absl::Substitute("ASID: $0\n", asid_);
  str += prefix + absl::Substitute("PID: $0\n", pid_);
  str += prefix + absl::Substitute("EpochID: $0\n", epoch_id_);
  str += prefix + absl::Substitute("LastUpdateTS: $0\n", last_update_ts_ns_);
  str += prefix + k8s_metadata_state_->DebugString(indent_level);
  str += prefix + absl::Substitute("PIDS($0)\n", pids_by_upid_.size());
  for (const auto& [upid, upid_info] : pids_by_upid_) {
    str += prefix + absl::Substitute("$0\n", upid_info->DebugString());
  }

  str += prefix + "\n";
  str += prefix + "--------------------------------------------\n";
  return str;
}

}  // namespace md
}  // namespace px
