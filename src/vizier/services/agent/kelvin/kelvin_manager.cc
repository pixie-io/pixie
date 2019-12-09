#include "src/vizier/services/agent/kelvin/kelvin_manager.h"

#include "src/vizier/services/agent/manager/manager.h"

namespace pl {
namespace vizier {
namespace agent {

Status KelvinManager::InitImpl() { return Status::OK(); }

Status KelvinManager::PostRegisterHook() { return Status::OK(); }

Status KelvinManager::StopImpl(std::chrono::milliseconds) { return Status::OK(); }

}  // namespace agent
}  // namespace vizier
}  // namespace pl
