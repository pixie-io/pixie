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

#include <absl/container/flat_hash_map.h>
#include <grpcpp/grpcpp.h>
#include <algorithm>
#include <memory>
#include <queue>
#include <string>

#include "src/api/proto/vizierpb/vizierapi.pb.h"
#include "src/carnot/carnotpb/carnot.grpc.pb.h"
#include "src/carnot/carnotpb/carnot.pb.h"
#include "src/common/base/base.h"
#include "src/common/base/statuspb/status.pb.h"
#include "src/common/uuid/uuid.h"
#include "src/vizier/services/agent/shared/vizier_results/result_conversion.h"

namespace px {
namespace vizier {
namespace agent {

using QueryExecStats = carnotpb::TransferResultChunkRequest_QueryExecutionAndTimingInfo;

// This class provides a local GRPC server to receive results Carnot for the standalone PEM.
// It is then responsible for forwarding the results to the proper consumer stream.
class StandaloneResultSinkServer final : public carnotpb::ResultSinkService::Service {
 public:
  // Implements the TransferResultChunkAPI of ResultSinkService.
  ::grpc::Status TransferResultChunk(
      ::grpc::ServerContext*,
      ::grpc::ServerReader<::px::carnotpb::TransferResultChunkRequest>* reader,
      ::px::carnotpb::TransferResultChunkResponse* response) override {
    auto rb = std::make_unique<carnotpb::TransferResultChunkRequest>();
    sole::uuid query_id;

    while (reader->Read(rb.get())) {
      query_id = ::px::ParseUUID(rb->query_id()).ConsumeValueOrDie();

      ::grpc::ServerWriter<::px::api::vizierpb::ExecuteScriptResponse>* consumer;
      {
        absl::base_internal::SpinLockHolder lock(&id_to_query_consumer_map_lock_);
        auto consumer_pair = consumer_map_.find(query_id);
        if (consumer_pair == consumer_map_.end()) {
          response->set_success(false);
          return ::grpc::Status::CANCELLED;
        }
        consumer = consumer_pair->second;
      }

      const auto resp = ExecuteScriptResponseFromChunk(*rb, query_id.str());
      if (resp.has_value()) {
        consumer->Write(*resp);
      }

      if (rb->has_execution_and_timing_info()) {
        absl::base_internal::SpinLockHolder lock(&id_to_query_consumer_map_lock_);
        consumer_map_.erase(query_id);
      }

      if (rb->has_execution_error() && rb->execution_error().err_code() == 0) {
        // err_code 0 is Carnot reporting success; ExecuteScriptResponseFromChunk
        // emits nothing for it and the stream ends here.
        response->set_success(true);
        {
          absl::base_internal::SpinLockHolder lock(&id_to_query_consumer_map_lock_);
          consumer_map_.erase(query_id);
        }
        return ::grpc::Status::OK;
      }

      rb = std::make_unique<carnotpb::TransferResultChunkRequest>();
    }

    response->set_success(true);
    return ::grpc::Status::OK;
  }

  void AddConsumer(sole::uuid query_id,
                   ::grpc::ServerWriter<::px::api::vizierpb::ExecuteScriptResponse>* response) {
    absl::base_internal::SpinLockHolder lock(&id_to_query_consumer_map_lock_);

    consumer_map_[query_id] = response;
  }

 private:
  absl::flat_hash_map<sole::uuid, ::grpc::ServerWriter<::px::api::vizierpb::ExecuteScriptResponse>*>
      consumer_map_ ABSL_GUARDED_BY(id_to_query_consumer_map_lock_);
  mutable absl::base_internal::SpinLock id_to_query_consumer_map_lock_;
};

class StandaloneGRPCResultSinkServer {
 public:
  StandaloneGRPCResultSinkServer() {
    grpc::ServerBuilder builder;

    builder.AddListeningPort("localhost:0", grpc::InsecureServerCredentials());
    builder.RegisterService(&result_sink_server_);
    grpc_server_ = builder.BuildAndStart();
    CHECK(grpc_server_ != nullptr);
  }

  ~StandaloneGRPCResultSinkServer() {
    if (grpc_server_) {
      grpc_server_->Shutdown();
    }
  }

  std::unique_ptr<carnotpb::ResultSinkService::StubInterface> StubGenerator(
      const std::string&) const {
    grpc::ChannelArguments args;
    return px::carnotpb::ResultSinkService::NewStub(grpc_server_->InProcessChannel(args));
  }

  void AddConsumer(sole::uuid query_id,
                   ::grpc::ServerWriter<::px::api::vizierpb::ExecuteScriptResponse>* response) {
    result_sink_server_.AddConsumer(query_id, response);
  }

 private:
  std::unique_ptr<grpc::Server> grpc_server_;
  StandaloneResultSinkServer result_sink_server_;
};

}  // namespace agent
}  // namespace vizier
}  // namespace px
