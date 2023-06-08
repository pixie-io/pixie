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
#include <utility>
#include <vector>

#include <absl/base/internal/spinlock.h>
#include "src/shared/metadata/state_manager.h"

namespace px {
namespace md {

/**
 * kEpochsBetweenObjectDeletion is the interval between when we check if old objects should be
 * removed and are no longer queryable by the metadata.
 */
constexpr uint64_t kEpochsBetweenObjectDeletion = 100;

/**
 * kMinObjectRetentionAfterDeathNS is the time in nanoseconds that the object is retained after
 * being deleted.
 */
constexpr uint64_t kMinObjectRetentionAfterDeathNS = 24ULL * 3600ULL * 1'000'000'000ULL;

std::shared_ptr<const AgentMetadataState>
AgentMetadataStateManagerImpl::CurrentAgentMetadataState() {
  absl::base_internal::SpinLockHolder lock(&agent_metadata_state_lock_);
  return std::const_pointer_cast<const AgentMetadataState>(agent_metadata_state_);
}

size_t AgentMetadataStateManagerImpl::NumPIDUpdates() const { return pid_updates_.size_approx(); }

std::unique_ptr<PIDStatusEvent> AgentMetadataStateManagerImpl::GetNextPIDStatusEvent() {
  std::unique_ptr<PIDStatusEvent> event(nullptr);
  bool found = pid_updates_.try_dequeue(event);
  return found ? std::move(event) : nullptr;
}

Status AgentMetadataStateManagerImpl::AddK8sUpdate(std::unique_ptr<ResourceUpdate> update) {
  incoming_k8s_updates_.enqueue(std::move(update));
  return Status::OK();
}

Status AgentMetadataStateManagerImpl::PerformMetadataStateUpdate() {
  // There should never be more than one update, but this just here for safety.
  std::lock_guard<std::mutex> state_update_lock(metadata_state_update_lock_);
  /*
   * Performing a state update involves:
   *   1. Create a copy of the current metadata state.
   *   2. Drain the incoming update queue from the metadata service and apply the updates.
   *   3. For each container pull the pid information. Diff this with the existing pids and update.
   *   4. Send diff of pids to the outgoing update Q.
   *   5. Set current update time and increment the epoch.
   *   6. Update pod/service CIDR information if it has changed.
   *   7. Replace the current agent_metdata_state_ ptr.
   */
  uint64_t epoch_id = 0;
  std::shared_ptr<AgentMetadataState> shadow_state;
  {
    absl::base_internal::SpinLockHolder lock(&agent_metadata_state_lock_);
    // Copy the current state into the shadow state.
    shadow_state = agent_metadata_state_->CloneToShared();
    epoch_id = agent_metadata_state_->epoch_id();
  }

  VLOG(1) << absl::Substitute("Starting update of current MDS, epoch_id=$0", epoch_id);
  VLOG(2) << "Current State: \n" << shadow_state->DebugString(1 /* indent_level */);

  // Get timestamp so all updates happen at the same timestamp.
  int64_t ts = agent_metadata_state_->current_time();
  PX_RETURN_IF_ERROR(
      ApplyK8sUpdates(ts, shadow_state.get(), metadata_filter_, &incoming_k8s_updates_));

  if (collects_data_) {
    // Update PID information.
    PX_RETURN_IF_ERROR(
        ProcessPIDUpdates(ts, proc_parser_, shadow_state.get(), md_reader_.get(), &pid_updates_));
  }

  // Update the pod/service CIDRs if they have been updated.
  {
    absl::base_internal::SpinLockHolder lock(&cidr_lock_);
    if (service_cidr_.has_value()) {
      shadow_state->k8s_metadata_state()->set_service_cidr(std::move(service_cidr_.value()));
      service_cidr_.reset();
    }
    if (pod_cidrs_.has_value()) {
      shadow_state->k8s_metadata_state()->set_pod_cidrs(std::move(pod_cidrs_.value()));
      pod_cidrs_.reset();
    }
  }

  if (epoch_id % kEpochsBetweenObjectDeletion == 0) {
    PX_RETURN_IF_ERROR(
        DeleteMetadataForDeadObjects(shadow_state.get(), kMinObjectRetentionAfterDeathNS));
  }

  // Increment epoch and update ts.
  ++epoch_id;
  shadow_state->set_epoch_id(epoch_id);
  shadow_state->set_last_update_ts_ns(ts);

  {
    absl::base_internal::SpinLockHolder lock(&agent_metadata_state_lock_);
    agent_metadata_state_ = std::move(shadow_state);
    shadow_state.reset();
  }

  VLOG(1) << "State Update Complete";
  VLOG(2) << "New MDS State: " << agent_metadata_state_->DebugString();
  return Status::OK();
}

Status ApplyK8sUpdates(
    int64_t ts, AgentMetadataState* state, AgentMetadataFilter* metadata_filter,
    moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>>* updates) {
  std::unique_ptr<ResourceUpdate> update(nullptr);
  PX_UNUSED(ts);

  // Returns false when no more items.
  while (updates->try_dequeue(update)) {
    switch (update->update_case()) {
      case ResourceUpdate::kPodUpdate:
        PX_RETURN_IF_ERROR(HandlePodUpdate(update->pod_update(), state, metadata_filter));
        break;
      case ResourceUpdate::kContainerUpdate:
        PX_RETURN_IF_ERROR(
            HandleContainerUpdate(update->container_update(), state, metadata_filter));
        break;
      case ResourceUpdate::kServiceUpdate:
        PX_RETURN_IF_ERROR(HandleServiceUpdate(update->service_update(), state, metadata_filter));
        break;
      case ResourceUpdate::kNamespaceUpdate:
        PX_RETURN_IF_ERROR(
            HandleNamespaceUpdate(update->namespace_update(), state, metadata_filter));
        break;
      case ResourceUpdate::kNodeUpdate:
        PX_RETURN_IF_ERROR(HandleNodeUpdate(update->node_update(), state, metadata_filter));
        break;
      case ResourceUpdate::kReplicaSetUpdate:
        PX_RETURN_IF_ERROR(
            HandleReplicaSetUpdate(update->replica_set_update(), state, metadata_filter));
        break;
      case ResourceUpdate::kDeploymentUpdate:
        PX_RETURN_IF_ERROR(
            HandleDeploymentUpdate(update->deployment_update(), state, metadata_filter));
        break;
      default:
        LOG(ERROR) << "Unhandled Update Type: " << update->update_case() << " (ignoring)";
    }
  }

  return Status::OK();
}

namespace {

StatusOr<UPID> InitUPID(const system::ProcParser& proc_parser, uint32_t asid, uint32_t pid) {
  PX_ASSIGN_OR_RETURN(int64_t pid_start_time, proc_parser.GetPIDStartTimeTicks(pid));
  return UPID(asid, pid, pid_start_time);
}

}  // namespace

void ProcessContainerPIDUpdates(
    CIDView cid, int64_t ts, const system::ProcParser& proc_parser, AgentMetadataState* md,
    StartTimeOrderedUPIDSet* upids, absl::flat_hash_set<uint32_t>* cgroups_pids,
    moodycamel::BlockingConcurrentQueue<std::unique_ptr<PIDStatusEvent>>* pid_updates) {
  // Iterate through old list of UPIDs, looking for PIDs which have been deleted.
  auto upids_iter = upids->begin();
  while (upids_iter != upids->end()) {
    const auto& prev_upid = *upids_iter;

    auto cgroups_pids_iter = cgroups_pids->find(prev_upid.pid());
    if (cgroups_pids_iter == cgroups_pids->end()) {
      // Deleted PID.
      md->MarkUPIDAsStopped(prev_upid, ts);

      // Push deletion events to the queue.
      pid_updates->enqueue(std::make_unique<PIDTerminatedEvent>(prev_upid, ts));

      // Must use this style instead of `upids->erase(upids_iter++)`,
      // otherwise the iterator becomes invalid, causing potential seg-faults.
      upids_iter = upids->erase(upids_iter);
      continue;
    }

    // We are already tracking this PID.
    // Consume it so cgroups_pids contains only new PIDs at the end of this loop.
    cgroups_pids->erase(cgroups_pids_iter);
    ++upids_iter;
  }

  // Any PIDs left-over in groups_pids are new.
  // Create UPIDs for them.
  for (const auto& pid : *cgroups_pids) {
    StatusOr<UPID> upid_status = InitUPID(proc_parser, md->asid(), pid);
    if (!upid_status.ok()) {
      LOG(WARNING) << absl::Substitute("Could not convert PID to UPID: $0", pid);
      continue;
    }

    UPID upid = upid_status.ValueOrDie();
    upids->emplace(upid);

    std::string exe_path = proc_parser.GetExePath(upid.pid()).ValueOr("");
    std::string cmdline = proc_parser.GetPIDCmdline(upid.pid());
    auto pid_info =
        std::make_unique<PIDInfo>(upid, std::move(exe_path), std::move(cmdline), CID(cid));

    // Push creation events to the queue.
    pid_updates->enqueue(std::make_unique<PIDStartedEvent>(*pid_info));

    md->AddUPID(upid, std::move(pid_info));
  }
}

Status ProcessPIDUpdates(
    int64_t ts, const system::ProcParser& proc_parser, AgentMetadataState* md,
    CGroupMetadataReader* md_reader,
    moodycamel::BlockingConcurrentQueue<std::unique_ptr<PIDStatusEvent>>* pid_updates) {
  const auto& k8s_md_state = md->k8s_metadata_state();

  for (const auto& [cid, cinfo] : k8s_md_state->containers_by_id()) {
    if (cinfo->stop_time_ns() != 0) {
      // Ignore dead containers.
      // TODO(zasgar): Come up with a cleaner way of doing this. Probably by using active/inactive
      // containers.
      VLOG(1) << "Ignore dead container: " << cinfo->DebugString();
      continue;
    }

    // For every container:
    //   1. Read the current PIDs (from cgroups).
    //   2. Get the list of current PIDs (from metadata).
    //   3. For each new PID create metadata object and attach to container.
    //   4. For each old PID deactivate it and set time of death.

    const UID& pod_id = cinfo->pod_id();
    if (pod_id.empty()) {
      // No pod id implies it has not synced yet.
      VLOG(1) << "Ignoring Container due to missing pod: \n" << cinfo->DebugString(1);
      continue;
    }
    const PodInfo* pod_info = k8s_md_state->PodInfoByID(pod_id);

    if (pod_info->stop_time_ns() != 0) {
      VLOG(1) << absl::Substitute("Found a running container in a deleted pod [cid=$0, pod_id=$1]",
                                  cid, pod_id);
      cinfo->set_stop_time_ns(pod_info->stop_time_ns());
      continue;
    }

    absl::flat_hash_set<uint32_t> cgroups_active_pids;
    Status s = md_reader->ReadPIDs(pod_info->qos_class(), pod_id, cid, cinfo->type(),
                                   &cgroups_active_pids);
    if (!s.ok()) {
      // Container probably died, we will eventually get a message from MDS and everything in that
      // container will be marked dead.
      LOG(WARNING) << absl::Substitute("Failed to read PID info for pod=$0, cid=$1 [msg=$2]",
                                       pod_id, cid, s.msg());

      // Don't wait for MDS to send the container death information; set the stop time right away.
      // This is so we stop trying to read stats for this non-existent container.
      // NOTE: Currently, MDS sends pods that do no belong to this Agent, so this is actually
      // required to avoid repeatedly printing out the warning message above.
      if (error::IsNotFound(s)) {
        cinfo->set_stop_time_ns(ts);
        for (const auto& upid : cinfo->active_upids()) {
          md->MarkUPIDAsStopped(upid, ts);
        }
        cinfo->mutable_active_upids()->clear();
      }
      continue;
    }

    ProcessContainerPIDUpdates(cid, ts, proc_parser, md, cinfo->mutable_active_upids(),
                               &cgroups_active_pids, pid_updates);
  }

  return Status::OK();
}

Status DeleteMetadataForDeadObjects(AgentMetadataState* state, int64_t retention_time) {
  PX_RETURN_IF_ERROR(
      state->k8s_metadata_state()->CleanupExpiredMetadata(state->current_time(), retention_time));
  return Status::OK();
}

std::string PrependK8sNamespace(std::string_view ns, std::string_view name) {
  return absl::Substitute("$0/$1", ns, name);
}

Status HandlePodUpdate(const PodUpdate& update, AgentMetadataState* state,
                       AgentMetadataFilter* md_filter) {
  VLOG(2) << "Pod Update: " << update.DebugString();
  PX_RETURN_IF_ERROR(md_filter->InsertEntity(MetadataType::POD_ID, update.uid()));
  PX_RETURN_IF_ERROR(md_filter->InsertEntity(MetadataType::POD_NAME, update.name()));
  PX_RETURN_IF_ERROR(md_filter->InsertEntity(
      MetadataType::POD_NAME, PrependK8sNamespace(update.namespace_(), update.name())));
  return state->k8s_metadata_state()->HandlePodUpdate(update);
}

Status HandleServiceUpdate(const ServiceUpdate& update, AgentMetadataState* state,
                           AgentMetadataFilter* md_filter) {
  VLOG(2) << "Service Update: " << update.DebugString();
  PX_RETURN_IF_ERROR(md_filter->InsertEntity(MetadataType::SERVICE_ID, update.uid()));
  PX_RETURN_IF_ERROR(md_filter->InsertEntity(MetadataType::SERVICE_NAME, update.name()));
  PX_RETURN_IF_ERROR(md_filter->InsertEntity(
      MetadataType::SERVICE_NAME, PrependK8sNamespace(update.namespace_(), update.name())));

  return state->k8s_metadata_state()->HandleServiceUpdate(update);
}

Status HandleContainerUpdate(const ContainerUpdate& update, AgentMetadataState* state,
                             AgentMetadataFilter* md_filter) {
  VLOG(2) << "Container Update: " << update.DebugString();
  PX_RETURN_IF_ERROR(md_filter->InsertEntity(MetadataType::CONTAINER_ID, update.cid()));
  return state->k8s_metadata_state()->HandleContainerUpdate(update);
}

Status HandleNamespaceUpdate(const NamespaceUpdate& update, AgentMetadataState* state,
                             AgentMetadataFilter*) {
  VLOG(2) << "Namespace Update: " << update.DebugString();

  return state->k8s_metadata_state()->HandleNamespaceUpdate(update);
}

Status HandleNodeUpdate(const NodeUpdate& update, AgentMetadataState* state, AgentMetadataFilter*) {
  VLOG(2) << "Node Update: " << update.DebugString();

  return state->k8s_metadata_state()->HandleNodeUpdate(update);
}

Status HandleReplicaSetUpdate(const ReplicaSetUpdate& update, AgentMetadataState* state,
                              AgentMetadataFilter*) {
  VLOG(2) << "Replica Set Update: " << update.DebugString();
  return state->k8s_metadata_state()->HandleReplicaSetUpdate(update);
}

Status HandleDeploymentUpdate(const DeploymentUpdate& update, AgentMetadataState* state,
                              AgentMetadataFilter*) {
  VLOG(2) << "Deployment Update: " << update.DebugString();
  return state->k8s_metadata_state()->HandleDeploymentUpdate(update);
}

}  // namespace md
}  // namespace px
