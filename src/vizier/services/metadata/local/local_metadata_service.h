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

#pragma once

#include <chrono>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>

#include "src/common/base/base.h"
#include "src/table_store/table_store.h"
#include "src/vizier/services/metadata/metadatapb/service.grpc.pb.h"
#include "src/vizier/services/metadata/metadatapb/service.pb.h"

namespace px {
namespace vizier {
namespace services {
namespace metadata {

/**
 * LocalMetadataServiceImpl implements a local stub for the MetadataService.
 * Only GetSchemas is implemented - it reads from the table store.
 * All other methods return UNIMPLEMENTED status.
 *
 * This is useful for testing and local execution environments where
 * a full metadata service is not available.
 */
class LocalMetadataServiceImpl final : public MetadataService::Service {
 public:
  LocalMetadataServiceImpl() = delete;
  explicit LocalMetadataServiceImpl(table_store::TableStore* table_store)
      : table_store_(table_store) {}

  ::grpc::Status GetSchemas(::grpc::ServerContext*, const SchemaRequest*,
                             SchemaResponse* response) override {

    // Get all table IDs from the table store
    auto table_ids = table_store_->GetTableIDs();

    // Build the schema response
    auto* schema = response->mutable_schema();

    for (const auto& table_id : table_ids) {
      // Get the table name
      std::string table_name = table_store_->GetTableName(table_id);
      if (table_name.empty()) {
        LOG(WARNING) << "Failed to get table name for ID: " << table_id;
        continue;
      }

      // Get the table object
      auto* table = table_store_->GetTable(table_id);
      if (table == nullptr) {
        LOG(WARNING) << "Failed to get table for ID: " << table_id;
        continue;
      }

      // Get the relation from the table
      auto relation = table->GetRelation();

      // Add to the relation map in the schema
      // The map value is a Relation proto directly
      auto& rel_proto = (*schema->mutable_relation_map())[table_name];

      // Add columns to the relation
      for (size_t i = 0; i < relation.NumColumns(); ++i) {
        auto* col = rel_proto.add_columns();
        col->set_column_name(relation.GetColumnName(i));
        col->set_column_type(relation.GetColumnType(i));
        col->set_column_desc("");  // No description available from table store
        col->set_pattern_type(types::PatternType::GENERAL);
      }

      // Set table description (empty for now)
      rel_proto.set_desc("");
    }

    return ::grpc::Status::OK;
  }

  ::grpc::Status GetAgentUpdates(::grpc::ServerContext*, const AgentUpdatesRequest*,
                                  ::grpc::ServerWriter<AgentUpdatesResponse>*) override {
    return ::grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "GetAgentUpdates not implemented");
  }

  ::grpc::Status GetAgentInfo(::grpc::ServerContext*, const AgentInfoRequest*,
                               AgentInfoResponse* response) override {

    // Create a single agent metadata entry for local testing
    auto* agent_metadata = response->add_info();

    // Set up Agent information
    auto* agent = agent_metadata->mutable_agent();
    auto* agent_info = agent->mutable_info();

    // Generate a fixed UUID for the agent (using a realistic looking UUID)
    // UUID: 12345678-1234-1234-1234-123456789abc
    auto* agent_id = agent_info->mutable_agent_id();
    agent_id->set_high_bits(0x1234567812341234);
    agent_id->set_low_bits(0x1234123456789abc);

    // Set up host information
    auto* host_info = agent_info->mutable_host_info();
    host_info->set_hostname("local-test-host");
    host_info->set_pod_name("local-pem-pod");
    host_info->set_host_ip("127.0.0.1");

    // Set kernel version (example: 5.15.0)
    auto* kernel = host_info->mutable_kernel();
    kernel->set_version(5);
    kernel->set_major_rev(15);
    kernel->set_minor_rev(0);
    host_info->set_kernel_headers_installed(true);

    // Set agent capabilities and parameters
    agent_info->set_ip_address("127.0.0.1");
    auto* capabilities = agent_info->mutable_capabilities();
    capabilities->set_collects_data(true);

    auto* parameters = agent_info->mutable_parameters();
    parameters->set_profiler_stack_trace_sample_period_ms(100);

    // Set agent timestamps and ASID
    auto current_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    agent->set_create_time_ns(current_time_ns);
    agent->set_last_heartbeat_ns(current_time_ns);
    agent->set_asid(0);

    // Set up AgentStatus
    auto* status = agent_metadata->mutable_status();
    status->set_ns_since_last_heartbeat(0);
    status->set_state(
        px::vizier::services::shared::agent::AgentState::AGENT_STATE_HEALTHY);

    // Set up CarnotInfo
    auto* carnot_info = agent_metadata->mutable_carnot_info();
    carnot_info->set_query_broker_address("local-pem:50300");
    auto* carnot_agent_id = carnot_info->mutable_agent_id();
    carnot_agent_id->set_high_bits(0x1234567812341234);
    carnot_agent_id->set_low_bits(0x1234123456789abc);
    carnot_info->set_has_grpc_server(true);
    carnot_info->set_grpc_address("local-pem:50300");
    carnot_info->set_has_data_store(true);
    carnot_info->set_processes_data(true);
    carnot_info->set_accepts_remote_sources(false);
    carnot_info->set_asid(0);

    return ::grpc::Status::OK;
  }

  ::grpc::Status GetWithPrefixKey(::grpc::ServerContext*, const WithPrefixKeyRequest*,
                                   WithPrefixKeyResponse*) override {
    return ::grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "GetWithPrefixKey not implemented");
  }

 private:
  table_store::TableStore* table_store_;
};

/**
 * LocalMetadataGRPCServer wraps the LocalMetadataServiceImpl and provides a gRPC server.
 * Uses in-process communication for efficiency.
 */
class LocalMetadataGRPCServer {
 public:
  LocalMetadataGRPCServer() = delete;
  explicit LocalMetadataGRPCServer(table_store::TableStore* table_store)
      : metadata_service_(std::make_unique<LocalMetadataServiceImpl>(table_store)) {
    grpc::ServerBuilder builder;

    // Use in-process communication
    builder.RegisterService(metadata_service_.get());

    grpc_server_ = builder.BuildAndStart();
    CHECK(grpc_server_ != nullptr);

    LOG(INFO) << "Starting Local Metadata service (in-process)";
  }

  void Stop() {
    if (grpc_server_) {
      grpc_server_->Shutdown();
    }
    grpc_server_.reset(nullptr);
  }

  ~LocalMetadataGRPCServer() { Stop(); }

  std::shared_ptr<MetadataService::Stub> StubGenerator() const {
    grpc::ChannelArguments args;
    // NewStub returns unique_ptr, convert to shared_ptr
    return std::shared_ptr<MetadataService::Stub>(
        MetadataService::NewStub(grpc_server_->InProcessChannel(args)));
  }

 private:
  std::unique_ptr<grpc::Server> grpc_server_;
  std::unique_ptr<LocalMetadataServiceImpl> metadata_service_;
};

}  // namespace metadata
}  // namespace services
}  // namespace vizier
}  // namespace px
