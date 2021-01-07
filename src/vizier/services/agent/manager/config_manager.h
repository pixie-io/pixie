#pragma once

#include <memory>

#include "src/vizier/services/agent/manager/manager.h"

namespace pl {
namespace vizier {
namespace agent {

/**
 * ConfigManager handles applying any config changes to the agent.
 */
class ConfigManager : public Manager::MessageHandler {
 public:
  ConfigManager() = delete;
  ConfigManager(pl::event::Dispatcher* dispatcher, Info* agent_info,
                Manager::VizierNATSConnector* nats_conn);

  Status HandleMessage(std::unique_ptr<messages::VizierMessage> msg) override;

 private:
  pl::event::Dispatcher* dispatcher_;
  Manager::VizierNATSConnector* nats_conn_;
};

}  // namespace agent
}  // namespace vizier
}  // namespace pl
