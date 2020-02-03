#pragma once

#include <memory>
#include <mutex>
#include <utility>

#include <absl/base/internal/spinlock.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include "src/common/base/base.h"
#include "src/common/system/system.h"
#include "src/shared/k8s/metadatapb/metadata.pb.h"
#include "src/shared/metadata/base_types.h"
#include "src/shared/metadata/cgroup_metadata_reader.h"
#include "src/shared/metadata/k8s_objects.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/shared/metadata/pids.h"

PL_SUPPRESS_WARNINGS_START()
// TODO(michelle): Fix this so that we don't need to the NOLINT.
// NOLINTNEXTLINE(build/include_subdir)
#include "blockingconcurrentqueue.h"
PL_SUPPRESS_WARNINGS_END()

namespace pl {
namespace md {

/**
 * AgentMetadata has all the metadata that is tracked on a per agent basis.
 */
class AgentMetadataStateManager {
 public:
  using ResourceUpdate = pl::shared::k8s::metadatapb::ResourceUpdate;
  using PodUpdate = pl::shared::k8s::metadatapb::PodUpdate;
  using ContainerUpdate = pl::shared::k8s::metadatapb::ContainerUpdate;
  using ServiceUpdate = pl::shared::k8s::metadatapb::ServiceUpdate;

  explicit AgentMetadataStateManager(std::string_view hostname, uint32_t asid, sole::uuid agent_id,
                                     bool collects_data, absl::optional<CIDRBlock> cluster_cidr_opt,
                                     const pl::system::Config& config)
      : asid_(asid), agent_id_(agent_id), collects_data_(collects_data) {
    md_reader_ = std::make_unique<CGroupMetadataReader>(config);
    agent_metadata_state_ = std::make_shared<AgentMetadataState>(hostname, asid, agent_id);
    if (cluster_cidr_opt.has_value()) {
      agent_metadata_state_->k8s_metadata_state()->set_cluster_cidr(cluster_cidr_opt.value());
    }
  }

  uint32_t asid() const { return asid_; }
  const sole::uuid& agent_id() const { return agent_id_; }

  /**
   * This returns the current valid K8sMetadataState. The state is periodically updated
   * and returned pointers should not be held for a long time to avoid memory leaks and
   * stale data.
   *
   * @return shared_ptr to the current AgentMetadataState.
   */
  std::shared_ptr<const AgentMetadataState> CurrentAgentMetadataState();

  /**
   * When called this function will perform a state update of the metadata.
   * This function is meant to be invoked from a thread and is thread-safe.
   *
   * @return Status::OK on success.
   */
  Status PerformMetadataStateUpdate();

  /**
   * Adds a K8s update event that will be processed the next time MetadataStateUpdate is called.
   * @param update the resoure update.
   * @return The status of enqueuing the event.
   */
  Status AddK8sUpdate(std::unique_ptr<ResourceUpdate> update);

  /**
   * The number of PID events to send upstream.
   * This count is approximate and should not be relied on since the underlying system is a
   * threaded.
   */
  size_t NumPIDUpdates() const;

  /**
   * Get the next pid status event. When no more events are available nullptr is returned.
   * @return unique_ptr with the PIDStatusEvent or nullptr.
   */
  std::unique_ptr<PIDStatusEvent> GetNextPIDStatusEvent();

  static Status ApplyK8sUpdates(
      int64_t ts, AgentMetadataState* state,
      moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>>* updates);

  static void RemoveDeadPods(int64_t ts, AgentMetadataState* md, CGroupMetadataReader* md_reader);

  static Status ProcessPIDUpdates(
      int64_t ts, AgentMetadataState*, CGroupMetadataReader*,
      moodycamel::BlockingConcurrentQueue<std::unique_ptr<PIDStatusEvent>>* pid_updates);

  static Status DeleteMetadataForDeadObjects(AgentMetadataState*, int64_t ttl);

 private:
  uint32_t asid_;
  sole::uuid agent_id_;
  std::unique_ptr<CGroupMetadataReader> md_reader_;
  std::shared_ptr<AgentMetadataState> agent_metadata_state_;
  absl::base_internal::SpinLock agent_metadata_state_lock_;
  bool collects_data_;

  std::mutex metadata_state_update_lock_;

  moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>> incoming_k8s_updates_;
  moodycamel::BlockingConcurrentQueue<std::unique_ptr<PIDStatusEvent>> pid_updates_;

  static Status HandlePodUpdate(const PodUpdate& update, AgentMetadataState* state);
  static Status HandleContainerUpdate(const ContainerUpdate& update, AgentMetadataState* state);
  static Status HandleServiceUpdate(const ServiceUpdate& update, AgentMetadataState* state);
};

}  // namespace md
}  // namespace pl
