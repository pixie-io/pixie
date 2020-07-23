#pragma once

#include <memory>

#include "src/stirling/stirling.h"
#include "src/vizier/services/agent/manager/manager.h"

namespace pl {
namespace vizier {
namespace agent {

/**
 * ProbeManager handles the lifecycles management of dynamic probes.
 *
 * This includes tracking all existing probes, listening for new probes from
 * the incoming message stream and replying to status requests.
 */
class ProbeManager : public Manager::MessageHandler {
 public:
  ProbeManager() = delete;
  ProbeManager(pl::event::Dispatcher* dispatcher, Info* agent_info,
               Manager::VizierNATSConnector* nats_conn, stirling::Stirling* stirling);

  Status HandleMessage(std::unique_ptr<messages::VizierMessage> msg) override;

 private:
  pl::event::Dispatcher* dispatcher_;
  Manager::VizierNATSConnector* nats_conn_;
  stirling::Stirling* stirling_;
};

}  // namespace agent
}  // namespace vizier
}  // namespace pl
