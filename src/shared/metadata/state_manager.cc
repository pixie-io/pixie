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

std::shared_ptr<const AgentMetadataState> AgentMetadataStateManager::CurrentAgentMetadataState() {
  absl::base_internal::SpinLockHolder lock(&agent_metadata_state_lock_);
  return std::const_pointer_cast<const AgentMetadataState>(agent_metadata_state_);
}

size_t AgentMetadataStateManager::NumPIDUpdates() const { return pid_updates_.size_approx(); }

std::unique_ptr<PIDStatusEvent> AgentMetadataStateManager::GetNextPIDStatusEvent() {
  std::unique_ptr<PIDStatusEvent> event(nullptr);
  bool found = pid_updates_.try_dequeue(event);
  return found ? std::move(event) : nullptr;
}

Status AgentMetadataStateManager::AddK8sUpdate(std::unique_ptr<ResourceUpdate> update) {
  incoming_k8s_updates_.enqueue(std::move(update));
  return Status::OK();
}

Status AgentMetadataStateManager::PerformMetadataStateUpdate() {
  // There should never be more than one update, but this just here for safety.
  std::lock_guard<std::mutex> state_update_lock(metadata_state_update_lock_);
  /*
   * Performing a state update involves:
   *   1. Create a copy of the current metadata state.
   *   2. Drain the incoming update queue from the metadata service and apply the updates.
   *   3. For each container pull the pid information. Diff this with the existing pids and update.
   *   4. Send diff of pids to the outgoing update Q.
   *   5. Set current update time and increment the epoch.
   *   5. Replace the current agent_metdata_state_ ptr.
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
  PL_RETURN_IF_ERROR(ApplyK8sUpdates(ts, shadow_state.get(), &incoming_k8s_updates_));

  // Look for dead pods. Normally, we would count on the K8s updates to take care of this,
  // but the initial set of pods that are received from the MDS includes pods that
  // belong to other nodes, so we have to do this. Also adds a layer of robustness.
  // TODO(oazizi): Consider removing this once MDS only sends pods belonging to this node/agent.
  RemoveDeadPods(ts, shadow_state.get(), md_reader_.get());

  // Update PID information.
  PL_RETURN_IF_ERROR(ProcessPIDUpdates(ts, shadow_state.get(), md_reader_.get(), &pid_updates_));

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

Status AgentMetadataStateManager::ApplyK8sUpdates(
    int64_t ts, AgentMetadataState* state,
    moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>>* updates) {
  std::unique_ptr<ResourceUpdate> update(nullptr);
  PL_UNUSED(ts);

  // Returns false when no more items.
  while (updates->try_dequeue(update)) {
    switch (update->update_case()) {
      case ResourceUpdate::kPodUpdate:
        PL_RETURN_IF_ERROR(HandlePodUpdate(update->pod_update(), state));
        break;
      case ResourceUpdate::kContainerUpdate:
        PL_RETURN_IF_ERROR(HandleContainerUpdate(update->container_update(), state));
        break;
      case ResourceUpdate::kServiceUpdate:
        PL_RETURN_IF_ERROR(HandleServiceUpdate(update->service_update(), state));
        break;
      default:
        LOG(ERROR) << "Unhandled Update Type: " << update->update_case() << " (ignoring)";
    }
  }

  return Status::OK();
}

// TODO(oazizi): This function should go away once the MDS only sends pods to the agents
// that they belong to.
void AgentMetadataStateManager::RemoveDeadPods(int64_t ts, AgentMetadataState* md,
                                               CGroupMetadataReader* md_reader) {
  const auto& md_state = md->k8s_metadata_state();
  for (const auto& [name, uid] : md_state->pods_by_name()) {
    PL_UNUSED(name);

    // NOTE: Using const_cast is really bad practice the way it is done here.
    // But removing the const-ness is safe here, since RemoveDeadPods() is in the state update loop.
    // Only doing it this way here because this is a temporary function.
    PodInfo* pod_info = const_cast<PodInfo*>(md_state->PodInfoByID(uid));

    if (pod_info->stop_time_ns() == 0 && !md_reader->PodDirExists(*pod_info)) {
      LOG(WARNING) << absl::Substitute(
          "Marking pod and its containers as dead. Likely didn't belong to this node to begin "
          "with. [pod_id=$0]",
          pod_info->uid());
      pod_info->set_stop_time_ns(ts);

      // Mark the Pod's containers as dead too.
      for (auto& cid : pod_info->containers()) {
        // See note above about const_cast.
        ContainerInfo* cinfo = const_cast<ContainerInfo*>(md_state->ContainerInfoByID(cid));
        if (cinfo->stop_time_ns() == 0) {
          cinfo->set_stop_time_ns(ts);

          // Mark the containers PIDs as stopped too.
          const auto& active_upids = cinfo->active_upids();
          for (const auto& upid : active_upids) {
            cinfo->DeactivateUPID(upid);
            md->MarkUPIDAsStopped(upid, ts);
          }
        }
      }
    }
  }
}

Status AgentMetadataStateManager::ProcessPIDUpdates(
    int64_t ts, AgentMetadataState* md, CGroupMetadataReader* md_reader,
    moodycamel::BlockingConcurrentQueue<std::unique_ptr<PIDStatusEvent>>* pid_updates) {
  // For every container:
  //    1. Read the current PIDs (from cgroups).
  //    2. Get the list of current PIDs (for metadata).
  //    3. For each in metadata collect a list of deactivated PIDs. Delete active PIDs from cgroup
  //    version.
  //    4. For each new PID create metadata object and attach to container.
  //    5. For each old pid deactivate it and set time of death.
  absl::flat_hash_set<uint32_t> cgroups_active_pids;
  absl::flat_hash_set<UPID> cgroups_active_upids;

  const auto& md_state = md->k8s_metadata_state();
  const auto& container_by_cid = md_state->containers_by_id();
  auto asid = md->asid();

  for (const auto& [cid, cinfo] : container_by_cid) {
    if (cinfo->stop_time_ns() != 0) {
      // Ignore dead containers.
      // TODO(zasgar): Come up with a cleaner way of doing this. Probably by using active/inactive
      // containers.
      VLOG(1) << "Ignore dead container: " << cinfo->DebugString();
      continue;
    }

    cgroups_active_pids.clear();
    cgroups_active_upids.clear();

    auto pod_id = cinfo->pod_id();
    if (pod_id == "") {
      // No pod id implies it has not synced yet.
      VLOG(1) << "Ignoring Container due to missing pod: \n" << cinfo->DebugString(1);
      continue;
    }
    auto pod_info = md_state->PodInfoByID(pod_id);
    auto pod_qos_class = pod_info->qos_class();

    if (pod_info->stop_time_ns() != 0) {
      LOG(WARNING) << absl::Substitute(
          "Found a running container in a deleted pod [cid=$0, pod_id=$1]", cid, pod_id);
      cinfo->set_stop_time_ns(pod_info->stop_time_ns());
      continue;
    }

    Status s = md_reader->ReadPIDs(pod_qos_class, pod_id, cid, &cgroups_active_pids);
    if (!s.ok()) {
      // Container probably died, we will eventually get a message from MDS and everything in that
      // container will be marked dead.
      LOG(WARNING) << absl::Substitute("Failed to read PID info for pod=$0, cid=$1 [msg=$2]",
                                       pod_id, cid, s.msg());

      // Don't wait for MDS to send the container death information; set the stop time right away.
      // This is so we stop trying to read stats for this non-existent container.
      // NOTE: Currently, MDS sends pods that do no belong to this Agent, so this is actually
      // required to avoid repeatedly printing out the warning message above.
      // TODO(oazizi): Can potentially remove this once MDS is fixed to only send active pods on
      // the agent. Although this code is arguably still useful for robustness.
      if (s.code() == statuspb::Code::NOT_FOUND) {
        cinfo->set_stop_time_ns(ts);
        const auto& active_upids = cinfo->active_upids();
        for (const auto& upid : active_upids) {
          cinfo->DeactivateUPID(upid);
          md->MarkUPIDAsStopped(upid, ts);
        }
      }
      continue;
    }

    // We convert all the cgroup_active_pids to the UPIDs so that we can easily convert and check.
    for (uint32_t pid : cgroups_active_pids) {
      cgroups_active_upids.emplace(asid, pid, md_reader->ReadPIDStartTimeTicks(pid));
    }

    const auto& active_upids = cinfo->active_upids();
    for (const auto& upid : active_upids) {
      auto it = cgroups_active_upids.find(upid);
      if (it != cgroups_active_upids.end()) {
        // Both have the pid so we just need to delete it from the other set.
        cgroups_active_upids.erase(it);
      } else {
        // It does not exist in the new set, but does in the old. Which means that the PID has died.
        // We mark the time of death as current and mark the PID as inactive.
        cinfo->DeactivateUPID(upid);
        md->MarkUPIDAsStopped(upid, ts);

        // Push deletion events to the queue.
        auto pid_status_event = std::make_unique<PIDTerminatedEvent>(upid, ts);
        pid_updates->enqueue(std::move(pid_status_event));
      }
    }

    // The pids left over in the cgroups upids are new processes.
    for (const auto& upid : cgroups_active_upids) {
      auto pid_info = std::make_unique<PIDInfo>(upid, md_reader->ReadPIDCmdline(upid.pid()), cid);
      cinfo->AddUPID(upid);
      // Push creation events to the queue.
      auto pid_status_event = std::make_unique<PIDStartedEvent>(*pid_info);
      pid_updates->enqueue(std::move(pid_status_event));

      md->AddUPID(upid, std::move(pid_info));
    }
  }

  return Status::OK();
}

Status AgentMetadataStateManager::DeleteMetadataForDeadObjects(AgentMetadataState*, int64_t ttl) {
  // TODO(zasgar/michelle): Implement this.
  PL_UNUSED(ttl);
  return Status::OK();
}

Status AgentMetadataStateManager::HandlePodUpdate(const PodUpdate& update,
                                                  AgentMetadataState* state) {
  VLOG(2) << "Pod Update: " << update.DebugString();
  return state->k8s_metadata_state()->HandlePodUpdate(update);
}

Status AgentMetadataStateManager::HandleServiceUpdate(const ServiceUpdate& update,
                                                      AgentMetadataState* state) {
  VLOG(2) << "Service Update: " << update.DebugString();
  return state->k8s_metadata_state()->HandleServiceUpdate(update);
}

Status AgentMetadataStateManager::HandleContainerUpdate(const ContainerUpdate& update,
                                                        AgentMetadataState* state) {
  VLOG(2) << "Container Update: " << update.DebugString();
  return state->k8s_metadata_state()->HandleContainerUpdate(update);
}

}  // namespace md
}  // namespace pl
