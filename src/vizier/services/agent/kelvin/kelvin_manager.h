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

#include "src/common/system/kernel_version.h"
#include "src/vizier/services/agent/shared/manager/manager.h"

namespace px {
namespace vizier {
namespace agent {

class KelvinManager : public Manager {
 public:
  template <typename... Args>
  static StatusOr<std::unique_ptr<Manager>> Create(Args&&... args) {
    auto m = std::unique_ptr<KelvinManager>(new KelvinManager(std::forward<Args>(args)...));
    PX_RETURN_IF_ERROR(m->Init());
    return std::unique_ptr<Manager>(std::move(m));
  }

  ~KelvinManager() override = default;

 protected:
  KelvinManager() = delete;
  KelvinManager(sole::uuid agent_id, std::string_view pod_name, std::string_view host_ip,
                std::string_view addr, int grpc_server_port, std::string_view nats_url,
                std::string_view mds_url, system::KernelVersion kernel_version)
      : Manager(agent_id, pod_name, host_ip, grpc_server_port, KelvinManager::Capabilities(),
                KelvinManager::Parameters(), nats_url, mds_url, kernel_version) {
    info()->address = std::string(addr);
  }

  Status InitImpl() override;
  Status PostRegisterHookImpl() override;
  Status StopImpl(std::chrono::milliseconds) override;
  std::string k8s_update_selector() const override { return kKelvinUpdateSelector; }

 private:
  static constexpr char kKelvinUpdateSelector[] = "all";
  static services::shared::agent::AgentCapabilities Capabilities() {
    services::shared::agent::AgentCapabilities capabilities;
    capabilities.set_collects_data(false);
    return capabilities;
  }

  static services::shared::agent::AgentParameters Parameters() {
    services::shared::agent::AgentParameters parameters;
    parameters.set_profiler_stack_trace_sample_period_ms(-1);
    return parameters;
  }
};

}  // namespace agent
}  // namespace vizier
}  // namespace px
