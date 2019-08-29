#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"

#include "src/shared/metadata/metadata_state.h"

namespace pl {
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

UID K8sMetadataState::PodIDByName(K8sNameIdentView pod_name) const {
  auto it = pods_by_name_.find(pod_name);
  return (it == pods_by_name_.end()) ? "" : it->second;
}

UID K8sMetadataState::ServiceIDByName(K8sNameIdentView service_name) const {
  auto it = services_by_name_.find(service_name);
  return (it == services_by_name_.end()) ? "" : it->second;
}

const ContainerInfo* K8sMetadataState::ContainerInfoByID(CIDView id) const {
  auto it = containers_by_id_.find(id);
  return it == containers_by_id_.end() ? nullptr : it->second.get();
}

std::unique_ptr<K8sMetadataState> K8sMetadataState::Clone() const {
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

  other->services_by_name_.reserve(services_by_name_.size());
  for (const auto& [k, v] : services_by_name_) {
    other->services_by_name_[k] = v;
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
  return str;
}

namespace {
PodQOSClass ConvertToPodQOsClass(pl::shared::k8s::metadatapb::PodQOSClass pb_enum) {
  using qos_pb = pl::shared::k8s::metadatapb::PodQOSClass;
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
}  // namespace

Status K8sMetadataState::HandlePodUpdate(const PodUpdate& update) {
  const auto& object_uid = update.uid();
  const std::string& name = update.name();
  const std::string& ns = update.namespace_();
  PodQOSClass qos_class = ConvertToPodQOsClass(update.qos_class());

  auto it = k8s_objects_.find(object_uid);
  if (it == k8s_objects_.end()) {
    auto pod = std::make_unique<PodInfo>(object_uid, ns, name, qos_class);
    VLOG(1) << "Adding POD: " << pod->DebugString();
    it = k8s_objects_.try_emplace(object_uid, std::move(pod)).first;
  }

  // We always just add to the container set even if the container is stopped.
  // We expect all cleanup to happen periodically to allow stale objects to be queried for some
  // time. Also, because we expect eventual consistency container ID may or may not be available
  // before the container state is available. Upstream code using this needs to be aware that the
  // state might be periodically inconsistent.
  auto pod_info = static_cast<PodInfo*>(it->second.get());
  for (const auto& cid : update.container_ids()) {
    pod_info->AddContainer(cid);

    // Check assumption that MDS does not send dangling reference.
    DCHECK(containers_by_id_.find(cid) != containers_by_id_.end());

    containers_by_id_[cid]->set_pod_id(object_uid);
  }
  pod_info->set_start_time_ns(update.start_timestamp_ns());
  pod_info->set_stop_time_ns(update.stop_timestamp_ns());

  pods_by_name_[{ns, name}] = object_uid;
  return Status::OK();
}

Status K8sMetadataState::HandleContainerUpdate(const ContainerUpdate& update) {
  const auto& cid = update.cid();

  auto it = containers_by_id_.find(cid);
  if (it == containers_by_id_.end()) {
    auto container = std::make_unique<ContainerInfo>(cid, update.start_timestamp_ns());
    it = containers_by_id_.try_emplace(cid, std::move(container)).first;
  }

  auto* container_info = it->second.get();
  container_info->set_stop_time_ns(update.stop_timestamp_ns());

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
    service_info->AddPod(uid);

    // Check assumption that MDS does not send dangling reference.
    DCHECK(k8s_objects_.find(uid) != k8s_objects_.end());
    DCHECK(k8s_objects_[uid]->type() == K8sObjectType::kPod);

    // We add the service uid to the pod. Lifetime of service still handled by the service object.
    PodInfo* pod_info = static_cast<PodInfo*>(k8s_objects_[uid].get());
    pod_info->AddService(service_uid);
  }
  service_info->set_start_time_ns(update.start_timestamp_ns());
  service_info->set_stop_time_ns(update.stop_timestamp_ns());

  services_by_name_[{ns, name}] = service_uid;
  return Status::OK();
}

std::shared_ptr<AgentMetadataState> AgentMetadataState::CloneToShared() const {
  auto state = std::make_shared<AgentMetadataState>(asid_);
  state->last_update_ts_ns_ = last_update_ts_ns_;
  state->epoch_id_ = epoch_id_;
  state->asid_ = asid_;
  state->k8s_metadata_state_ = k8s_metadata_state_->Clone();
  state->pids_by_upid_.reserve(pids_by_upid_.size());

  for (const auto& [k, v] : pids_by_upid_) {
    state->pids_by_upid_[k] = v->Clone();
  }
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
}  // namespace pl
