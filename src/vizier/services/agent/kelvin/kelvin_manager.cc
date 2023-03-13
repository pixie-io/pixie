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

#include "src/vizier/services/agent/kelvin/kelvin_manager.h"

#include "src/vizier/services/agent/shared/manager/exec.h"
#include "src/vizier/services/agent/shared/manager/manager.h"

namespace px {
namespace vizier {
namespace agent {

Status KelvinManager::InitImpl() { return Status::OK(); }

Status KelvinManager::PostRegisterHookImpl() {
  auto execute_query_handler = std::make_shared<ExecuteQueryMessageHandler>(
      dispatcher(), info(), agent_nats_connector(), carnot());
  PX_RETURN_IF_ERROR(RegisterMessageHandler(messages::VizierMessage::MsgCase::kExecuteQueryRequest,
                                            execute_query_handler));

  return Status::OK();
}

Status KelvinManager::StopImpl(std::chrono::milliseconds) { return Status::OK(); }

}  // namespace agent
}  // namespace vizier
}  // namespace px
