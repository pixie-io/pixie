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

#include <chrono>
#include <memory>
#include <numeric>
#include <string>
#include <utility>

#include <prometheus/gauge.h>

#include "src/common/system/kernel_version.h"
#include "src/stirling/stirling.h"
#include "src/vizier/services/agent/pem/tracepoint_manager.h"
#include "src/vizier/services/agent/shared/manager/manager.h"

DECLARE_uint32(stirling_profiler_stack_trace_sample_period_ms);

namespace px {
namespace vizier {
namespace agent {

constexpr auto kNodeMemoryCollectionPeriod = std::chrono::minutes(1);

class PEMManager : public Manager {
 public:
  template <typename... Args>
  static StatusOr<std::unique_ptr<Manager>> Create(Args&&... args) {
    auto m = std::unique_ptr<PEMManager>(new PEMManager(std::forward<Args>(args)...));
    PX_RETURN_IF_ERROR(m->Init());
    return std::unique_ptr<Manager>(std::move(m));
  }

  ~PEMManager() override = default;

 protected:
  PEMManager() = delete;
  PEMManager(sole::uuid agent_id, std::string_view pod_name, std::string_view host_ip,
             std::string_view nats_url, px::system::KernelVersion kernel_version)
      : PEMManager(agent_id, pod_name, host_ip, nats_url,
                   px::stirling::Stirling::Create(px::stirling::CreateSourceRegistryFromFlag()),
                   kernel_version) {}

  // Constructor which creates the HostInfo for an agent (runs once per node).
  PEMManager(sole::uuid agent_id, std::string_view pod_name, std::string_view host_ip,
             std::string_view nats_url, std::unique_ptr<stirling::Stirling> stirling,
             px::system::KernelVersion kernel_version)
      : Manager(agent_id, pod_name, host_ip, /*grpc_server_port*/ 0, PEMManager::Capabilities(),
                PEMManager::Parameters(), nats_url,
                /*mds_url*/ "", kernel_version),
        stirling_(std::move(stirling)),
        node_available_memory_(prometheus::BuildGauge()
                                   .Name("node_available_memory")
                                   .Help("Amount of memory available for use on the node. "
                                         "Corresponds to /proc/meminfo MemAvailable.")
                                   .Register(GetMetricsRegistry())
                                   .Add({})),
        node_total_memory_(prometheus::BuildGauge()
                               .Name("node_total_memory")
                               .Help("Total amount of memory on the node (includes inuse and free "
                                     "memory). Corresponds to /proc/meminfo MemTotal.")
                               .Register(GetMetricsRegistry())
                               .Add({})) {}

  std::string k8s_update_selector() const override { return info()->host_ip; }

  Status InitImpl() override;
  Status PostRegisterHookImpl() override;
  Status StopImpl(std::chrono::milliseconds) override;

 private:
  Status InitSchemas();
  Status InitClockConverters();
  void StartNodeMemoryCollector();
  static services::shared::agent::AgentCapabilities Capabilities() {
    services::shared::agent::AgentCapabilities capabilities;
    capabilities.set_collects_data(true);
    return capabilities;
  }

  static services::shared::agent::AgentParameters Parameters() {
    services::shared::agent::AgentParameters parameters;
    parameters.set_profiler_stack_trace_sample_period_ms(
        FLAGS_stirling_profiler_stack_trace_sample_period_ms);
    return parameters;
  }

  std::unique_ptr<stirling::Stirling> stirling_;
  std::shared_ptr<TracepointManager> tracepoint_manager_;

  // Timer for triggering ClockConverter polls.
  px::event::TimerUPtr clock_converter_timer_;
  // Timer for collecting info about the node's available memory.
  px::event::TimerUPtr node_memory_timer_;
  prometheus::Gauge& node_available_memory_;
  prometheus::Gauge& node_total_memory_;
};

}  // namespace agent
}  // namespace vizier
}  // namespace px
