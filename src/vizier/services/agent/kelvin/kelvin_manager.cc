#include "src/vizier/services/agent/kelvin/kelvin_manager.h"

#include "src/vizier/services/agent/manager/exec.h"
#include "src/vizier/services/agent/manager/manager.h"

namespace pl {
namespace vizier {
namespace agent {

Status KelvinManager::InitImpl() { return Status::OK(); }

Status KelvinManager::PostRegisterHook() {
  auto execute_query_handler = std::make_shared<ExecuteQueryMessageHandler>(
      dispatcher_.get(), info(), nats_connector(), qb_stub_, carnot_.get());
  PL_RETURN_IF_ERROR(RegisterMessageHandler(messages::VizierMessage::MsgCase::kExecuteQueryRequest,
                                            execute_query_handler));

  return Status::OK();
}

Status KelvinManager::StopImpl(std::chrono::milliseconds) { return Status::OK(); }

KelvinManager::QueryBrokerServiceSPtr KelvinManager::CreateDefaultQueryBrokerStub(
    std::string_view query_broker_addr, std::shared_ptr<grpc::ChannelCredentials> channel_creds) {
  // We need to move the channel here since gRPC mocking is done by the stub.
  auto chan = grpc::CreateChannel(std::string(query_broker_addr), channel_creds);
  return std::make_shared<QueryBrokerService::Stub>(chan);
}

}  // namespace agent
}  // namespace vizier
}  // namespace pl
