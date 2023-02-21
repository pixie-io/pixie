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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/exec/exec_metrics.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/funcs/funcs.h"
#include "src/carnot/plan/plan_state.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/udf/model_pool.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {

/**
 * EngineState manages the state required to compile and execute a query.
 *
 * The purpose of this class is to keep track of resources required for the query
 * and provide common resources (UDFs, UDA, etc) the operators within the query.
 */
class EngineState : public NotCopyable {
 public:
  EngineState() = delete;
  EngineState(std::unique_ptr<udf::Registry> func_registry,
              std::shared_ptr<table_store::TableStore> table_store,
              std::unique_ptr<planner::RegistryInfo> registry_info,
              const exec::ResultSinkStubGenerator& stub_generator,
              std::function<void(grpc::ClientContext*)> add_auth_to_grpc_context_func,
              exec::GRPCRouter* grpc_router, std::unique_ptr<udf::ModelPool> model_pool)
      : func_registry_(std::move(func_registry)),
        table_store_(std::move(table_store)),
        registry_info_(std::move(registry_info)),
        stub_generator_(stub_generator),
        add_auth_to_grpc_context_func_(add_auth_to_grpc_context_func),
        grpc_router_(grpc_router),
        model_pool_(std::move(model_pool)),
        metrics_(std::make_unique<ExecMetrics>(&(GetMetricsRegistry()))) {}

  static StatusOr<std::unique_ptr<EngineState>> CreateDefault(
      std::unique_ptr<udf::Registry> func_registry,
      std::shared_ptr<table_store::TableStore> table_store,
      const exec::ResultSinkStubGenerator& stub_generator,
      std::function<void(grpc::ClientContext*)> add_auth_to_grpc_context_func,
      exec::GRPCRouter* grpc_router) {
    auto registry_info = std::make_unique<planner::RegistryInfo>();
    auto udf_info = func_registry->ToProto();
    PX_RETURN_IF_ERROR(registry_info->Init(udf_info));
    auto model_pool = udf::ModelPool::Create();

    return std::make_unique<EngineState>(
        std::move(func_registry), table_store, std::move(registry_info), stub_generator,
        add_auth_to_grpc_context_func, grpc_router, std::move(model_pool));
  }

  table_store::TableStore* table_store() { return table_store_.get(); }
  std::unique_ptr<exec::ExecState> CreateExecState(const sole::uuid& query_id) {
    return std::make_unique<exec::ExecState>(
        func_registry_.get(), table_store_, stub_generator_,
        [this](const std::string& remote_addr, bool insecure) {
          return MetricsStubGenerator(remote_addr, insecure);
        },
        [this](const std::string& remote_addr, bool insecure) {
          return TraceStubGenerator(remote_addr, insecure);
        },
        query_id, model_pool_.get(), grpc_router_, add_auth_to_grpc_context_func_, metrics_.get());
  }
  std::shared_ptr<grpc::Channel> CreateChannel(const std::string& remote_addr, bool insecure) {
    grpc::ChannelArguments args;
    args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, 100000);
    args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 100000);
    args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);
    args.SetInt(GRPC_ARG_HTTP2_BDP_PROBE, 1);
    args.SetInt(GRPC_ARG_HTTP2_MIN_RECV_PING_INTERVAL_WITHOUT_DATA_MS, 50000);
    args.SetInt(GRPC_ARG_HTTP2_MIN_SENT_PING_INTERVAL_WITHOUT_DATA_MS, 100000);

    auto channel_creds = insecure ? grpc::InsecureChannelCredentials()
                                  : grpc::SslCredentials(grpc::SslCredentialsOptions());
    return grpc::CreateCustomChannel(remote_addr, channel_creds, args);
  }

  std::unique_ptr<opentelemetry::proto::collector::metrics::v1::MetricsService::StubInterface>
  MetricsStubGenerator(const std::string& remote_addr, bool insecure) {
    return opentelemetry::proto::collector::metrics::v1::MetricsService::NewStub(
        CreateChannel(remote_addr, insecure));
  }

  std::unique_ptr<opentelemetry::proto::collector::trace::v1::TraceService::StubInterface>
  TraceStubGenerator(const std::string& remote_addr, bool insecure) {
    return opentelemetry::proto::collector::trace::v1::TraceService::NewStub(
        CreateChannel(remote_addr, insecure));
  }

  std::unique_ptr<plan::PlanState> CreatePlanState() {
    return std::make_unique<plan::PlanState>(func_registry_.get());
  }

  std::unique_ptr<planner::CompilerState> CreateLocalExecutionCompilerState(
      types::Time64NSValue time_now) {
    auto rel_map = table_store_->GetRelationMap();
    // Use an empty string for query result address, because the local execution mode should use
    // the Local GRPC result server to send results to.
    return std::make_unique<planner::CompilerState>(
        std::move(rel_map), planner::SensitiveColumnMap{}, registry_info_.get(), time_now,
        /* max_output_rows_per_table */ 0,
        /* result address */ "",
        /* ssl target name override*/ "", planner::RedactionOptions{}, nullptr, nullptr,
        planner::DebugInfo{});
  }

  const udf::Registry* func_registry() const { return func_registry_.get(); }
  std::function<void(grpc::ClientContext*)> add_auth_to_grpc_context_func() const {
    return add_auth_to_grpc_context_func_;
  }

  udf::ModelPool* model_pool() const { return model_pool_.get(); }

 private:
  std::unique_ptr<udf::Registry> func_registry_;
  std::shared_ptr<table_store::TableStore> table_store_;
  std::unique_ptr<planner::RegistryInfo> registry_info_;
  const exec::ResultSinkStubGenerator stub_generator_;
  std::function<void(grpc::ClientContext*)> add_auth_to_grpc_context_func_;
  exec::GRPCRouter* grpc_router_ = nullptr;
  std::unique_ptr<udf::ModelPool> model_pool_;
  std::unique_ptr<ExecMetrics> metrics_;
};

}  // namespace carnot
}  // namespace px
