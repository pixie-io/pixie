#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"

#include "src/shared/metadata/metadata_state.h"

namespace pl {
namespace md {

const PodInfo* K8sMetadataState::PodInfoByID(UID pod_id) const {
  auto it = k8s_objects_.find(pod_id);

  if (it == k8s_objects_.end()) {
    return nullptr;
  }

  return (it->second->type() != K8sObjectType::kPod) ? nullptr
                                                     : static_cast<PodInfo*>(it->second.get());
}

UID K8sMetadataState::PodIDByName(K8sNameIdent pod_name) const {
  auto it = pods_by_name_.find(pod_name);
  return (it == pods_by_name_.end()) ? "" : it->second;
}

const ContainerInfo* K8sMetadataState::ContainerInfoByID(const CID& id) const {
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
  return other;
}

Status K8sMetadataState::HandlePodUpdate(const PodUpdate& update) {
  const auto& object_uid = update.uid();
  const std::string& name = update.name();
  const std::string& ns = update.namespace_();

  auto it = k8s_objects_.find(object_uid);
  if (it == k8s_objects_.end()) {
    auto pod = std::make_unique<PodInfo>(object_uid, ns, name);
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
    auto container = std::make_unique<ContainerInfo>(cid);
    it = containers_by_id_.try_emplace(cid, std::move(container)).first;
  }

  auto* container_info = it->second.get();
  container_info->set_start_time_ns(update.start_timestamp_ns());
  container_info->set_stop_time_ns(update.stop_timestamp_ns());

  return Status::OK();
}

std::shared_ptr<AgentMetadataState> AgentMetadataState::CloneToShared() const {
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

}  // namespace md
}  // namespace pl
