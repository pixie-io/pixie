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

#include "src/vizier/services/agent/shared/manager/manager.h"

namespace px {
namespace vizier {
namespace agent {

/**
 * ConfigManager handles applying any config changes to the agent.
 */
class ConfigManager : public Manager::MessageHandler {
 public:
  ConfigManager() = delete;
  ConfigManager(px::event::Dispatcher* dispatcher, Info* agent_info,
                Manager::VizierNATSConnector* nats_conn);

  Status HandleMessage(std::unique_ptr<messages::VizierMessage> msg) override;

 private:
  px::event::Dispatcher* dispatcher_;
  Manager::VizierNATSConnector* nats_conn_;
};

}  // namespace agent
}  // namespace vizier
}  // namespace px
