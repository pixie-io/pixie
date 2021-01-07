#include "src/vizier/services/agent/manager/config_manager.h"
#include "src/common/base/base.h"

namespace pl {
namespace vizier {
namespace agent {

ConfigManager::ConfigManager(pl::event::Dispatcher* dispatcher, Info* agent_info,
                             Manager::VizierNATSConnector* nats_conn)
    : MessageHandler(dispatcher, agent_info, nats_conn),
      dispatcher_(dispatcher),
      nats_conn_(nats_conn) {
  PL_UNUSED(dispatcher_);
  PL_UNUSED(nats_conn_);
}

Status ConfigManager::HandleMessage(std::unique_ptr<messages::VizierMessage> msg) {
  if (!msg->has_config_update_message()) {
    return error::InvalidArgument("Can only handle config update requests");
  }
  LOG(INFO) << "Got ConfigUpdate Request: " << msg->config_update_message().DebugString();
  return error::Unimplemented("Function is not yet implemented");
}

}  // namespace agent
}  // namespace vizier
}  // namespace pl
