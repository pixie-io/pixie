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

#include <absl/strings/substitute.h>

#include "src/carnot/funcs/funcs.h"
#include "src/carnot/udf/registry.h"
#include "src/common/system/config.h"
#include "src/vizier/services/agent/shared/manager/exec.h"
#include "src/vizier/services/agent/shared/manager/manager.h"
#include "src/vizier/services/agent/shared/manager/ssl.h"

// Direct-query gRPC endpoint on the normal PEM. Defined here (not in
// pem_main.cc) so the test binary, which links cc_library but not pem_main.cc,
// picks up the symbol. Default-OFF: flag false → port never opened → existing
// PEM deployments unchanged. See DIRECT_QUERY_CONTRACT.md.
//
// Compile-time disable: when PX_PEM_DIRECT_QUERY_DISABLED is defined
// (`--//src/vizier/services/agent/pem:direct_query=disabled`), the gflags
// registrations + the runtime feature code are excluded from the binary
// entirely. See DIRECT_QUERY_SECURITY.md.
#ifndef PX_PEM_DIRECT_QUERY_DISABLED
DEFINE_bool(direct_query_enabled, gflags::BoolFromEnv("PL_PEM_DIRECT_QUERY_ENABLED", false),
            "If true, expose VizierService::ExecuteScript directly from this PEM. "
            "Default false; existing PEM deploys see no behavior change.");
DEFINE_int32(direct_query_port, gflags::Int32FromEnv("PL_PEM_DIRECT_QUERY_PORT", 50305),
             "gRPC listen port for the direct-query service when "
             "--direct_query_enabled=true.");
DEFINE_string(direct_query_jwt_signing_key, gflags::StringFromEnv("PL_JWT_SIGNING_KEY", ""),
              "HMAC key the bearer JWT must verify against. Optional; when "
              "empty, falls back to the shared manager JWT mint key "
              "(--jwt_signing_key in manager.cc, same PL_JWT_SIGNING_KEY env).");

// Declared (not defined) here so MaybeStartDirectQueryServer can fall back to
// it when --direct_query_jwt_signing_key is unset.
// The actual DEFINE_string(jwt_signing_key, …) lives in shared/manager/manager.cc.
DECLARE_string(jwt_signing_key);
#endif  // PX_PEM_DIRECT_QUERY_DISABLED

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
  PX_RETURN_IF_ERROR(MaybeStartDirectQueryServer());
  return Status::OK();
}

// MaybeStartDirectQueryServer is fail-soft: a direct-query init failure
// must never take the PEM data plane down with it. Every error path logs
// and returns Status::OK() so the PEM continues without direct-query.
// Each step emits a LOG(INFO) breadcrumb for debugging via stderr.
Status PEMManager::MaybeStartDirectQueryServer() {
#ifdef PX_PEM_DIRECT_QUERY_DISABLED
  LOG(INFO) << "direct-query: compiled out (PX_PEM_DIRECT_QUERY_DISABLED)";
  return Status::OK();
#else
  if (!FLAGS_direct_query_enabled) {
    LOG(INFO) << "direct-query: disabled (--direct_query_enabled=false)";
    return Status::OK();
  }
  LOG(INFO) << "direct-query: start (port=" << FLAGS_direct_query_port << ")";
  const std::string& effective_signing_key = FLAGS_direct_query_jwt_signing_key.empty()
                                                 ? FLAGS_jwt_signing_key
                                                 : FLAGS_direct_query_jwt_signing_key;
  if (effective_signing_key.empty()) {
    LOG(ERROR) << "direct-query: --direct_query_enabled=true but both signing keys are empty "
                  "(set PL_JWT_SIGNING_KEY) — staying up, direct-query disabled";
    return Status::OK();
  }
  try {
    LOG(INFO) << "direct-query: step 1/6 create sink server";
    direct_query_sink_ = std::make_unique<carnot::exec::LocalGRPCResultSinkServer>();

    LOG(INFO) << "direct-query: step 2/6 register udfs";
    auto func_registry = std::make_unique<carnot::udf::Registry>("direct_query_registry");
    carnot::funcs::RegisterFuncsOrDie(func_registry.get());

    LOG(INFO) << "direct-query: step 3/6 build carnot configs";
    auto clients_config =
        std::make_unique<carnot::Carnot::ClientsConfig>(carnot::Carnot::ClientsConfig{
            [this](const std::string& address, const std::string&) {
              return direct_query_sink_->StubGenerator(address);
            },
            [](::grpc::ClientContext*) {},
        });
    auto server_config = std::make_unique<carnot::Carnot::ServerConfig>();
    server_config->grpc_server_creds = SSL::DefaultGRPCServerCreds();
    server_config->grpc_server_port = 0;

    LOG(INFO) << "direct-query: step 4/6 Carnot::Create";
    std::shared_ptr<table_store::TableStore> ts(table_store(), [](table_store::TableStore*) {});
    auto carnot_or = carnot::Carnot::Create(info()->agent_id, std::move(func_registry), ts,
                                            std::move(clients_config), std::move(server_config));
    if (!carnot_or.ok()) {
      LOG(ERROR) << "direct-query: Carnot::Create failed: " << carnot_or.status().msg()
                 << " — staying up, direct-query disabled";
      direct_query_sink_.reset();
      return Status::OK();
    }
    direct_query_carnot_ = carnot_or.ConsumeValueOrDie();
    direct_query_carnot_->RegisterAgentMetadataCallback(
        std::bind(&::px::md::AgentMetadataStateManager::CurrentAgentMetadataState, mds_manager()));

    LOG(INFO) << "direct-query: step 5/6 build DirectQueryServer";
    direct_query_service_ = std::make_unique<DirectQueryServer>(
        direct_query_carnot_.get(), direct_query_carnot_->GetEngineState(),
        direct_query_sink_.get(), effective_signing_key);

    LOG(INFO) << "direct-query: step 6/6 grpc BuildAndStart on :" << FLAGS_direct_query_port;
    ::grpc::ServerBuilder builder;
    const std::string addr = absl::Substitute("0.0.0.0:$0", FLAGS_direct_query_port);
    builder.AddListeningPort(addr, SSL::DefaultGRPCServerCreds());
    builder.RegisterService(direct_query_service_.get());
    direct_query_grpc_server_ = builder.BuildAndStart();
    if (!direct_query_grpc_server_) {
      LOG(ERROR) << "direct-query: BuildAndStart returned null on " << addr
                 << " — staying up, direct-query disabled";
      StopDirectQueryServer();
      return Status::OK();
    }
    LOG(INFO) << "direct-query: READY on " << addr;
  } catch (const std::exception& e) {
    LOG(ERROR) << "direct-query: exception during startup: " << e.what()
               << " — staying up, direct-query disabled";
    StopDirectQueryServer();
  }
  return Status::OK();
#endif  // PX_PEM_DIRECT_QUERY_DISABLED
}

void PEMManager::StopDirectQueryServer() {
  if (direct_query_grpc_server_) {
    direct_query_grpc_server_->Shutdown();
    direct_query_grpc_server_.reset();
  }
  direct_query_service_.reset();
  direct_query_carnot_.reset();
  direct_query_sink_.reset();
}

Status PEMManager::StopImpl(std::chrono::milliseconds) {
  StopDirectQueryServer();
  stirling_->Stop();
  stirling_.reset();
  return Status::OK();
}

Status PEMManager::InitSchemas() {
  px::stirling::stirlingpb::Publish publish_pb;
  stirling_->GetPublishProto(&publish_pb);
  auto relation_info_vec = ConvertPublishPBToRelationInfo(publish_pb);

  const int64_t memory_limit = FLAGS_table_store_data_limit * 1024 * 1024;
  const int64_t num_tables = relation_info_vec.size();
  const int64_t http_table_size = (FLAGS_table_store_http_events_percent * memory_limit) / 100;
  const int64_t stirling_error_table_size = FLAGS_table_store_stirling_error_limit_bytes / 2;
  const int64_t probe_status_table_size = FLAGS_table_store_stirling_error_limit_bytes / 2;
  const int64_t proc_exit_events_table_size = FLAGS_table_store_proc_exit_events_limit_bytes;

  // Determine which of the four default tables are present
  bool has_http_events = false, has_stirling_error = false, has_probe_status = false,
       has_proc_exit_events = false;
  for (const auto& relation_info : relation_info_vec) {
    if (relation_info.name == "http_events") {
      has_http_events = true;
    } else if (relation_info.name == "stirling_error") {
      has_stirling_error = true;
    } else if (relation_info.name == "probe_status") {
      has_probe_status = true;
    } else if (relation_info.name == "proc_exit_events") {
      has_proc_exit_events = true;
    }
  }

  // Calculate memory used by specific tables
  int64_t used_memory = 0;
  if (has_http_events) {
    used_memory += http_table_size;
  }
  if (has_stirling_error) {
    used_memory += stirling_error_table_size;
  }
  if (has_probe_status) {
    used_memory += probe_status_table_size;
  }
  if (has_proc_exit_events) {
    used_memory += proc_exit_events_table_size;
  }

  const int64_t remaining_memory = memory_limit - used_memory;
  if (remaining_memory < 0) {
    return error::Internal("Table store data limit is too low to store the tables.");
  }
  const int64_t other_table_count =
      num_tables - (has_http_events + has_stirling_error + has_probe_status + has_proc_exit_events);
  const int64_t other_table_size =
      (other_table_count > 0) ? remaining_memory / other_table_count : 0;

  // Create tables with allocated sizes
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
