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

#include "src/common/system/config.h"
#include "src/vizier/services/agent/shared/manager/exec.h"
#include "src/vizier/services/agent/shared/manager/manager.h"

DEFINE_int32(
    table_store_data_limit, gflags::Int32FromEnv("PL_TABLE_STORE_DATA_LIMIT_MB", 1024 + 256),
    "The maximum amount of data to store in the table store. Defaults to 1.25GB. "
    "(Note that this is the maximum amount of data stored in all tables, but the "
    "actual memory usage of all tables could be slightly higher because of indexing and other "
    "overheads.");

DEFINE_int32(table_store_http_events_percent,
             gflags::Int32FromEnv("PL_TABLE_STORE_HTTP_EVENTS_PERCENT", 40),
             "The percent of the table store data limit that should be devoted to the http_events "
             "table. Defaults to 40%.");

DEFINE_int32(table_store_stirling_error_limit_bytes,
             gflags::Int32FromEnv("PL_TABLE_STORE_STIRLING_ERROR_LIMIT_BYTES", 2 * 1024 * 1024),
             "The maximum amount of data to store in the two tables for Stirling error reporting, "
             "the stirling_error table and probe_status table.");

DEFINE_int32(table_store_proc_exit_events_limit_bytes,
             gflags::Int32FromEnv("PL_TABLE_STORE_PROC_EXIT_EVENTS_LIMIT_BYTES", 10 * 1024 * 1024),
             "The maximum amount of data to store in the proc_exit_events table.");

namespace px {
namespace vizier {
namespace agent {

Status PEMManager::InitImpl() {
  PX_RETURN_IF_ERROR(InitClockConverters());
  StartNodeMemoryCollector();
  return Status::OK();
}

Status PEMManager::PostRegisterHookImpl() {
  stirling_->RegisterDataPushCallback(std::bind(&table_store::TableStore::AppendData, table_store(),
                                                std::placeholders::_1, std::placeholders::_2,
                                                std::placeholders::_3));

  // Enable use of USR1/USR2 for controlling Stirling debug.
  stirling_->RegisterUserDebugSignalHandlers();

  // Register the metadata callback for Stirling.
  stirling_->RegisterAgentMetadataCallback(
      std::bind(&px::md::AgentMetadataStateManager::CurrentAgentMetadataState, mds_manager()));

  PX_RETURN_IF_ERROR(InitSchemas());
  PX_RETURN_IF_ERROR(stirling_->RunAsThread());

  auto execute_query_handler = std::make_shared<ExecuteQueryMessageHandler>(
      dispatcher(), info(), agent_nats_connector(), carnot());
  PX_RETURN_IF_ERROR(RegisterMessageHandler(messages::VizierMessage::MsgCase::kExecuteQueryRequest,
                                            execute_query_handler));

  tracepoint_manager_ =
      std::make_shared<TracepointManager>(dispatcher(), info(), agent_nats_connector(),
                                          stirling_.get(), table_store(), relation_info_manager());
  PX_RETURN_IF_ERROR(RegisterMessageHandler(messages::VizierMessage::MsgCase::kTracepointMessage,
                                            tracepoint_manager_));
  return Status::OK();
}

Status PEMManager::StopImpl(std::chrono::milliseconds) {
  stirling_->Stop();
  stirling_.reset();
  return Status::OK();
}

Status PEMManager::InitSchemas() {
  px::stirling::stirlingpb::Publish publish_pb;
  stirling_->GetPublishProto(&publish_pb);
  auto relation_info_vec = ConvertPublishPBToRelationInfo(publish_pb);

  int64_t memory_limit = FLAGS_table_store_data_limit * 1024 * 1024;
  int64_t num_tables = relation_info_vec.size();
  int64_t http_table_size = (FLAGS_table_store_http_events_percent * memory_limit) / 100;
  int64_t stirling_error_table_size = FLAGS_table_store_stirling_error_limit_bytes / 2;
  int64_t probe_status_table_size = FLAGS_table_store_stirling_error_limit_bytes / 2;
  int64_t proc_exit_events_table_size = FLAGS_table_store_proc_exit_events_limit_bytes;
  int64_t other_table_size = (memory_limit - http_table_size - stirling_error_table_size -
                              probe_status_table_size - proc_exit_events_table_size) /
                             (num_tables - 4);

  for (const auto& relation_info : relation_info_vec) {
    std::shared_ptr<table_store::Table> table_ptr;
    if (relation_info.name == "http_events") {
      // Special case to set the max size of the http_events table differently from the other
      // tables. For now, the min cold batch size is set to 256kB to be consistent with previous
      // behaviour.
      table_ptr = std::make_shared<table_store::Table>(relation_info.name, relation_info.relation,
                                                       http_table_size, 256 * 1024);
    } else if (relation_info.name == "stirling_error") {
      table_ptr = std::make_shared<table_store::Table>(relation_info.name, relation_info.relation,
                                                       stirling_error_table_size);
    } else if (relation_info.name == "probe_status") {
      table_ptr = std::make_shared<table_store::Table>(relation_info.name, relation_info.relation,
                                                       probe_status_table_size);
    } else if (relation_info.name == "proc_exit_events") {
      table_ptr = std::make_shared<table_store::Table>(relation_info.name, relation_info.relation,
                                                       proc_exit_events_table_size);
    } else {
      table_ptr = std::make_shared<table_store::Table>(relation_info.name, relation_info.relation,
                                                       other_table_size);
    }

    table_store()->AddTable(std::move(table_ptr), relation_info.name, relation_info.id);
    PX_RETURN_IF_ERROR(relation_info_manager()->AddRelationInfo(relation_info));
  }
  return Status::OK();
}

Status PEMManager::InitClockConverters() {
  clock_converter_timer_ = dispatcher()->CreateTimer([this]() {
    auto clock_converter = px::system::Config::GetInstance().clock_converter();
    clock_converter->Update();
    if (clock_converter_timer_) {
      clock_converter_timer_->EnableTimer(clock_converter->UpdatePeriod());
    }
  });
  clock_converter_timer_->EnableTimer(
      px::system::Config::GetInstance().clock_converter()->UpdatePeriod());
  return Status::OK();
}

void PEMManager::StartNodeMemoryCollector() {
  node_memory_timer_ = dispatcher()->CreateTimer([this]() {
    px::system::ProcParser proc_parser;
    px::system::ProcParser::SystemStats stats;
    auto s = proc_parser.ParseProcMemInfo(&stats);
    LOG_IF(ERROR, !s.ok()) << "Failed to parse /proc/meminfo " << s.msg();
    if (s.ok()) {
      node_total_memory_.Set(stats.mem_total_bytes);
      node_available_memory_.Set(stats.mem_available_bytes);
    }
    if (node_memory_timer_) {
      node_memory_timer_->EnableTimer(kNodeMemoryCollectionPeriod);
    }
  });
  node_memory_timer_->EnableTimer(kNodeMemoryCollectionPeriod);
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
