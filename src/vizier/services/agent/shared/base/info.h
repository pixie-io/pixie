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

#include <string>

#include <sole.hpp>

#include "src/common/system/kernel_version.h"
#include "src/vizier/services/shared/agentpb/agent.pb.h"

namespace px {
namespace vizier {
namespace agent {

/**
 * Info tracks basic information about and agent such as:
 * id, asid, hostname.
 */
struct Info {
  Info() = default;
  // Identification information for the agent.
  sole::uuid agent_id;
  // Agent short Id.
  uint32_t asid = 0;
  uint32_t pid = 0;
  std::string hostname;
  std::string address;
  std::string pod_name;
  std::string host_ip;
  system::KernelVersion kernel_version;
  services::shared::agent::AgentCapabilities capabilities;
  services::shared::agent::AgentParameters parameters;
};

}  // namespace agent
}  // namespace vizier
}  // namespace px
