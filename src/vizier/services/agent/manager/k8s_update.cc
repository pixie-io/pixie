#include "src/vizier/services/agent/manager/k8s_update.h"

#include <memory>
#include <utility>
#include <vector>

#include "src/vizier/services/agent/manager/manager.h"

namespace pl {
namespace vizier {
namespace agent {

Status K8sUpdateHandler::HandleMissingK8sMetadataResponse(
    const messages::MissingK8sMetadataResponse&) {
  return error::Unimplemented("Handling for MissingK8sMetadataResponse is currently unimplemented");
}

Status K8sUpdateHandler::HandleMessage(std::unique_ptr<messages::VizierMessage> msg) {
  LOG_IF(FATAL, !msg->has_k8s_metadata_message()) << "Expected K8sMetadataMessage";
  auto k8s_msg = msg->k8s_metadata_message();

  if (k8s_msg.has_k8s_metadata_update()) {
    return mds_manager_->AddK8sUpdate(
        std::make_unique<ResourceUpdate>(k8s_msg.k8s_metadata_update()));
  }
  if (k8s_msg.has_missing_k8s_metadata_response()) {
    return HandleMissingK8sMetadataResponse(k8s_msg.missing_k8s_metadata_response());
  }

  return error::Internal(
      "Expected either ResourceUpdate or MissingK8sMetadataResponse in K8sMetadataMessage");
}

}  // namespace agent
}  // namespace vizier
}  // namespace pl
