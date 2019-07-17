#include <memory>
#include <utility>

#include "absl/base/internal/spinlock.h"
#include "src/shared/metadata/state_manager.h"

namespace pl {
namespace md {

using pl::shared::k8s::metadatapb::MetadataResourceType;
using pl::shared::k8s::metadatapb::ResourceUpdate;

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
  // There should never be more than one updated, but this just here for safety.
  // TODO(zasgar): Change this to a mutex lock.
  absl::base_internal::SpinLockHolder state_update_lock(&metadata_state_update_lock_);

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

  // Get timestamp so all updates happen at the same timestamp.
  // TODO(zasgar): Change this to an injected clock.
  int64_t ts = CurrentTimeNS();
  PL_RETURN_IF_ERROR(ApplyK8sUpdates(ts, shadow_state.get(), &incoming_k8s_updates_));

  // Update PID information.
  PL_RETURN_IF_ERROR(ProcessPIDUpdates(ts, shadow_state.get(), &pid_updates_));

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
      default:
        CHECK(0) << "Unhandled type";
    }
  }

  return Status::OK();
}

Status AgentMetadataStateManager::ProcessPIDUpdates(
    int64_t ts, AgentMetadataState*,
    moodycamel::BlockingConcurrentQueue<std::unique_ptr<PIDStatusEvent>>* pid_updates) {
  PL_UNUSED(ts);
  PL_UNUSED(pid_updates);
  return Status::OK();
}

Status AgentMetadataStateManager::DeleteMetadataForDeadObjects(AgentMetadataState*, int64_t ttl) {
  // TODO(zasgar/michelle): Implement this.
  PL_UNUSED(ttl);
  return Status::OK();
}

Status AgentMetadataStateManager::HandlePodUpdate(const PodUpdate& update,
                                                  AgentMetadataState* state) {
  return state->k8s_metadata_state()->HandlePodUpdate(update);
}

Status AgentMetadataStateManager::HandleContainerUpdate(const ContainerUpdate& update,
                                                        AgentMetadataState* state) {
  return state->k8s_metadata_state()->HandleContainerUpdate(update);
}

}  // namespace md
}  // namespace pl
