#include <memory>
#include <string>
#include <utility>

#include <absl/container/flat_hash_set.h>

#include "src/shared/metadata/metadata_state.h"

namespace px {
namespace md {

const PodInfo* K8sMetadataState::PodInfoByID(UIDView pod_id) const {
  auto it = k8s_objects_.find(pod_id);

  if (it == k8s_objects_.end()) {
    return nullptr;
  }

  return (it->second->type() != K8sObjectType::kPod) ? nullptr
                                                     : static_cast<PodInfo*>(it->second.get());
}

const ServiceInfo* K8sMetadataState::ServiceInfoByID(UIDView service_id) const {
  auto it = k8s_objects_.find(service_id);

  if (it == k8s_objects_.end()) {
    return nullptr;
  }

  return (it->second->type() != K8sObjectType::kService)
             ? nullptr
             : static_cast<ServiceInfo*>(it->second.get());
}

const NamespaceInfo* K8sMetadataState::NamespaceInfoByID(UIDView ns_id) const {
  auto it = k8s_objects_.find(ns_id);

  if (it == k8s_objects_.end()) {
    return nullptr;
  }

  return (it->second->type() != K8sObjectType::kNamespace)
             ? nullptr
             : static_cast<NamespaceInfo*>(it->second.get());
}

UID K8sMetadataState::PodIDByName(K8sNameIdentView pod_name) const {
  auto it = pods_by_name_.find(pod_name);
  return (it == pods_by_name_.end()) ? "" : it->second;
}

UID K8sMetadataState::PodIDByIP(std::string_view pod_ip) const {
  auto it = pods_by_ip_.find(pod_ip);
  return (it == pods_by_ip_.end()) ? "" : it->second;
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

const ContainerInfo* K8sMetadataState::ContainerInfoByID(CIDView id) const {
  auto it = containers_by_id_.find(id);
  return it == containers_by_id_.end() ? nullptr : it->second.get();
}

std::unique_ptr<K8sMetadataState> K8sMetadataState::Clone() const {
  auto other = std::make_unique<K8sMetadataState>();

  other->pod_cidrs_ = pod_cidrs_;
  other->service_cidr_ = service_cidr_;

  other->k8s_objects_.reserve(k8s_objects_.size());
  for (const auto& [k, v] : k8s_objects_) {
    other->k8s_objects_[k] = v->Clone();
  }

  other->pods_by_name_.reserve(pods_by_name_.size());
  for (const auto& [k, v] : pods_by_name_) {
    other->pods_by_name_[k] = v;
  }

  other->pods_by_ip_.reserve(pods_by_ip_.size());
  for (const auto& [k, v] : pods_by_ip_) {
    other->pods_by_ip_[k] = v;
  }

  other->containers_by_id_.reserve(containers_by_id_.size());
  for (const auto& [k, v] : containers_by_id_) {
    other->containers_by_id_[k] = v->Clone();
  }

  other->services_by_name_.reserve(services_by_name_.size());
  for (const auto& [k, v] : services_by_name_) {
    other->services_by_name_[k] = v;
  }

  other->namespaces_by_name_.reserve(namespaces_by_name_.size());
  for (const auto& [k, v] : namespaces_by_name_) {
    other->namespaces_by_name_[k] = v;
  }

  other->containers_by_name_.reserve(containers_by_name_.size());
  for (const auto& [k, v] : containers_by_name_) {
    other->containers_by_name_[k] = v;
  }
  return other;
}

std::string K8sMetadataState::DebugString(int indent_level) const {
  std::string str;
  std::string prefix = Indent(indent_level);

  str += prefix + "K8s Objects:\n";
  for (const auto& it : k8s_objects_) {
    str += absl::Substitute("$0\n", it.second->DebugString(indent_level + 1));
  }
  str += "\n";
  str += prefix + "Containers:\n";
  for (const auto& it : containers_by_id_) {
    str += absl::Substitute("$0\n", it.second->DebugString(indent_level + 1));
  }
  str += "\n";
  str += prefix + "IPs:\n";
  for (const auto& [k, v] : pods_by_ip_) {
    str += absl::Substitute("pod_id: $0, ip: $1\n", v, k);
  }

  str += prefix + absl::Substitute("PodCIDRs($0): ", pod_cidrs_.size());
  for (const auto& cidr : pod_cidrs_) {
    str += absl::Substitute("$0,", ToString(cidr));
  }
  str += "\n";

  return str;
}

Status K8sMetadataState::HandlePodUpdate(const PodUpdate& update) {
  const auto& object_uid = update.uid();
  const std::string& name = update.name();
  const std::string& ns = update.namespace_();

  auto it = k8s_objects_.find(object_uid);
  if (it == k8s_objects_.end()) {
    auto pod = std::make_unique<PodInfo>(update);
    VLOG(1) << "Adding Pod: " << pod->DebugString();
    it = k8s_objects_.try_emplace(object_uid, std::move(pod)).first;
  }

  // We always just add to the container set even if the container is stopped.
  // We expect all cleanup to happen periodically to allow stale objects to be queried for some
  // time. Also, because we expect eventual consistency container ID may or may not be available
  // before the container state is available. Upstream code using this needs to be aware that the
  // state might be periodically inconsistent.
  auto pod_info = static_cast<PodInfo*>(it->second.get());
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

  pod_info->set_start_time_ns(update.start_timestamp_ns());
  pod_info->set_stop_time_ns(update.stop_timestamp_ns());
  pod_info->set_node_name(update.node_name());
  pod_info->set_hostname(update.hostname());
  pod_info->set_pod_ip(update.pod_ip());
  pod_info->set_phase(ConvertToPodPhase(update.phase()));
  pod_info->set_conditions(ConvertToPodConditions(update.conditions()));
  pod_info->set_phase_message(update.message());
  pod_info->set_phase_reason(update.reason());

  pods_by_name_[{ns, name}] = object_uid;
  if (update.host_ip() !=
      update.pod_ip()) {  // Filter out daemonset which don't have their own, unique podIP.
    pods_by_ip_[update.pod_ip()] = object_uid;
  }

  return Status::OK();
}

Status K8sMetadataState::HandleContainerUpdate(const ContainerUpdate& update) {
  const auto& cid = update.cid();

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
  const auto& service_uid = update.uid();
  const std::string& name = update.name();
  const std::string& ns = update.namespace_();

  auto it = k8s_objects_.find(service_uid);
  if (it == k8s_objects_.end()) {
    auto service = std::make_unique<ServiceInfo>(service_uid, ns, name);
    VLOG(1) << "Adding Service: " << service->DebugString();
    it = k8s_objects_.try_emplace(service_uid, std::move(service)).first;
  }

  auto service_info = static_cast<ServiceInfo*>(it->second.get());
  for (const auto& uid : update.pod_ids()) {
    if (k8s_objects_.find(uid) == k8s_objects_.end()) {
      // We should be resilient to the case where we happened to miss a pod update
      // in the stream of events. If we did miss a pod update, just skip adding the
      // pod to this particular service to avoid dangling references.
      LOG(INFO) << absl::Substitute("Didn't find pod UID $0 for service $1/$2", uid, ns, name);
      continue;
    }
    ECHECK(k8s_objects_[uid]->type() == K8sObjectType::kPod);
    // We add the service uid to the pod. Lifetime of service still handled by the service object.
    PodInfo* pod_info = static_cast<PodInfo*>(k8s_objects_[uid].get());
    pod_info->AddService(service_uid);
  }
  service_info->set_start_time_ns(update.start_timestamp_ns());
  service_info->set_stop_time_ns(update.stop_timestamp_ns());

  VLOG(1) << "service update: " << update.name();

  services_by_name_[{ns, name}] = service_uid;
  return Status::OK();
}

Status K8sMetadataState::HandleNamespaceUpdate(const NamespaceUpdate& update) {
  const auto& namespace_uid = update.uid();
  const std::string& name = update.name();
  const std::string& ns = update.name();

  auto it = k8s_objects_.find(namespace_uid);
  if (it == k8s_objects_.end()) {
    auto ns_obj = std::make_unique<NamespaceInfo>(namespace_uid, ns, name);
    VLOG(1) << "Adding Namespace: " << ns_obj->DebugString();
    it = k8s_objects_.try_emplace(namespace_uid, std::move(ns_obj)).first;
  }

  auto ns_info = static_cast<NamespaceInfo*>(it->second.get());

  ns_info->set_start_time_ns(update.start_timestamp_ns());
  ns_info->set_stop_time_ns(update.stop_timestamp_ns());

  VLOG(1) << "namespace update: " << update.name();

  namespaces_by_name_[{ns, name}] = namespace_uid;
  return Status::OK();
}

std::shared_ptr<AgentMetadataState> AgentMetadataState::CloneToShared() const {
  auto state = std::make_shared<AgentMetadataState>(hostname_, asid_, agent_id_, pod_name_);
  state->last_update_ts_ns_ = last_update_ts_ns_;
  state->epoch_id_ = epoch_id_;
  state->asid_ = asid_;
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
  str += prefix + absl::Substitute("EpochID: $0\n", epoch_id_);
  str += prefix + absl::Substitute("LastUpdateTS: $0\n", last_update_ts_ns_);
  str += prefix + k8s_metadata_state_->DebugString(indent_level);
  str += prefix + absl::Substitute("PIDS($0)\n", pids_by_upid_.size());
  for (const auto& [upid, upid_info] : pids_by_upid_) {
    PL_UNUSED(upid);
    str += prefix + absl::Substitute("$0\n", upid_info->DebugString());
  }

  str += prefix + "\n";
  str += prefix + "--------------------------------------------\n";
  return str;
}

}  // namespace md
}  // namespace px
