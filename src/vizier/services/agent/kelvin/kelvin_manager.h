#pragma once

#include <memory>
#include <string>
#include <utility>

#include "src/vizier/services/agent/manager/manager.h"
#include "src/vizier/services/query_broker/querybrokerpb/service.grpc.pb.h"

namespace pl {
namespace vizier {
namespace agent {

class KelvinManager : public Manager {
 public:
  using QueryBrokerService = pl::vizier::services::query_broker::querybrokerpb::QueryBrokerService;
  using QueryBrokerServiceSPtr = std::shared_ptr<QueryBrokerService::Stub>;

  template <typename... Args>
  static StatusOr<std::unique_ptr<Manager>> Create(Args&&... args) {
    auto m = std::unique_ptr<KelvinManager>(new KelvinManager(std::forward<Args>(args)...));
    PL_RETURN_IF_ERROR(m->Init());
    return std::unique_ptr<Manager>(std::move(m));
  }

  ~KelvinManager() override = default;

 protected:
  KelvinManager() = delete;
  KelvinManager(sole::uuid agent_id, std::string_view pod_name, std::string_view host_ip,
                std::string_view addr, int grpc_server_port, std::string_view nats_url,
                std::string_view qb_url, std::string_view mds_url)
      : Manager(agent_id, pod_name, host_ip, grpc_server_port, KelvinManager::Capabilities(),
                nats_url, mds_url),
        qb_stub_(KelvinManager::CreateDefaultQueryBrokerStub(qb_url, grpc_channel_creds_)) {
    info()->address = std::string(addr);
  }

  Status InitImpl() override;
  Status PostRegisterHook() override;
  Status StopImpl(std::chrono::milliseconds) override;

 private:
  static QueryBrokerServiceSPtr CreateDefaultQueryBrokerStub(
      std::string_view query_broker_addr, std::shared_ptr<grpc::ChannelCredentials> channel_creds);

  static services::shared::agent::AgentCapabilities Capabilities() {
    services::shared::agent::AgentCapabilities capabilities;
    capabilities.set_collects_data(false);
    return capabilities;
  }

  QueryBrokerServiceSPtr qb_stub_;
};

}  // namespace agent
}  // namespace vizier
}  // namespace pl
