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
#include "src/common/event/time_system.h"
#include "src/common/system/system.h"
#include "src/shared/k8s/metadatapb/metadata.pb.h"
#include "src/shared/metadata/cgroup_metadata_reader.h"
#include "src/shared/metadata/k8s_objects.h"
#include "src/shared/metadata/metadata_filter.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/shared/metadata/pids.h"
#include "src/shared/metadatapb/metadata.pb.h"
#include "src/shared/upid/upid.h"

PX_SUPPRESS_WARNINGS_START()
#include "blockingconcurrentqueue.h"
PX_SUPPRESS_WARNINGS_END()

namespace px {
namespace md {

using ResourceUpdate = px::shared::k8s::metadatapb::ResourceUpdate;
using PodUpdate = px::shared::k8s::metadatapb::PodUpdate;
using ContainerUpdate = px::shared::k8s::metadatapb::ContainerUpdate;
using ServiceUpdate = px::shared::k8s::metadatapb::ServiceUpdate;
using NamespaceUpdate = px::shared::k8s::metadatapb::NamespaceUpdate;
using NodeUpdate = px::shared::k8s::metadatapb::NodeUpdate;
using ReplicaSetUpdate = px::shared::k8s::metadatapb::ReplicaSetUpdate;
using DeploymentUpdate = px::shared::k8s::metadatapb::DeploymentUpdate;

/**
 * AgentMetadataStateManager has all the metadata that is tracked on a per agent basis.
 */
class AgentMetadataStateManager {
 public:
  virtual ~AgentMetadataStateManager() = default;
  /**
   * Returns the metadata filter for this state manager.
   * @return The metadata filter.
   */
  virtual AgentMetadataFilter* metadata_filter() const = 0;

  /**
   * This returns the current valid K8sMetadataState. The state is periodically updated
   * and returned pointers should not be held for a long time to avoid memory leaks and
   * stale data.
   *
   * @return shared_ptr to the current AgentMetadataState.
   */
  virtual std::shared_ptr<const AgentMetadataState> CurrentAgentMetadataState() = 0;

  /**
   * When called this function will perform a state update of the metadata.
   * This function is meant to be invoked from a thread and is thread-safe.
   *
   * @return Status::OK on success.
   */
  virtual Status PerformMetadataStateUpdate() = 0;

  /**
   * Adds a K8s update event that will be processed the next time MetadataStateUpdate is called.
   * @param update the resoure update.
   * @return The status of enqueuing the event.
   */
  virtual Status AddK8sUpdate(std::unique_ptr<ResourceUpdate> update) = 0;

  /**
   * Sets the service CIDR.
   * @param the service CIDR.
   */
  virtual void SetServiceCIDR(CIDRBlock cidr) = 0;

  /**
   * Sets the pod CIDRs.
   * @param the pod CIDRs.
   */
  virtual void SetPodCIDR(std::vector<CIDRBlock> cidrs) = 0;

  /**
   * Get the next pid status event. When no more events are available nullptr is returned.
   * @return unique_ptr with the PIDStatusEvent or nullptr.
   */
  virtual std::unique_ptr<PIDStatusEvent> GetNextPIDStatusEvent() = 0;
};

/**
 * Implements AgentMetadataStateManger.
 */
class AgentMetadataStateManagerImpl : public AgentMetadataStateManager {
 public:
  virtual ~AgentMetadataStateManagerImpl() = default;

  AgentMetadataStateManagerImpl(std::string_view hostname, uint32_t asid, uint32_t pid,
                                std::string pod_name, sole::uuid agent_id, bool collects_data,
                                const px::system::Config& config,
                                AgentMetadataFilter* metadata_filter, sole::uuid vizier_id,
                                std::string vizier_name, std::string vizier_namespace,
                                event::TimeSystem* time_system)
      : pod_name_(pod_name), collects_data_(collects_data), metadata_filter_(metadata_filter) {
    md_reader_ = std::make_unique<CGroupMetadataReader>(config);
    agent_metadata_state_ =
        std::make_shared<AgentMetadataState>(hostname, asid, pid, agent_id, pod_name, vizier_id,
                                             vizier_name, vizier_namespace, time_system);
  }

  AgentMetadataFilter* metadata_filter() const override { return metadata_filter_; }

  std::shared_ptr<const AgentMetadataState> CurrentAgentMetadataState() override;

  Status PerformMetadataStateUpdate() override;

  Status AddK8sUpdate(std::unique_ptr<ResourceUpdate> update) override;

  void SetServiceCIDR(CIDRBlock cidr) override {
    absl::base_internal::SpinLockHolder lock(&cidr_lock_);
    service_cidr_ = std::move(cidr);
  }

  void SetPodCIDR(std::vector<CIDRBlock> cidrs) override {
    absl::base_internal::SpinLockHolder lock(&cidr_lock_);
    pod_cidrs_ = std::move(cidrs);
  }

  std::unique_ptr<PIDStatusEvent> GetNextPIDStatusEvent() override;

 private:
  /**
   * The number of PID events to send upstream.
   * This count is approximate and should not be relied on since the underlying system is a
   * threaded.
   */
  size_t NumPIDUpdates() const;

  std::string pod_name_;
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
};

/**
 * Applies K8s updates to the current state.
 */
Status ApplyK8sUpdates(
    int64_t ts, AgentMetadataState* state, AgentMetadataFilter* metadata_filter,
    moodycamel::BlockingConcurrentQueue<std::unique_ptr<ResourceUpdate>>* updates);

/**
 * Removes dead pods from the current state.
 */
void RemoveDeadPods(int64_t ts, AgentMetadataState* md, CGroupMetadataReader* md_reader);

/**
 * Processes PID updates.
 */
Status ProcessPIDUpdates(
    int64_t ts, const system::ProcParser& proc_parser, AgentMetadataState*, CGroupMetadataReader*,
    moodycamel::BlockingConcurrentQueue<std::unique_ptr<PIDStatusEvent>>* pid_updates);

/**
 * Deletes metadata for dead objects.
 */
Status DeleteMetadataForDeadObjects(AgentMetadataState*, int64_t ttl);

/**
 * Handlers for K8s update types.
 */
Status HandlePodUpdate(const PodUpdate& update, AgentMetadataState* state,
                       AgentMetadataFilter* metadata_filter);
Status HandleContainerUpdate(const ContainerUpdate& update, AgentMetadataState* state,
                             AgentMetadataFilter* metadata_filter);
Status HandleServiceUpdate(const ServiceUpdate& update, AgentMetadataState* state,
                           AgentMetadataFilter* metadata_filter);
Status HandleNamespaceUpdate(const NamespaceUpdate& update, AgentMetadataState* state,
                             AgentMetadataFilter* metadata_filter);
Status HandleNodeUpdate(const NodeUpdate& update, AgentMetadataState* state,
                        AgentMetadataFilter* metadata_filter);
Status HandleReplicaSetUpdate(const ReplicaSetUpdate& update, AgentMetadataState* state,
                              AgentMetadataFilter* metadata_filter);
Status HandleDeploymentUpdate(const DeploymentUpdate& update, AgentMetadataState* state,
                              AgentMetadataFilter* metadata_filter);
}  // namespace md
}  // namespace px
