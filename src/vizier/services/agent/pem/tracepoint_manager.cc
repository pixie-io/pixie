#include "src/vizier/services/agent/pem/tracepoint_manager.h"
#include "src/common/base/base.h"

namespace pl {
namespace vizier {
namespace agent {

TracepointManager::TracepointManager(pl::event::Dispatcher* dispatcher, Info* agent_info,
                                     Manager::VizierNATSConnector* nats_conn,
                                     stirling::Stirling* stirling)
    : MessageHandler(dispatcher, agent_info, nats_conn),
      dispatcher_(dispatcher),
      nats_conn_(nats_conn),
      stirling_(stirling) {
  PL_UNUSED(dispatcher_);
  PL_UNUSED(nats_conn_);
  PL_UNUSED(stirling_);
}

Status TracepointManager::HandleMessage(std::unique_ptr<messages::VizierMessage> msg) {
  if (!msg->has_tracepoint_message()) {
    return error::InvalidArgument("Can only handle probe requests");
  }
  LOG(INFO) << "Got Tracepoint Request: " << msg->tracepoint_message().DebugString();
  return error::Unimplemented("Function is not yet implemented");
}

}  // namespace agent
}  // namespace vizier
}  // namespace pl
