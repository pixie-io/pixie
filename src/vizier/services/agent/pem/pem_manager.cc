/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/vizier/services/agent/pem/pem_manager.h"

#include "src/vizier/services/agent/manager/exec.h"
#include "src/vizier/services/agent/manager/manager.h"

namespace px {
namespace vizier {
namespace agent {

Status PEMManager::InitImpl() { return Status::OK(); }

Status PEMManager::PostRegisterHookImpl() {
  stirling_->RegisterDataPushCallback(std::bind(&table_store::TableStore::AppendData, table_store(),
                                                std::placeholders::_1, std::placeholders::_2,
                                                std::placeholders::_3));

  // Enable use of USR1/USR2 for controlling Stirling debug.
  stirling_->RegisterUserDebugSignalHandlers();

  // Register the metadata callback for Stirling.
  stirling_->RegisterAgentMetadataCallback(
      std::bind(&px::md::AgentMetadataStateManager::CurrentAgentMetadataState, mds_manager()));

  PL_RETURN_IF_ERROR(InitSchemas());
  PL_RETURN_IF_ERROR(stirling_->RunAsThread());

  auto execute_query_handler = std::make_shared<ExecuteQueryMessageHandler>(
      dispatcher(), info(), agent_nats_connector(), carnot());
  PL_RETURN_IF_ERROR(RegisterMessageHandler(messages::VizierMessage::MsgCase::kExecuteQueryRequest,
                                            execute_query_handler));

  tracepoint_manager_ =
      std::make_shared<TracepointManager>(dispatcher(), info(), agent_nats_connector(),
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
  px::stirling::stirlingpb::Publish publish_pb;
  stirling_->GetPublishProto(&publish_pb);
  auto relation_info_vec = ConvertPublishPBToRelationInfo(publish_pb);
  for (const auto& relation_info : relation_info_vec) {
    std::shared_ptr<table_store::Table> table_ptr;
    if (relation_info.name == "http_events") {
      // Make http_events hold 512Mi. This is a hack and will be removed once we have proactive
      // backup.
      table_ptr = std::make_shared<table_store::Table>(relation_info.relation, 1024 * 1024 * 512);
    } else {
      table_ptr = table_store::Table::Create(relation_info.relation);
    }

    table_store()->AddTable(std::move(table_ptr), relation_info.name, relation_info.id);
    PL_RETURN_IF_ERROR(relation_info_manager()->AddRelationInfo(relation_info));
  }
  return Status::OK();
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
