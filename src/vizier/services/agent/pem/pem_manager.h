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
#include <string>
#include <utility>

#include "src/stirling/stirling.h"
#include "src/vizier/services/agent/manager/manager.h"
#include "src/vizier/services/agent/pem/tracepoint_manager.h"

namespace px {
namespace vizier {
namespace agent {

class PEMManager : public Manager {
 public:
  template <typename... Args>
  static StatusOr<std::unique_ptr<Manager>> Create(Args&&... args) {
    auto m = std::unique_ptr<PEMManager>(new PEMManager(std::forward<Args>(args)...));
    PL_RETURN_IF_ERROR(m->Init());
    return std::unique_ptr<Manager>(std::move(m));
  }

  ~PEMManager() override = default;

 protected:
  PEMManager() = delete;
  PEMManager(sole::uuid agent_id, std::string_view pod_name, std::string_view host_ip,
             std::string_view nats_url)
      : PEMManager(agent_id, pod_name, host_ip, nats_url,
                   px::stirling::Stirling::Create(px::stirling::CreateSourceRegistryFromFlag())) {}

  PEMManager(sole::uuid agent_id, std::string_view pod_name, std::string_view host_ip,
             std::string_view nats_url, std::unique_ptr<stirling::Stirling> stirling)
      : Manager(agent_id, pod_name, host_ip, /*grpc_server_port*/ 0, PEMManager::Capabilities(),
                nats_url,
                /*mds_url*/ ""),
        stirling_(std::move(stirling)) {}

  std::string k8s_update_selector() const override { return info()->host_ip; }

  Status InitImpl() override;
  Status PostRegisterHookImpl() override;
  Status StopImpl(std::chrono::milliseconds) override;

 private:
  Status InitSchemas();
  Status InitClockConverters();
  static services::shared::agent::AgentCapabilities Capabilities() {
    services::shared::agent::AgentCapabilities capabilities;
    capabilities.set_collects_data(true);
    return capabilities;
  }

  std::unique_ptr<stirling::Stirling> stirling_;
  std::shared_ptr<TracepointManager> tracepoint_manager_;

  // Timer for triggering ClockConverter polls.
  px::event::TimerUPtr clock_converter_timer_;
};

}  // namespace agent
}  // namespace vizier
}  // namespace px
