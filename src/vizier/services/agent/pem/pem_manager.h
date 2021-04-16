#pragma once

#include <memory>
#include <string>
#include <utility>

#include "src/stirling/stirling.h"
#include "src/vizier/services/agent/manager/manager.h"
#include "src/vizier/services/agent/pem/tracepoint_manager.h"

namespace px {
namespace vizier {
namespace agent {

class PEMManager : public Manager {
 public:
  template <typename... Args>
  static StatusOr<std::unique_ptr<Manager>> Create(Args&&... args) {
    auto m = std::unique_ptr<PEMManager>(new PEMManager(std::forward<Args>(args)...));
    PL_RETURN_IF_ERROR(m->Init());
    return std::unique_ptr<Manager>(std::move(m));
  }

  ~PEMManager() override = default;

 protected:
  PEMManager() = delete;
  PEMManager(sole::uuid agent_id, std::string_view pod_name, std::string_view host_ip,
             std::string_view nats_url)
      : PEMManager(agent_id, host_ip, pod_name, nats_url,
                   px::stirling::Stirling::Create(px::stirling::CreateSourceRegistry())) {}

  PEMManager(sole::uuid agent_id, std::string_view pod_name, std::string_view host_ip,
             std::string_view nats_url, std::unique_ptr<stirling::Stirling> stirling)
      : Manager(agent_id, host_ip, pod_name, /*grpc_server_port*/ 0, PEMManager::Capabilities(),
                nats_url,
                /*mds_url*/ ""),
        stirling_(std::move(stirling)) {}

  std::string k8s_update_selector() const override { return info()->host_ip; }

  Status InitImpl() override;
  Status PostRegisterHookImpl() override;
  Status StopImpl(std::chrono::milliseconds) override;

 private:
  Status InitSchemas();
  static services::shared::agent::AgentCapabilities Capabilities() {
    services::shared::agent::AgentCapabilities capabilities;
    capabilities.set_collects_data(true);
    return capabilities;
  }

  std::unique_ptr<stirling::Stirling> stirling_;
  std::shared_ptr<TracepointManager> tracepoint_manager_;
};

}  // namespace agent
}  // namespace vizier
}  // namespace px
