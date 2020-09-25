#include "src/vizier/services/agent/pem/pem_manager.h"

#include "src/vizier/services/agent/manager/exec.h"
#include "src/vizier/services/agent/manager/manager.h"

namespace pl {
namespace vizier {
namespace agent {

Status PEMManager::InitImpl() { return Status::OK(); }

Status PEMManager::PostRegisterHook() {
  stirling_->RegisterDataPushCallback(std::bind(&table_store::TableStore::AppendData, table_store(),
                                                std::placeholders::_1, std::placeholders::_2,
                                                std::placeholders::_3));

  // Register the metadata callback for Stirling.
  stirling_->RegisterAgentMetadataCallback(
      std::bind(&pl::md::AgentMetadataStateManager::CurrentAgentMetadataState, mds_manager()));

  PL_RETURN_IF_ERROR(InitSchemas());
  PL_RETURN_IF_ERROR(stirling_->RunAsThread());

  auto execute_query_handler = std::make_shared<ExecuteQueryMessageHandler>(
      dispatcher_.get(), info(), nats_connector(), carnot_.get());
  PL_RETURN_IF_ERROR(RegisterMessageHandler(messages::VizierMessage::MsgCase::kExecuteQueryRequest,
                                            execute_query_handler));

  tracepoint_manager_ =
      std::make_shared<TracepointManager>(dispatcher_.get(), info(), nats_connector(),
                                          stirling_.get(), table_store(), relation_info_manager());
  PL_RETURN_IF_ERROR(RegisterMessageHandler(messages::VizierMessage::MsgCase::kTracepointMessage,
                                            tracepoint_manager_));
  return Status::OK();
}

Status PEMManager::StopImpl(std::chrono::milliseconds) {
  stirling_->Stop();
  return Status::OK();
}

Status PEMManager::InitSchemas() {
  pl::stirling::stirlingpb::Publish publish_pb;
  stirling_->GetPublishProto(&publish_pb);
  auto subscribe_pb = stirling::SubscribeToAllInfoClasses(publish_pb);
  PL_RETURN_IF_ERROR(stirling_->SetSubscription(subscribe_pb));

  // This should eventually be done by subscribe requests.
  auto relation_info_vec = ConvertSubscribePBToRelationInfo(subscribe_pb);
  for (const auto& relation_info : relation_info_vec) {
    if (relation_info.name == "http_events") {
      // Make http_events hold 512Mi. This is a hack and will be removed once we have proactive
      // backup.
      auto t = std::shared_ptr<table_store::Table>(
          new table_store::Table(relation_info.relation, 1024 * 1024 * 512));
      table_store()->AddTable(relation_info.id, relation_info.name, t);
    } else {
      table_store()->AddTable(relation_info.id, relation_info.name,
                              table_store::Table::Create(relation_info.relation));
    }
    PL_RETURN_IF_ERROR(relation_info_manager()->AddRelationInfo(relation_info));
  }
  return Status::OK();
}

}  // namespace agent
}  // namespace vizier
}  // namespace pl
