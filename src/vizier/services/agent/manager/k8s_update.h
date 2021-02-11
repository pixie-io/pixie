#pragma once

#include <deque>
#include <memory>
#include <queue>
#include <string>

#include "src/vizier/services/agent/manager/manager.h"

namespace pl {
namespace vizier {
namespace agent {

using ::pl::event::Dispatcher;
using ::pl::shared::k8s::metadatapb::MissingK8sMetadataResponse;
using ::pl::shared::k8s::metadatapb::ResourceUpdate;

// K8sUpdateHandler is responsible for processing K8s updates.
// TODO(nserrino): PP-2374 Add support for requesting old resource versions.
class K8sUpdateHandler : public Manager::MessageHandler {
 public:
  K8sUpdateHandler() = delete;

  K8sUpdateHandler(Dispatcher* d, pl::md::AgentMetadataStateManager* mds_manager, Info* agent_info,
                   Manager::VizierNATSConnector* nats_conn)
      : K8sUpdateHandler(d, mds_manager, agent_info, nats_conn, kDefaultMaxUpdateBacklogQueueSize) {
  }

  K8sUpdateHandler(Dispatcher* d, pl::md::AgentMetadataStateManager* mds_manager, Info* agent_info,
                   Manager::VizierNATSConnector* nats_conn, size_t max_update_queue_size);

  ~K8sUpdateHandler() override = default;

  Status HandleMessage(std::unique_ptr<messages::VizierMessage> msg) override;

 private:
  Status HandleMissingK8sMetadataResponse(const MissingK8sMetadataResponse& update);
  Status HandleK8sUpdate(const ResourceUpdate& update);
  Status AddK8sUpdate(const ResourceUpdate& update);
  void RequestMissingMetadata();

  pl::md::AgentMetadataStateManager* mds_manager_;

  // The most recent update version passed to the state manager.
  // If there are resources in the backlog, this will be a lower version than those updates.
  bool initial_metadata_received_ = false;
  int64_t current_update_version_ = 0;

  // Logic for re-requesting missing metadata.
  pl::event::TimerUPtr missing_metadata_request_timer_;
  static constexpr std::chrono::seconds kMissingMetadataTimeout{5};

  // logic/variables for the backlog update queue.
  std::priority_queue<ResourceUpdate, std::deque<ResourceUpdate>,
                      std::function<bool(ResourceUpdate, ResourceUpdate)>>
      update_backlog_;
  const size_t max_update_queue_size_;
  static constexpr size_t kDefaultMaxUpdateBacklogQueueSize{1000};
};

}  // namespace agent
}  // namespace vizier
}  // namespace pl
