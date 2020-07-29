#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <absl/base/internal/spinlock.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include "src/common/base/base.h"
#include "src/common/system/system.h"
#include "src/shared/k8s/metadatapb/metadata.pb.h"
#include "src/shared/metadata/base_types.h"
#include "src/shared/metadata/cgroup_metadata_reader.h"
#include "src/shared/metadata/k8s_objects.h"
#include "src/shared/metadata/metadata_filter.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/shared/metadata/pids.h"
#include "src/shared/metadatapb/metadata.pb.h"

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
  using NamespaceUpdate = pl::shared::k8s::metadatapb::NamespaceUpdate;

  explicit AgentMetadataStateManager(std::string_view hostname, uint32_t asid, sole::uuid agent_id,
                                     bool collects_data, const pl::system::Config& config,
                                     AgentMetadataFilter* metadata_filter)
      : asid_(asid),
        agent_id_(agent_id),
        proc_parser_(config),
        collects_data_(collects_data),
        metadata_filter_(metadata_filter) {
    md_reader_ = std::make_unique<CGroupMetadataReader>(config);
    agent_metadata_state_ = std::make_shared<AgentMetadataState>(hostname, asid, agent_id);
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

  void SetServiceCIDR(CIDRBlock cidr) {
    absl::base_internal::SpinLockHolder lock(&cidr_lock_);
    service_cidr_ = std::move(cidr);
  }

  void SetPodCIDR(std::vector<CIDRBlock> cidrs) {
    absl::base_internal::SpinLockHolder lock(&cidr_lock_);
    pod_cidrs_ = std::move(cidrs);
  }

  static Status ApplyK8sUpdates(
      int64_t ts, AgentMetadataState* state, AgentMetadataFilter* metadata_filter,
      moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>>* updates);

  static void RemoveDeadPods(int64_t ts, AgentMetadataState* md, CGroupMetadataReader* md_reader);

  static Status ProcessPIDUpdates(
      int64_t ts, const system::ProcParser& proc_parser, AgentMetadataState*, CGroupMetadataReader*,
      moodycamel::BlockingConcurrentQueue<std::unique_ptr<PIDStatusEvent>>* pid_updates);

  static Status DeleteMetadataForDeadObjects(AgentMetadataState*, int64_t ttl);

  static absl::flat_hash_set<MetadataType> MetadataFilterEntities() {
    // TODO(nserrino): Add other metadata fields, such as container name, namespace, node name,
    // hostname.
    return {MetadataType::SERVICE_ID, MetadataType::SERVICE_NAME, MetadataType::POD_ID,
            MetadataType::POD_NAME, MetadataType::CONTAINER_ID};
  }

  AgentMetadataFilter* metadata_filter() const { return metadata_filter_; }

 private:
  uint32_t asid_;
  sole::uuid agent_id_;

  system::ProcParser proc_parser_;

  std::unique_ptr<CGroupMetadataReader> md_reader_;
  // The metadata state stored here is immutable so that we can easily share a read only
  // copy across threads. The pointer is atomically updated in PerformMetadataStateUpdate(),
  // which is responsible for applying the queued updates.
  std::shared_ptr<const AgentMetadataState> agent_metadata_state_;
  absl::base_internal::SpinLock agent_metadata_state_lock_;
  bool collects_data_;

  std::mutex metadata_state_update_lock_;

  moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>> incoming_k8s_updates_;
  moodycamel::BlockingConcurrentQueue<std::unique_ptr<PIDStatusEvent>> pid_updates_;

  absl::base_internal::SpinLock cidr_lock_;
  std::optional<CIDRBlock> service_cidr_;
  std::optional<std::vector<CIDRBlock>> pod_cidrs_;

  AgentMetadataFilter* metadata_filter_;

  static Status HandlePodUpdate(const PodUpdate& update, AgentMetadataState* state,
                                AgentMetadataFilter* metadata_filter);
  static Status HandleContainerUpdate(const ContainerUpdate& update, AgentMetadataState* state,
                                      AgentMetadataFilter* metadata_filter);
  static Status HandleServiceUpdate(const ServiceUpdate& update, AgentMetadataState* state,
                                    AgentMetadataFilter* metadata_filter);
  static Status HandleNamespaceUpdate(const NamespaceUpdate& update, AgentMetadataState* state,
                                      AgentMetadataFilter* metadata_filter);
};

}  // namespace md
}  // namespace pl
