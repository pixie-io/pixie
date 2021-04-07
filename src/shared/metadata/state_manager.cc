#include <memory>
#include <utility>
#include <vector>

#include <absl/base/internal/spinlock.h>
#include "src/shared/metadata/state_manager.h"

namespace pl {
namespace md {

/**
 * kEpochsBetweenObjectDeletion is the interval between when we check if old objects should be
 * removed and are no longer queryable by the metadata.
 */
constexpr uint64_t kEpochsBetweenObjectDeletion = 100;

/**
 * kMaxObjectRetentionAfterDeathNS is the time in nanoseconds that the object is retained after
 * being deleted.
 */
constexpr uint64_t kMaxObjectRetentionAfterDeathNS = 24ULL * 3600ULL * 1'000'000'000ULL;

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
  // TODO(zasgar): Change this to an injected clock.
  int64_t ts = CurrentTimeNS();
  PL_RETURN_IF_ERROR(
      ApplyK8sUpdates(ts, shadow_state.get(), metadata_filter_, &incoming_k8s_updates_));

  if (collects_data_) {
    // Update PID information.
    PL_RETURN_IF_ERROR(
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

  // Increment epoch and update ts.
  ++epoch_id;
  shadow_state->set_epoch_id(epoch_id);
  shadow_state->set_last_update_ts_ns(ts);

  if (epoch_id > 0 && epoch_id % kEpochsBetweenObjectDeletion == 0) {
    PL_RETURN_IF_ERROR(
        DeleteMetadataForDeadObjects(shadow_state.get(), kMaxObjectRetentionAfterDeathNS));
  }

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
  PL_UNUSED(ts);

  // Returns false when no more items.
  while (updates->try_dequeue(update)) {
    switch (update->update_case()) {
      case ResourceUpdate::kPodUpdate:
        PL_RETURN_IF_ERROR(HandlePodUpdate(update->pod_update(), state, metadata_filter));
        break;
      case ResourceUpdate::kContainerUpdate:
        PL_RETURN_IF_ERROR(
            HandleContainerUpdate(update->container_update(), state, metadata_filter));
        break;
      case ResourceUpdate::kServiceUpdate:
        PL_RETURN_IF_ERROR(HandleServiceUpdate(update->service_update(), state, metadata_filter));
        break;
      case ResourceUpdate::kNamespaceUpdate:
        PL_RETURN_IF_ERROR(
            HandleNamespaceUpdate(update->namespace_update(), state, metadata_filter));
        break;
      default:
        LOG(ERROR) << "Unhandled Update Type: " << update->update_case() << " (ignoring)";
    }
  }

  return Status::OK();
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
    //   2. Get the list of current PIDs (for metadata).
    //   3. For each in metadata collect a list of deactivated PIDs. Delete active PIDs from cgroup
    //   version.
    //   4. For each new PID create metadata object and attach to container.
    //   5. For each old pid deactivate it and set time of death.

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
      VLOG(1) << absl::Substitute("Failed to read PID info for pod=$0, cid=$1 [msg=$2]", pod_id,
                                  cid, s.msg());

      // Don't wait for MDS to send the container death information; set the stop time right away.
      // This is so we stop trying to read stats for this non-existent container.
      // NOTE: Currently, MDS sends pods that do no belong to this Agent, so this is actually
      // required to avoid repeatedly printing out the warning message above.
      if (error::IsNotFound(s)) {
        cinfo->set_stop_time_ns(ts);
        for (const auto& upid : cinfo->active_upids()) {
          md->MarkUPIDAsStopped(upid, ts);
        }
        cinfo->DeactivateAllUPIDs();
      }
      continue;
    }
    absl::flat_hash_set<UPID> cgroups_active_upids;
    // We convert all the cgroup_active_pids to the UPIDs so that we can easily convert and check.
    for (uint32_t pid : cgroups_active_pids) {
      StatusOr<int64_t> pid_start_time = proc_parser.GetPIDStartTimeTicks(pid);
      if (!pid_start_time.ok()) {
        LOG(WARNING) << absl::Substitute("Could not determine start time of PID $0", pid);
        continue;
      }
      cgroups_active_upids.emplace(md->asid(), pid, pid_start_time.ValueOrDie());
    }

    std::vector<UPID> upids_to_deactivate;
    for (const auto& upid : cinfo->active_upids()) {
      auto it = cgroups_active_upids.find(upid);
      if (it != cgroups_active_upids.end()) {
        // Both have the pid so we just need to delete it from the other set.
        cgroups_active_upids.erase(it);
      } else {
        // It does not exist in the new set, but does in the old. Which means that the PID has died.
        // We mark the time of death as current and mark the PID as inactive.
        upids_to_deactivate.emplace_back(upid);
        md->MarkUPIDAsStopped(upid, ts);

        // Push deletion events to the queue.
        auto pid_status_event = std::make_unique<PIDTerminatedEvent>(upid, ts);
        pid_updates->enqueue(std::move(pid_status_event));
      }
    }
    for (const auto& upid : upids_to_deactivate) {
      cinfo->DeactivateUPID(upid);
    }
    // The pids left over in the cgroups upids are new processes.
    for (const auto& upid : cgroups_active_upids) {
      auto pid_info = std::make_unique<PIDInfo>(upid, proc_parser.GetPIDCmdline(upid.pid()), cid);
      cinfo->AddUPID(upid);
      // Push creation events to the queue.
      auto pid_status_event = std::make_unique<PIDStartedEvent>(*pid_info);
      pid_updates->enqueue(std::move(pid_status_event));

      md->AddUPID(upid, std::move(pid_info));
    }
  }

  return Status::OK();
}

Status DeleteMetadataForDeadObjects(AgentMetadataState*, int64_t ttl) {
  // TODO(zasgar/michellenguyen, PP-2583): We should clean up any expired metadata.
  PL_UNUSED(ttl);
  return Status::OK();
}

std::string PrependK8sNamespace(std::string_view ns, std::string_view name) {
  return absl::Substitute("$0/$1", ns, name);
}

Status HandlePodUpdate(const PodUpdate& update, AgentMetadataState* state,
                       AgentMetadataFilter* md_filter) {
  VLOG(2) << "Pod Update: " << update.DebugString();
  PL_RETURN_IF_ERROR(md_filter->InsertEntity(MetadataType::POD_ID, update.uid()));
  PL_RETURN_IF_ERROR(md_filter->InsertEntity(MetadataType::POD_NAME, update.name()));
  // TODO(nserrino): Remove this once k8s entities are referred to without namespace in the query
  // language. for now we store names like <namespace>/<entity_name> but in the future just like
  // <entity_name>.
  PL_RETURN_IF_ERROR(md_filter->InsertEntity(
      MetadataType::POD_NAME, PrependK8sNamespace(update.namespace_(), update.name())));
  return state->k8s_metadata_state()->HandlePodUpdate(update);
}

Status HandleServiceUpdate(const ServiceUpdate& update, AgentMetadataState* state,
                           AgentMetadataFilter* md_filter) {
  VLOG(2) << "Service Update: " << update.DebugString();
  PL_RETURN_IF_ERROR(md_filter->InsertEntity(MetadataType::SERVICE_ID, update.uid()));
  // TODO(nserrino): Remove this once k8s entities are referred to without namespace in the query
  // language. for now we store names like <namespace>/<entity_name> but in the future just like
  // <entity_name>.
  PL_RETURN_IF_ERROR(md_filter->InsertEntity(MetadataType::SERVICE_NAME, update.name()));
  PL_RETURN_IF_ERROR(md_filter->InsertEntity(
      MetadataType::SERVICE_NAME, PrependK8sNamespace(update.namespace_(), update.name())));

  return state->k8s_metadata_state()->HandleServiceUpdate(update);
}

Status HandleContainerUpdate(const ContainerUpdate& update, AgentMetadataState* state,
                             AgentMetadataFilter* md_filter) {
  VLOG(2) << "Container Update: " << update.DebugString();
  PL_RETURN_IF_ERROR(md_filter->InsertEntity(MetadataType::CONTAINER_ID, update.cid()));
  return state->k8s_metadata_state()->HandleContainerUpdate(update);
}

Status HandleNamespaceUpdate(const NamespaceUpdate& update, AgentMetadataState* state,
                             AgentMetadataFilter*) {
  VLOG(2) << "Namespace Update: " << update.DebugString();

  return state->k8s_metadata_state()->HandleNamespaceUpdate(update);
}

}  // namespace md
}  // namespace pl
