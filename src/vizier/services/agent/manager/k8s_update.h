#pragma once

#include <memory>

#include "src/vizier/services/agent/manager/manager.h"

namespace pl {
namespace vizier {
namespace agent {

using ::pl::event::Dispatcher;
using ::pl::shared::k8s::metadatapb::ResourceUpdate;

// K8sUpdateHandler is responsible for processing K8s updates.
// TODO(nserrino): PP-2374 Add support for requesting old resource versions.
class K8sUpdateHandler : public Manager::MessageHandler {
 public:
  K8sUpdateHandler() = delete;
  K8sUpdateHandler(Dispatcher* d, pl::md::AgentMetadataStateManager* mds_manager, Info* agent_info,
                   Manager::VizierNATSConnector* nats_conn)
      : MessageHandler(d, agent_info, nats_conn), mds_manager_(mds_manager) {}

  ~K8sUpdateHandler() override = default;

  Status HandleMessage(std::unique_ptr<messages::VizierMessage> msg) override;

 private:
  Status HandleMissingK8sMetadataResponse(const messages::MissingK8sMetadataResponse& update);

  pl::md::AgentMetadataStateManager* mds_manager_;
};

}  // namespace agent
}  // namespace vizier
}  // namespace pl
