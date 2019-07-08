#include <memory>
#include <utility>

#include "src/shared/metadata/metadata.h"

namespace pl {
namespace md {

std::shared_ptr<const K8sMetadataState> AgentMetadataState::K8sMetadata() {
  return std::const_pointer_cast<const K8sMetadataState>(k8s_metadata_state_);
}

}  // namespace md
}  // namespace pl
