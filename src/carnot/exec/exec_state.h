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

#include <arrow/memory_pool.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <sole.hpp>

#include "src/carnot/carnotpb/carnot.pb.h"
#include "src/carnot/exec/exec_metrics.h"
#include "src/carnot/exec/grpc_router.h"
#include "src/carnot/udf/model_pool.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/table_store/table/table_store.h"

#include "opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "opentelemetry/proto/collector/trace/v1/trace_service.grpc.pb.h"
#include "src/carnot/carnotpb/carnot.grpc.pb.h"

namespace px {
namespace carnot {
namespace exec {

using ResultSinkStubGenerator =
    std::function<std::unique_ptr<carnotpb::ResultSinkService::StubInterface>(
        const std::string& address, const std::string& ssl_targetname)>;
using MetricsStubGenerator = std::function<
    std::unique_ptr<opentelemetry::proto::collector::metrics::v1::MetricsService::StubInterface>(
        const std::string& address, bool insecure)>;
using TraceStubGenerator = std::function<
    std::unique_ptr<opentelemetry::proto::collector::trace::v1::TraceService::StubInterface>(
        const std::string& address, bool insecure)>;

/**
 * ExecState manages the execution state for a single query. A new one will
 * be constructed for every query executed in Carnot and it will not be reused.
 *
 * The purpose of this class is to keep track of resources required for the query
 * and provide common resources (UDFs, UDA, etc) the operators within the query.
 */
class ExecState {
 public:
  ExecState() = delete;
  ExecState(
      udf::Registry* func_registry, std::shared_ptr<table_store::TableStore> table_store,
      const ResultSinkStubGenerator& stub_generator,
      const MetricsStubGenerator& metrics_stub_generator,
      const TraceStubGenerator& trace_stub_generator, const sole::uuid& query_id,
      udf::ModelPool* model_pool, GRPCRouter* grpc_router = nullptr,
      std::function<void(grpc::ClientContext*)> add_auth_func = [](grpc::ClientContext*) {},
      ExecMetrics* exec_metrics = nullptr)
      : func_registry_(func_registry),
        table_store_(std::move(table_store)),
        stub_generator_(stub_generator),
        metrics_stub_generator_(metrics_stub_generator),
        trace_stub_generator_(trace_stub_generator),
        query_id_(query_id),
        model_pool_(model_pool),
        grpc_router_(grpc_router),
        add_auth_to_grpc_client_context_func_(add_auth_func),
        exec_metrics_(exec_metrics) {}

  ~ExecState() {
    if (grpc_router_ != nullptr) {
      grpc_router_->DeleteQuery(query_id_);
    }
  }
  arrow::MemoryPool* exec_mem_pool() {
    // TOOD(zasgar): Make this the correct pool.
    return arrow::default_memory_pool();
  }

  udf::Registry* func_registry() { return func_registry_; }

  table_store::TableStore* table_store() { return table_store_.get(); }

  const sole::uuid& query_id() const { return query_id_; }

  udf::ModelPool* model_pool() { return model_pool_; }

  Status AddScalarUDF(int64_t id, const std::string& name,
                      const std::vector<types::DataType> arg_types) {
    PX_ASSIGN_OR_RETURN(auto def, func_registry_->GetScalarUDFDefinition(name, arg_types));
    id_to_scalar_udf_map_[id] = def;
    return Status::OK();
  }

  Status AddUDA(int64_t id, const std::string& name, const std::vector<types::DataType> arg_types) {
    PX_ASSIGN_OR_RETURN(auto def, func_registry_->GetUDADefinition(name, arg_types));
    id_to_uda_map_[id] = def;
    return Status::OK();
  }

  // This function returns a stub to a service that is responsible for receiving results.
  // Currently, it will either be a Kelvin instance or a query broker.
  carnotpb::ResultSinkService::StubInterface* ResultSinkServiceStub(
      const std::string& remote_address, const std::string& ssl_targetname) {
    if (result_sink_stub_map_.contains(remote_address)) {
      return result_sink_stub_map_[remote_address];
    }
    std::unique_ptr<carnotpb::ResultSinkService::StubInterface> stub_ =
        stub_generator_(remote_address, ssl_targetname);
    carnotpb::ResultSinkService::StubInterface* raw = stub_.get();
    result_sink_stub_map_[remote_address] = raw;
    // Push to the pool.
    result_sink_stubs_pool_.push_back(std::move(stub_));
    return raw;
  }

  opentelemetry::proto::collector::metrics::v1::MetricsService::StubInterface* MetricsServiceStub(
      const std::string& remote_address, bool insecure) {
    if (metrics_service_stub_map_.contains(remote_address)) {
      return metrics_service_stub_map_[remote_address];
    }
    std::unique_ptr<opentelemetry::proto::collector::metrics::v1::MetricsService::StubInterface>
        stub_ = metrics_stub_generator_(remote_address, insecure);
    opentelemetry::proto::collector::metrics::v1::MetricsService::StubInterface* raw = stub_.get();
    metrics_service_stub_map_[remote_address] = raw;
    // Push to the pool.
    metrics_service_stubs_pool_.push_back(std::move(stub_));
    return raw;
  }
  opentelemetry::proto::collector::trace::v1::TraceService::StubInterface* TraceServiceStub(
      const std::string& remote_address, bool insecure) {
    if (trace_service_stub_map_.contains(remote_address)) {
      return trace_service_stub_map_[remote_address];
    }
    std::unique_ptr<opentelemetry::proto::collector::trace::v1::TraceService::StubInterface> stub_ =
        trace_stub_generator_(remote_address, insecure);
    opentelemetry::proto::collector::trace::v1::TraceService::StubInterface* raw = stub_.get();
    trace_service_stub_map_[remote_address] = raw;
    // Push to the pool.
    trace_service_stubs_pool_.push_back(std::move(stub_));
    return raw;
  }

  udf::ScalarUDFDefinition* GetScalarUDFDefinition(int64_t id) { return id_to_scalar_udf_map_[id]; }

  std::map<int64_t, udf::ScalarUDFDefinition*> id_to_scalar_udf_map() {
    return id_to_scalar_udf_map_;
  }

  udf::UDADefinition* GetUDADefinition(int64_t id) { return id_to_uda_map_[id]; }

  std::unique_ptr<udf::FunctionContext> CreateFunctionContext() {
    auto ctx = std::make_unique<udf::FunctionContext>(metadata_state_, model_pool_);
    return ctx;
  }

  // A node (ie. Limit) can call this method to say no more records will be processed for this
  // source. That node is responsible for setting eos.
  void StopSource(int64_t src_id) { source_id_to_keep_running_map_[src_id] = false; }

  bool keep_running() {
    DCHECK(current_source_set_);
    return source_id_to_keep_running_map_[current_source_];
  }

  void SetCurrentSource(int64_t source_id) {
    current_source_ = source_id;
    current_source_set_ = true;
    if (source_id_to_keep_running_map_.find(current_source_) ==
        source_id_to_keep_running_map_.end()) {
      source_id_to_keep_running_map_[current_source_] = true;
    }
  }

  void set_metadata_state(std::shared_ptr<const md::AgentMetadataState> metadata_state) {
    metadata_state_ = metadata_state;
  }

  GRPCRouter* grpc_router() { return grpc_router_; }

  void AddAuthToGRPCClientContext(grpc::ClientContext* ctx) {
    CHECK(add_auth_to_grpc_client_context_func_);
    add_auth_to_grpc_client_context_func_(ctx);
  }

  ExecMetrics* exec_metrics() { return exec_metrics_; }

 private:
  udf::Registry* func_registry_;
  std::shared_ptr<table_store::TableStore> table_store_;
  std::shared_ptr<const md::AgentMetadataState> metadata_state_;
  const ResultSinkStubGenerator stub_generator_;
  const MetricsStubGenerator metrics_stub_generator_;
  const TraceStubGenerator trace_stub_generator_;
  std::map<int64_t, udf::ScalarUDFDefinition*> id_to_scalar_udf_map_;
  std::map<int64_t, udf::UDADefinition*> id_to_uda_map_;
  const sole::uuid query_id_;
  udf::ModelPool* model_pool_;
  GRPCRouter* grpc_router_ = nullptr;
  std::function<void(grpc::ClientContext*)> add_auth_to_grpc_client_context_func_;
  ExecMetrics* exec_metrics_;

  int64_t current_source_ = 0;
  bool current_source_set_ = false;
  std::map<int64_t, bool> source_id_to_keep_running_map_;

  std::vector<std::unique_ptr<carnotpb::ResultSinkService::StubInterface>> result_sink_stubs_pool_;
  // Mapping of remote address to stub that serves that address.
  absl::flat_hash_map<std::string, carnotpb::ResultSinkService::StubInterface*>
      result_sink_stub_map_;

  std::vector<
      std::unique_ptr<opentelemetry::proto::collector::metrics::v1::MetricsService::StubInterface>>
      metrics_service_stubs_pool_;
  absl::flat_hash_map<std::string,
                      opentelemetry::proto::collector::metrics::v1::MetricsService::StubInterface*>
      metrics_service_stub_map_;

  std::vector<
      std::unique_ptr<opentelemetry::proto::collector::trace::v1::TraceService::StubInterface>>
      trace_service_stubs_pool_;
  absl::flat_hash_map<std::string,
                      opentelemetry::proto::collector::trace::v1::TraceService::StubInterface*>
      trace_service_stub_map_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace px
