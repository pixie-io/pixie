#include <memory>
#include <utility>

#include "src/shared/metadata/metadata.h"

namespace pl {
namespace md {

std::shared_ptr<const AgentMetadataState> AgentMetadataStateManager::CurrentAgentMetadataState() {
  absl::base_internal::SpinLockHolder lock(&agent_metadata_state_lock_);
  return std::const_pointer_cast<const AgentMetadataState>(agent_metadata_state_);
}

}  // namespace md
}  // namespace pl
