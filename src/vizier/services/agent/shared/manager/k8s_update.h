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

#include <deque>
#include <memory>
#include <queue>
#include <string>

#include "src/vizier/services/agent/shared/manager/manager.h"

namespace px {
namespace vizier {
namespace agent {

using ::px::event::Dispatcher;
using ::px::shared::k8s::metadatapb::MissingK8sMetadataResponse;
using ::px::shared::k8s::metadatapb::ResourceUpdate;

// K8sUpdateHandler is responsible for processing K8s updates.
class K8sUpdateHandler : public Manager::MessageHandler {
 public:
  K8sUpdateHandler() = delete;

  K8sUpdateHandler(Dispatcher* d, px::md::AgentMetadataStateManager* mds_manager, Info* agent_info,
                   Manager::VizierNATSConnector* nats_conn, const std::string& update_selector)
      : K8sUpdateHandler(d, mds_manager, agent_info, nats_conn, update_selector,
                         kDefaultMaxUpdateBacklogQueueSize) {}

  K8sUpdateHandler(Dispatcher* d, px::md::AgentMetadataStateManager* mds_manager, Info* agent_info,
                   Manager::VizierNATSConnector* nats_conn, const std::string& update_selector,
                   size_t max_update_queue_size);

  ~K8sUpdateHandler() override = default;

  Status HandleMessage(std::unique_ptr<messages::VizierMessage> msg) override;

 private:
  Status HandleMissingK8sMetadataResponse(const MissingK8sMetadataResponse& update);
  Status HandleK8sUpdate(const ResourceUpdate& update);
  Status AddK8sUpdate(const ResourceUpdate& update);
  void RequestMissingMetadata();

  px::md::AgentMetadataStateManager* mds_manager_;

  // The selector to use when requested K8s updates. Varies based on manager type.
  const std::string update_selector_;

  // The most recent update version passed to the state manager.
  // If there are resources in the backlog, this will be a lower version than those updates.
  bool initial_metadata_received_ = false;
  int64_t current_update_version_ = 0;

  // Logic for re-requesting missing metadata.
  px::event::TimerUPtr missing_metadata_request_timer_;
  static constexpr std::chrono::seconds kMissingMetadataTimeout{5};

  // logic/variables for the backlog update queue.
  std::priority_queue<ResourceUpdate, std::deque<ResourceUpdate>,
                      std::function<bool(ResourceUpdate, ResourceUpdate)>>
      update_backlog_;
  const size_t max_update_queue_size_;
  static constexpr size_t kDefaultMaxUpdateBacklogQueueSize{1000};
};

}  // namespace agent
}  // namespace vizier
}  // namespace px
