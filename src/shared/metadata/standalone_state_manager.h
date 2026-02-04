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
#include <vector>

#include <absl/base/internal/spinlock.h>

#include "src/common/base/error.h"
#include "src/common/event/time_system.h"
#include "src/shared/metadata/state_manager.h"

namespace px {
namespace md {

/**
 * Implements AgentMetadataStateManger.
 */
class StandaloneAgentMetadataStateManager : public AgentMetadataStateManager {
 public:
  StandaloneAgentMetadataStateManager(std::string_view hostname, uint32_t asid, uint32_t pid,
                                      sole::uuid agent_id, event::TimeSystem* time_system) {
    agent_metadata_state_ = std::make_shared<AgentMetadataState>(hostname, asid, pid, agent_id,
                                                                 /*pod_name=*/"", sole::uuid(),
                                                                 "standalone_pem", "", time_system);
  }
  virtual ~StandaloneAgentMetadataStateManager() = default;
  AgentMetadataFilter* metadata_filter() const override { return nullptr; }
  std::shared_ptr<const AgentMetadataState> CurrentAgentMetadataState() override;
  Status PerformMetadataStateUpdate() override;

  Status AddK8sUpdate(std::unique_ptr<ResourceUpdate>) override {
    return px::error::Unimplemented("k8s not available in standalone context");
  };

  void SetServiceCIDR(CIDRBlock) override{/* empty */};
  void SetPodCIDR(std::vector<CIDRBlock>) override{/* empty */};
  std::unique_ptr<PIDStatusEvent> GetNextPIDStatusEvent() override { return nullptr; };

 private:
  // The metadata state stored here is immutable so that we can easily share a read only
  // copy across threads. The pointer is atomically updated in PerformMetadataStateUpdate(),
  // which is responsible for applying the queued updates.
  std::shared_ptr<const AgentMetadataState> agent_metadata_state_;
  absl::base_internal::SpinLock agent_metadata_state_lock_;
  std::mutex metadata_state_update_lock_;
};

}  // namespace md
}  // namespace px
