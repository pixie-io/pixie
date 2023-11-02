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

      ::px::api::vizierpb::ExecuteScriptResponse resp;
      resp.set_query_id(query_id.str());

      if (rb->has_execution_and_timing_info()) {
        HandleExecutionAndTimingInfo(&resp, rb.get());
        consumer->Write(resp);
        {
          // Query is complete.
          absl::base_internal::SpinLockHolder lock(&id_to_query_consumer_map_lock_);
          consumer_map_.erase(query_id);
        }
      }

      if (rb->has_query_result()) {
        HandleQueryResult(&resp, rb.get());

        consumer->Write(resp);
      }

      if (rb->has_execution_error()) {
        auto exec_error = rb->execution_error();
        if (exec_error.err_code() == 0) {
          response->set_success(true);
          {
            absl::base_internal::SpinLockHolder lock(&id_to_query_consumer_map_lock_);
            consumer_map_.erase(query_id);
          }
          return ::grpc::Status::OK;
        }
        auto status = resp.mutable_status();
        status->set_message(exec_error.msg());
        consumer->Write(resp);
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
  void HandleQueryResult(::px::api::vizierpb::ExecuteScriptResponse* resp,
                         carnotpb::TransferResultChunkRequest* rb) {
    auto query_result = rb->query_result();
    auto row_data = query_result.row_batch();

    auto batch = resp->mutable_data()->mutable_batch();

    batch->set_table_id(query_result.table_name());
    batch->set_num_rows(row_data.num_rows());
    batch->set_eow(row_data.eow());
    batch->set_eos(row_data.eos());

    for (auto col : row_data.cols()) {
      ::px::api::vizierpb::Column c;
      if (col.has_boolean_data()) {
        auto batch_cols = batch->add_cols()->mutable_boolean_data();
        for (auto dt : col.boolean_data().data()) {
          batch_cols->add_data(dt);
        }
      }
      if (col.has_int64_data()) {
        auto batch_cols = batch->add_cols()->mutable_int64_data();
        for (auto dt : col.int64_data().data()) {
          batch_cols->add_data(dt);
        }
      }
      if (col.has_time64ns_data()) {
        auto batch_cols = batch->add_cols()->mutable_time64ns_data();
        for (auto dt : col.time64ns_data().data()) {
          batch_cols->add_data(dt);
        }
      }
      if (col.has_float64_data()) {
        auto batch_cols = batch->add_cols()->mutable_float64_data();
        for (auto dt : col.float64_data().data()) {
          batch_cols->add_data(dt);
        }
      }
      if (col.has_string_data()) {
        auto batch_cols = batch->add_cols()->mutable_string_data();
        for (auto dt : col.string_data().data()) {
          batch_cols->add_data(dt);
        }
      }
      if (col.has_uint128_data()) {
        auto batch_cols = batch->add_cols()->mutable_uint128_data();
        for (auto dt : col.uint128_data().data()) {
          auto n = batch_cols->add_data();
          n->set_low(dt.low());
          n->set_high(dt.high());
        }
      }
    }
  }

  void HandleExecutionAndTimingInfo(::px::api::vizierpb::ExecuteScriptResponse* resp,
                                    carnotpb::TransferResultChunkRequest* rb) {
    auto timing_info = rb->execution_and_timing_info();
    auto stats = resp->mutable_data()->mutable_execution_stats();
    stats->set_bytes_processed(timing_info.execution_stats().bytes_processed());
    stats->set_records_processed(timing_info.execution_stats().records_processed());
    auto timing = stats->mutable_timing();
    timing->set_execution_time_ns(timing_info.execution_stats().timing().execution_time_ns());
    timing->set_compilation_time_ns(timing_info.execution_stats().timing().compilation_time_ns());
  }

  absl::flat_hash_map<sole::uuid, ::grpc::ServerWriter<::px::api::vizierpb::ExecuteScriptResponse>*>
      consumer_map_ GUARDED_BY(id_to_query_consumer_map_lock_);
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
