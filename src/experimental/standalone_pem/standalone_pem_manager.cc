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

#include <memory>
#include <utility>

#include "src/experimental/standalone_pem/standalone_pem_manager.h"
#include "src/shared/schema/utils.h"
#include "src/table_store/table_store.h"
#include "src/vizier/funcs/funcs.h"

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

StandalonePEMManager::StandalonePEMManager(sole::uuid agent_id, std::string_view host_ip)
    : time_system_(std::make_unique<px::event::RealTimeSystem>()),
      api_(std::make_unique<px::event::APIImpl>(time_system_.get())),
      dispatcher_(api_->AllocateDispatcher("manager")),
      table_store_(std::make_shared<table_store::TableStore>()),
      func_context_(this, /* mds_stub= */ nullptr, /* mdtp_stub= */ nullptr,
                    /* cronscript_stub= */ nullptr, table_store_, [](grpc::ClientContext*) {}),
      stirling_(px::stirling::Stirling::Create(px::stirling::CreateSourceRegistryFromFlag())) {
  info_.agent_id = agent_id;
  info_.host_ip = host_ip;

  // Register Vizier specific and carnot builtin functions.
  auto func_registry = std::make_unique<px::carnot::udf::Registry>("vizier_func_registry");
  ::px::vizier::funcs::RegisterFuncsOrDie(func_context_, func_registry.get());

  auto clients_config =
      std::make_unique<px::carnot::Carnot::ClientsConfig>(px::carnot::Carnot::ClientsConfig{});
  auto server_config = std::make_unique<px::carnot::Carnot::ServerConfig>();

  carnot_ = px::carnot::Carnot::Create(agent_id, std::move(func_registry), table_store_,
                                       std::move(clients_config), std::move(server_config))
                .ConsumeValueOrDie();
}

Status StandalonePEMManager::Run() {
  running_ = true;
  PL_RETURN_IF_ERROR(stirling_->RunAsThread());
  dispatcher_->Run(px::event::Dispatcher::RunType::Block);
  running_ = false;
  return Status::OK();
}

Status StandalonePEMManager::Init() {
  stirling_->RegisterDataPushCallback(std::bind(&table_store::TableStore::AppendData, table_store_,
                                                std::placeholders::_1, std::placeholders::_2,
                                                std::placeholders::_3));

  // Enable use of USR1/USR2 for controlling Stirling debug.
  stirling_->RegisterUserDebugSignalHandlers();

  PL_RETURN_IF_ERROR(InitSchemas());
  return Status::OK();
}

Status StandalonePEMManager::InitSchemas() {
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

    table_store_->AddTable(std::move(table_ptr), relation_info.name, relation_info.id);
    // PL_RETURN_IF_ERROR(relation_info_manager_->AddRelationInfo(relation_info));
  }
  return Status::OK();
}

Status StandalonePEMManager::Stop(std::chrono::milliseconds timeout) {
  // Already stopping, protect against multiple calls.
  if (stop_called_) {
    return Status::OK();
  }
  stop_called_ = true;

  dispatcher_->Stop();
  auto s = StopImpl(timeout);

  // Wait for a limited amount of time for main thread to stop processing.
  std::chrono::time_point expiration_time = time_system_->MonotonicTime() + timeout;
  while (running_ && time_system_->MonotonicTime() < expiration_time) {
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
  }

  return s;
}

Status StandalonePEMManager::StopImpl(std::chrono::milliseconds) {
  stirling_->Stop();
  stirling_.reset();
  return Status::OK();
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
