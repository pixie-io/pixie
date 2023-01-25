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

#include "src/vizier/services/agent/shared/manager/config_manager.h"
#include "src/common/base/base.h"

namespace px {
namespace vizier {
namespace agent {

ConfigManager::ConfigManager(px::event::Dispatcher* dispatcher, Info* agent_info,
                             Manager::VizierNATSConnector* nats_conn)
    : MessageHandler(dispatcher, agent_info, nats_conn),
      dispatcher_(dispatcher),
      nats_conn_(nats_conn) {
  PX_UNUSED(dispatcher_);
  PX_UNUSED(nats_conn_);
}

Status ConfigManager::HandleMessage(std::unique_ptr<messages::VizierMessage> msg) {
  if (!msg->has_config_update_message()) {
    return error::InvalidArgument("Can only handle config update requests");
  }
  LOG(INFO) << "Got ConfigUpdate Request: " << msg->config_update_message().DebugString();
  return error::Unimplemented("Function is not yet implemented");
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
