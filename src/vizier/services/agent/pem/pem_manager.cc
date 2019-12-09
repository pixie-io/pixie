#include "src/vizier/services/agent/pem/pem_manager.h"

#include "src/vizier/services/agent/manager/manager.h"

namespace pl {
namespace vizier {
namespace agent {

Status PEMManager::InitImpl() { return Status::OK(); }

Status PEMManager::PostRegisterHook() {
  stirling_->RegisterCallback(std::bind(&table_store::TableStore::AppendData, table_store(),
                                        std::placeholders::_1, std::placeholders::_2,
                                        std::placeholders::_3));

  // Register the metadata callback for Stirling.
  stirling_->RegisterAgentMetadataCallback(
      std::bind(&pl::md::AgentMetadataStateManager::CurrentAgentMetadataState, mds_manager()));

  PL_RETURN_IF_ERROR(InitSchemas());
  PL_RETURN_IF_ERROR(stirling_->RunAsThread());
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
  PL_RETURN_IF_ERROR(relation_info_manager()->UpdateRelationInfo(relation_info_vec));
  for (const auto& relation_info : relation_info_vec) {
    PL_RETURN_IF_ERROR(table_store()->AddTable(relation_info.id, relation_info.name,
                                               table_store::Table::Create(relation_info.relation)));
  }
  return Status::OK();
}

}  // namespace agent
}  // namespace vizier
}  // namespace pl
