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

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <absl/numeric/int128.h>
#include <clickhouse/client.h>
#include <grpcpp/grpcpp.h>
#include <magic_enum.hpp>

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"
#include "src/common/base/base.h"
#include "src/common/uuid/uuid.h"
#include "src/shared/types/typespb/types.pb.h"
#include "src/vizier/services/agent/shared/manager/manager.h"
#include "src/vizier/services/metadata/metadatapb/service.grpc.pb.h"

namespace px {
namespace vizier {
namespace funcs {
namespace md {

constexpr std::string_view kKernelHeadersInstalledDesc =
    "Whether the agent had linux headers pre-installed";

template <typename TUDTF>
class UDTFWithMDFactory : public carnot::udf::UDTFFactory {
 public:
  UDTFWithMDFactory() = delete;
  explicit UDTFWithMDFactory(const VizierFuncFactoryContext& ctx) : ctx_(ctx) {}

  std::unique_ptr<carnot::udf::AnyUDTF> Make() override {
    return std::make_unique<TUDTF>(ctx_.mds_stub(), ctx_.add_auth_to_grpc_context_func());
  }

 private:
  const VizierFuncFactoryContext& ctx_;
};

template <typename TUDTF>
class UDTFWithMDTPFactory : public carnot::udf::UDTFFactory {
 public:
  UDTFWithMDTPFactory() = delete;
  explicit UDTFWithMDTPFactory(const VizierFuncFactoryContext& ctx) : ctx_(ctx) {}

  std::unique_ptr<carnot::udf::AnyUDTF> Make() override {
    return std::make_unique<TUDTF>(ctx_.mdtp_stub(), ctx_.add_auth_to_grpc_context_func());
  }

 private:
  const VizierFuncFactoryContext& ctx_;
};

template <typename TUDTF>
class UDTFWithMDFSFactory : public carnot::udf::UDTFFactory {
 public:
  UDTFWithMDFSFactory() = delete;
  explicit UDTFWithMDFSFactory(const VizierFuncFactoryContext& ctx) : ctx_(ctx) {}

  std::unique_ptr<carnot::udf::AnyUDTF> Make() override {
    return std::make_unique<TUDTF>(ctx_.mdfs_stub(), ctx_.add_auth_to_grpc_context_func());
  }

 private:
  const VizierFuncFactoryContext& ctx_;
};

template <typename TUDTF>
class UDTFWithCronscriptFactory : public carnot::udf::UDTFFactory {
 public:
  UDTFWithCronscriptFactory() = delete;
  explicit UDTFWithCronscriptFactory(const VizierFuncFactoryContext& ctx) : ctx_(ctx) {}

  std::unique_ptr<carnot::udf::AnyUDTF> Make() override {
    return std::make_unique<TUDTF>(ctx_.cronscript_stub(), ctx_.add_auth_to_grpc_context_func());
  }

 private:
  const VizierFuncFactoryContext& ctx_;
};

template <typename TUDTF>
class UDTFWithRegistryFactory : public carnot::udf::UDTFFactory {
 public:
  UDTFWithRegistryFactory() = delete;
  explicit UDTFWithRegistryFactory(const carnot::udf::Registry* registry) : registry_(registry) {}

  std::unique_ptr<carnot::udf::AnyUDTF> Make() override {
    return std::make_unique<TUDTF>(registry_);
  }

 private:
  const carnot::udf::Registry* registry_;
};

template <typename TUDTF>
class UDTFWithTableStoreFactory : public carnot::udf::UDTFFactory {
 public:
  UDTFWithTableStoreFactory() = delete;
  explicit UDTFWithTableStoreFactory(const ::px::table_store::TableStore* table_store)
      : table_store_(table_store) {}

  std::unique_ptr<carnot::udf::AnyUDTF> Make() override {
    return std::make_unique<TUDTF>(table_store_);
  }

 private:
  const ::px::table_store::TableStore* table_store_;
};

/**
 * This UDTF fetches all the tables that are available to query from the MDS.
 */
class GetTables final : public carnot::udf::UDTF<GetTables> {
 public:
  using MDSStub = vizier::services::metadata::MetadataService::Stub;
  using SchemaResponse = vizier::services::metadata::SchemaResponse;
  GetTables() = delete;
  GetTables(std::shared_ptr<MDSStub> stub,
            std::function<void(grpc::ClientContext*)> add_context_authentication)
      : idx_(0), stub_(stub), add_context_authentication_func_(add_context_authentication) {}

  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ONE_KELVIN; }

  static constexpr auto OutputRelation() {
    return MakeArray(ColInfo("table_name", types::DataType::STRING, types::PatternType::GENERAL,
                             "The table name"),
                     ColInfo("table_desc", types::DataType::STRING, types::PatternType::GENERAL,
                             "Description of the table"),
                     ColInfo("table_metadata", types::DataType::STRING, types::PatternType::GENERAL,
                             "Metadata of the table in JSON"));
  }

  Status Init(FunctionContext*) {
    px::vizier::services::metadata::SchemaRequest req;
    px::vizier::services::metadata::SchemaResponse resp;

    grpc::ClientContext ctx;
    add_context_authentication_func_(&ctx);
    auto s = stub_->GetSchemas(&ctx, req, &resp);
    if (!s.ok()) {
      return error::Internal("Failed to make RPC call to metadata service");
    }

    for (const auto& [table_name, rel] : resp.schema().relation_map()) {
      table_info_.emplace_back(table_name, rel.desc(), rel.mutation_id());
    }
    return Status::OK();
  }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    if (!table_info_.size()) {
      return false;
    }
    const auto& r = table_info_[idx_];
    rw->Append<IndexOf("table_name")>(r.table_name);
    rw->Append<IndexOf("table_desc")>(r.table_desc);
    rw->Append<IndexOf("table_metadata")>(r.table_metadata);

    idx_++;
    return idx_ < static_cast<int>(table_info_.size());
  }

 private:
  struct TableInfo {
    TableInfo(const std::string& table_name, const std::string& table_desc,
              const std::string& table_metadata)
        : table_name(table_name), table_desc(table_desc), table_metadata(table_metadata) {}
    std::string table_name;
    std::string table_desc;
    std::string table_metadata;
  };

  int idx_ = 0;
  std::vector<TableInfo> table_info_;
  std::shared_ptr<MDSStub> stub_;
  std::function<void(grpc::ClientContext*)> add_context_authentication_func_;
};

/**
 * This UDTF fetches all the schemas that are available to query from the MDS.
 */
class GetTableSchemas final : public carnot::udf::UDTF<GetTableSchemas> {
 public:
  using MDSStub = vizier::services::metadata::MetadataService::Stub;
  using SchemaResponse = vizier::services::metadata::SchemaResponse;
  GetTableSchemas() = delete;
  GetTableSchemas(std::shared_ptr<MDSStub> stub,
                  std::function<void(grpc::ClientContext*)> add_context_authentication)
      : idx_(0), stub_(stub), add_context_authentication_func_(add_context_authentication) {}

  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ONE_KELVIN; }

  static constexpr auto OutputRelation() {
    return MakeArray(ColInfo("table_name", types::DataType::STRING, types::PatternType::GENERAL,
                             "The table name"),
                     ColInfo("column_name", types::DataType::STRING, types::PatternType::GENERAL,
                             "The name of the column"),
                     ColInfo("column_type", types::DataType::STRING, types::PatternType::GENERAL,
                             "The type of the column"),
                     ColInfo("pattern_type", types::DataType::STRING, types::PatternType::GENERAL,
                             "The pattern type of the metric"),
                     ColInfo("column_desc", types::DataType::STRING, types::PatternType::GENERAL,
                             "Description of the column"));
  }

  Status Init(FunctionContext*) {
    px::vizier::services::metadata::SchemaRequest req;
    px::vizier::services::metadata::SchemaResponse resp;

    grpc::ClientContext ctx;
    add_context_authentication_func_(&ctx);
    auto s = stub_->GetSchemas(&ctx, req, &resp);
    if (!s.ok()) {
      return error::Internal("Failed to make RPC call to metadata service");
    }

    // TODO(zasgar): We store the data since it's hard to traverse two maps at once. We should
    // either do that or perhaps have an interface that allows UDTFs to write multiple records in
    // a single invocation.
    for (const auto& [table_name, rel] : resp.schema().relation_map()) {
      for (const auto& col : rel.columns()) {
        relation_info_.emplace_back(
            table_name, col.column_name(), std::string(magic_enum::enum_name(col.column_type())),
            std::string(types::ToString(col.pattern_type())), col.column_desc());
      }
    }
    return Status::OK();
  }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    if (!relation_info_.size()) {
      return false;
    }
    const auto& r = relation_info_[idx_];
    rw->Append<IndexOf("table_name")>(r.table_name);
    rw->Append<IndexOf("column_name")>(r.column_name);
    rw->Append<IndexOf("column_type")>(r.column_type);
    rw->Append<IndexOf("pattern_type")>(r.pattern_type);
    rw->Append<IndexOf("column_desc")>(r.column_desc);

    idx_++;
    return idx_ < static_cast<int>(relation_info_.size());
  }

 private:
  struct RelationInfo {
    RelationInfo(const std::string& table_name, const std::string& column_name,
                 const std::string& column_type, const std::string& pattern_type,
                 const std::string& column_desc)
        : table_name(table_name),
          column_name(column_name),
          column_type(column_type),
          pattern_type(pattern_type),
          column_desc(column_desc) {}
    std::string table_name;
    std::string column_name;
    std::string column_type;
    std::string pattern_type;
    std::string column_desc;
  };

  int idx_ = 0;
  std::vector<RelationInfo> relation_info_;
  std::shared_ptr<MDSStub> stub_;
  std::function<void(grpc::ClientContext*)> add_context_authentication_func_;
};

/**
 * This UDTF shows the status of each agent.
 */
class GetAgentStatus final : public carnot::udf::UDTF<GetAgentStatus> {
 public:
  using MDSStub = vizier::services::metadata::MetadataService::Stub;
  using SchemaResponse = vizier::services::metadata::SchemaResponse;
  GetAgentStatus() = delete;
  GetAgentStatus(std::shared_ptr<MDSStub> stub,
                 std::function<void(grpc::ClientContext*)> add_context_authentication)
      : idx_(0), stub_(stub), add_context_authentication_func_(add_context_authentication) {}

  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ONE_KELVIN; }

  static constexpr auto OutputRelation() {
    return MakeArray(
        ColInfo("agent_id", types::DataType::UINT128, types::PatternType::GENERAL,
                "The id of the agent"),
        ColInfo("asid", types::DataType::INT64, types::PatternType::GENERAL, "The Agent Short ID"),
        ColInfo("hostname", types::DataType::STRING, types::PatternType::GENERAL,
                "The hostname of the agent"),
        ColInfo("ip_address", types::DataType::STRING, types::PatternType::GENERAL,
                "The IP address of the agent"),
        ColInfo("agent_state", types::DataType::STRING, types::PatternType::GENERAL,
                "The current health status of the agent"),
        ColInfo("create_time", types::DataType::TIME64NS, types::PatternType::GENERAL,
                "The creation time of the agent"),
        ColInfo("last_heartbeat_ns", types::DataType::INT64, types::PatternType::GENERAL,
                "Time (in nanoseconds) since the last heartbeat"),
        ColInfo("kernel_headers_installed", types::DataType::BOOLEAN, types::PatternType::GENERAL,
                kKernelHeadersInstalledDesc));
  }

  Status Init(FunctionContext*, types::BoolValue include_kelvin) {
    px::vizier::services::metadata::AgentInfoRequest req;
    resp_ = std::make_unique<px::vizier::services::metadata::AgentInfoResponse>();
    include_kelvin_ = include_kelvin.val;

    grpc::ClientContext ctx;
    add_context_authentication_func_(&ctx);
    auto s = stub_->GetAgentInfo(&ctx, req, resp_.get());
    if (!s.ok()) {
      return error::Internal("Failed to make RPC call to GetAgentInfo");
    }
    return Status::OK();
  }

  static constexpr auto InitArgs() {
    return MakeArray(UDTFArg::Make<types::BOOLEAN>(
        "include_kelvin", "Whether to include Kelvin agents in the output", true));
  }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    const auto& agent_metadata = resp_->info(idx_);
    const auto& agent_info = agent_metadata.agent();
    const auto& agent_status = agent_metadata.status();

    auto u_or_s = ParseUUID(agent_info.info().agent_id());
    sole::uuid u;
    if (u_or_s.ok()) {
      u = u_or_s.ConsumeValueOrDie();
    }
    // TODO(zasgar): Figure out abort mechanism;

    auto host_info = agent_info.info().host_info();
    auto collects_data = agent_info.info().capabilities().collects_data();
    if (collects_data || include_kelvin_) {
      rw->Append<IndexOf("agent_id")>(absl::MakeUint128(u.ab, u.cd));
      rw->Append<IndexOf("asid")>(agent_info.asid());
      rw->Append<IndexOf("hostname")>(host_info.hostname());
      rw->Append<IndexOf("ip_address")>(agent_info.info().ip_address());
      rw->Append<IndexOf("agent_state")>(StringValue(magic_enum::enum_name(agent_status.state())));
      rw->Append<IndexOf("create_time")>(agent_info.create_time_ns());
      rw->Append<IndexOf("last_heartbeat_ns")>(agent_status.ns_since_last_heartbeat());
      rw->Append<IndexOf("kernel_headers_installed")>(host_info.kernel_headers_installed());
    }

    ++idx_;
    return idx_ < resp_->info_size();
  }

 private:
  int idx_ = 0;
  bool include_kelvin_ = false;
  std::unique_ptr<px::vizier::services::metadata::AgentInfoResponse> resp_;
  std::shared_ptr<MDSStub> stub_;
  std::function<void(grpc::ClientContext*)> add_context_authentication_func_;
};

/**
 * This UDTF gets the profiler stack trace sampling period.
 */
class GetProfilerSamplingPeriodMS final : public carnot::udf::UDTF<GetProfilerSamplingPeriodMS> {
 public:
  using MDSStub = vizier::services::metadata::MetadataService::Stub;
  using SchemaResponse = vizier::services::metadata::SchemaResponse;
  GetProfilerSamplingPeriodMS() = delete;
  GetProfilerSamplingPeriodMS(std::shared_ptr<MDSStub> stub,
                              std::function<void(grpc::ClientContext*)> add_context_authentication)
      : idx_(0), stub_(stub), add_context_authentication_func_(add_context_authentication) {}

  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ONE_KELVIN; }

  static constexpr auto OutputRelation() {
    return MakeArray(
        ColInfo("asid", types::DataType::INT64, types::PatternType::GENERAL, "The Agent Short ID"),
        ColInfo("profiler_sampling_period_ms", types::DataType::INT64, types::PatternType::GENERAL,
                "The sampling period in ms."));
  }

  Status Init(FunctionContext*) {
    px::vizier::services::metadata::AgentInfoRequest req;
    resp_ = std::make_unique<px::vizier::services::metadata::AgentInfoResponse>();

    grpc::ClientContext ctx;
    add_context_authentication_func_(&ctx);
    auto s = stub_->GetAgentInfo(&ctx, req, resp_.get());
    if (!s.ok()) {
      return error::Internal("Failed to make RPC call to GetAgentInfo");
    }
    return Status::OK();
  }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    const auto& agent_metadata = resp_->info(idx_);
    const auto& agent_info = agent_metadata.agent();

    const auto asid = agent_info.asid();
    const auto period_ms = agent_info.info().parameters().profiler_stack_trace_sample_period_ms();
    rw->Append<IndexOf("asid")>(asid);
    rw->Append<IndexOf("profiler_sampling_period_ms")>(period_ms);

    ++idx_;
    return idx_ < resp_->info_size();
  }

 private:
  int idx_ = 0;
  std::unique_ptr<px::vizier::services::metadata::AgentInfoResponse> resp_;
  std::shared_ptr<MDSStub> stub_;
  std::function<void(grpc::ClientContext*)> add_context_authentication_func_;
};

/**
 * This UDTF retrieves the status of the agents' Linux headers installation.
 */
class GetLinuxHeadersStatus final : public carnot::udf::UDTF<GetLinuxHeadersStatus> {
 public:
  using MDSStub = vizier::services::metadata::MetadataService::Stub;
  using SchemaResponse = vizier::services::metadata::SchemaResponse;
  GetLinuxHeadersStatus() = delete;
  GetLinuxHeadersStatus(std::shared_ptr<MDSStub> stub,
                        std::function<void(grpc::ClientContext*)> add_context_authentication)
      : idx_(0), stub_(stub), add_context_authentication_func_(add_context_authentication) {}

  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ONE_KELVIN; }

  static constexpr auto OutputRelation() {
    return MakeArray(
        ColInfo("asid", types::DataType::INT64, types::PatternType::GENERAL, "The Agent Short ID"),
        ColInfo("kernel_headers_installed", types::DataType::BOOLEAN, types::PatternType::GENERAL,
                kKernelHeadersInstalledDesc));
  }

  Status Init(FunctionContext*, BoolValue include_kelvin) {
    px::vizier::services::metadata::AgentInfoRequest req;
    resp_ = std::make_unique<px::vizier::services::metadata::AgentInfoResponse>();
    include_kelvin_ = include_kelvin.val;

    grpc::ClientContext ctx;
    add_context_authentication_func_(&ctx);
    auto s = stub_->GetAgentInfo(&ctx, req, resp_.get());
    if (!s.ok()) {
      return error::Internal("Failed to make RPC call to GetAgentInfo");
    }
    return Status::OK();
  }

  static constexpr auto InitArgs() {
    return MakeArray(UDTFArg::Make<types::BOOLEAN>(
        "include_kelvin", "Whether to include Kelvin agents in the output", true));
  }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    const auto& agent_metadata = resp_->info(idx_);
    const auto& agent_info = agent_metadata.agent();

    const auto asid = agent_info.asid();
    auto collects_data = agent_info.info().capabilities().collects_data();
    const auto host_info = agent_info.info().host_info();
    const auto kernel_headers_installed = host_info.kernel_headers_installed();
    if (collects_data || include_kelvin_) {
      rw->Append<IndexOf("asid")>(asid);
      rw->Append<IndexOf("kernel_headers_installed")>(kernel_headers_installed);
    }

    ++idx_;
    return idx_ < resp_->info_size();
  }

 private:
  int idx_ = 0;
  bool include_kelvin_ = false;
  std::unique_ptr<px::vizier::services::metadata::AgentInfoResponse> resp_;
  std::shared_ptr<MDSStub> stub_;
  std::function<void(grpc::ClientContext*)> add_context_authentication_func_;
};

namespace internal {
inline rapidjson::GenericStringRef<char> StringRef(std::string_view s) {
  return rapidjson::GenericStringRef<char>(s.data(), s.size());
}

}  // namespace internal

class GetUDTFList final : public carnot::udf::UDTF<GetUDTFList> {
 public:
  GetUDTFList() = delete;
  explicit GetUDTFList(const carnot::udf::Registry* func_registry)
      : registry_map_(func_registry->map()), registry_map_iter_(registry_map_.begin()) {}

  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ONE_KELVIN; }

  static constexpr auto OutputRelation() {
    return MakeArray(ColInfo("name", types::DataType::STRING, types::PatternType::GENERAL,
                             "The name of the UDTF"),
                     ColInfo("executor", types::DataType::STRING, types::PatternType::GENERAL,
                             "The location where the UDTF is executed"),
                     ColInfo("init_args", types::DataType::STRING, types::PatternType::GENERAL,
                             "The init arguments to the UDTF"),
                     ColInfo("output_relation", types::DataType::STRING,
                             types::PatternType::GENERAL, "The output relation of the UDTF"));
  }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    while (registry_map_iter_ != registry_map_.end()) {
      if (registry_map_iter_->second->kind() != carnot::udf::UDFDefinitionKind::kUDTF) {
        ++registry_map_iter_;
        continue;
      }
      auto* udtf_def =
          static_cast<carnot::udf::UDTFDefinition*>(registry_map_iter_->second->GetDefinition());

      if (absl::StartsWith(udtf_def->name(), "_")) {
        // Hide "internal" funcs.
        ++registry_map_iter_;
        continue;
      }

      rapidjson::Document init_args;
      init_args.SetObject();
      rapidjson::Value init_args_arr(rapidjson::kArrayType);
      for (const auto& arg : udtf_def->init_arguments()) {
        rapidjson::Value val(rapidjson::kObjectType);
        val.AddMember("name", internal::StringRef(arg.name()), init_args.GetAllocator());
        val.AddMember("type", internal::StringRef(magic_enum::enum_name(arg.type())),
                      init_args.GetAllocator());
        val.AddMember("stype", internal::StringRef(magic_enum::enum_name(arg.stype())),
                      init_args.GetAllocator());
        val.AddMember("desc", internal::StringRef(arg.desc()), init_args.GetAllocator());

        init_args_arr.PushBack(val.Move(), init_args.GetAllocator());
      }
      init_args.AddMember("args", init_args_arr.Move(), init_args.GetAllocator());

      rapidjson::Document relation;
      relation.SetObject();
      rapidjson::Value relation_arr(rapidjson::kArrayType);
      for (const auto& arg : udtf_def->output_relation()) {
        rapidjson::Value val(rapidjson::kObjectType);

        val.AddMember("name", internal::StringRef(arg.name()), relation.GetAllocator());
        val.AddMember("type", internal::StringRef(magic_enum::enum_name(arg.type())),
                      relation.GetAllocator());
        val.AddMember("ptype", internal::StringRef(magic_enum::enum_name(arg.ptype())),
                      relation.GetAllocator());
        val.AddMember("desc", internal::StringRef(arg.desc()), relation.GetAllocator());

        relation_arr.PushBack(val.Move(), init_args.GetAllocator());
      }
      relation.AddMember("relation", relation_arr.Move(), relation.GetAllocator());

      rapidjson::StringBuffer init_args_sb;
      rapidjson::Writer<rapidjson::StringBuffer> init_args_writer(init_args_sb);
      init_args.Accept(init_args_writer);

      rapidjson::StringBuffer relation_sb;
      rapidjson::Writer<rapidjson::StringBuffer> relation_writer(relation_sb);
      relation.Accept(relation_writer);

      rw->Append<IndexOf("name")>(udtf_def->name());
      rw->Append<IndexOf("executor")>(std::string(magic_enum::enum_name(udtf_def->executor())));
      rw->Append<IndexOf("init_args")>(init_args_sb.GetString());
      rw->Append<IndexOf("output_relation")>(relation_sb.GetString());

      ++registry_map_iter_;
      break;
    }

    return registry_map_iter_ != registry_map_.end();
  }

 private:
  const carnot::udf::Registry::RegistryMap& registry_map_;
  carnot::udf::Registry::RegistryMap::const_iterator registry_map_iter_;
};

class GetUDFList final : public carnot::udf::UDTF<GetUDFList> {
 public:
  GetUDFList() = delete;
  explicit GetUDFList(const carnot::udf::Registry* func_registry)
      : registry_map_(func_registry->map()), registry_map_iter_(registry_map_.begin()) {}

  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ONE_KELVIN; }

  static constexpr auto OutputRelation() {
    return MakeArray(ColInfo("name", types::DataType::STRING, types::PatternType::GENERAL,
                             "The name of the Scalar UDF"),
                     ColInfo("return_type", types::DataType::STRING, types::PatternType::GENERAL,
                             "The return type of the Scalar UDF"),
                     ColInfo("args", types::DataType::STRING, types::PatternType::GENERAL,
                             "The argument types of the scalar UDF"));
  }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    while (registry_map_iter_ != registry_map_.end()) {
      if (registry_map_iter_->second->kind() != carnot::udf::UDFDefinitionKind::kScalarUDF) {
        ++registry_map_iter_;
        continue;
      }
      auto* udf_def = static_cast<carnot::udf::ScalarUDFDefinition*>(
          registry_map_iter_->second->GetDefinition());

      if (absl::StartsWith(udf_def->name(), "_")) {
        // Hide "internal" funcs.
        ++registry_map_iter_;
        continue;
      }

      rapidjson::Document args;
      args.SetObject();
      rapidjson::Value args_arr(rapidjson::kArrayType);
      for (const auto& arg : udf_def->exec_arguments()) {
        args_arr.PushBack(internal::StringRef(magic_enum::enum_name(arg)), args.GetAllocator());
      }
      args.AddMember("args", args_arr.Move(), args.GetAllocator());

      rapidjson::StringBuffer args_sb;
      rapidjson::Writer<rapidjson::StringBuffer> args_writer(args_sb);
      args.Accept(args_writer);

      rw->Append<IndexOf("name")>(udf_def->name());
      rw->Append<IndexOf("return_type")>(
          std::string(magic_enum::enum_name(udf_def->exec_return_type())));
      rw->Append<IndexOf("args")>(args_sb.GetString());

      ++registry_map_iter_;
      break;
    }

    return registry_map_iter_ != registry_map_.end();
  }

 private:
  const carnot::udf::Registry::RegistryMap& registry_map_;
  carnot::udf::Registry::RegistryMap::const_iterator registry_map_iter_;
};

class GetUDAList final : public carnot::udf::UDTF<GetUDAList> {
 public:
  GetUDAList() = delete;
  explicit GetUDAList(const carnot::udf::Registry* func_registry)
      : registry_map_(func_registry->map()), registry_map_iter_(registry_map_.begin()) {}

  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ONE_KELVIN; }

  static constexpr auto OutputRelation() {
    return MakeArray(ColInfo("name", types::DataType::STRING, types::PatternType::GENERAL,
                             "The name of the UDA"),
                     ColInfo("return_type", types::DataType::STRING, types::PatternType::GENERAL,
                             "The return type of the UDA"),
                     ColInfo("args", types::DataType::STRING, types::PatternType::GENERAL,
                             "The argument types of the UDA"));
  }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    while (registry_map_iter_ != registry_map_.end()) {
      if (registry_map_iter_->second->kind() != carnot::udf::UDFDefinitionKind::kUDA) {
        ++registry_map_iter_;
        continue;
      }
      auto* uda_def =
          static_cast<carnot::udf::UDADefinition*>(registry_map_iter_->second->GetDefinition());

      if (absl::StartsWith(uda_def->name(), "_")) {
        // Hide "internal" funcs.
        ++registry_map_iter_;
        continue;
      }

      rapidjson::Document args;
      args.SetObject();
      rapidjson::Value args_arr(rapidjson::kArrayType);
      for (const auto& arg : uda_def->update_arguments()) {
        args_arr.PushBack(internal::StringRef(magic_enum::enum_name(arg)), args.GetAllocator());
      }
      args.AddMember("args", args_arr.Move(), args.GetAllocator());

      rapidjson::StringBuffer args_sb;
      rapidjson::Writer<rapidjson::StringBuffer> args_writer(args_sb);
      args.Accept(args_writer);

      rw->Append<IndexOf("name")>(uda_def->name());
      rw->Append<IndexOf("return_type")>(
          std::string(magic_enum::enum_name(uda_def->finalize_return_type())));
      rw->Append<IndexOf("args")>(args_sb.GetString());

      ++registry_map_iter_;
      break;
    }

    return registry_map_iter_ != registry_map_.end();
  }

 private:
  const carnot::udf::Registry::RegistryMap& registry_map_;
  carnot::udf::Registry::RegistryMap::const_iterator registry_map_iter_;
};

/**
 * This UDTF dumps the debug string for the current metadata state.
 */
class GetDebugMDState final : public carnot::udf::UDTF<GetDebugMDState> {
 public:
  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }

  static constexpr auto OutputRelation() {
    return MakeArray(ColInfo("asid", types::DataType::INT64, types::PatternType::GENERAL,
                             "The short ID of the agent"),
                     ColInfo("pod_name", types::DataType::STRING, types::PatternType::GENERAL,
                             "The pod name of the agent"),
                     ColInfo("debug_state", types::DataType::STRING, types::PatternType::GENERAL,
                             "The debug state of metadata on the agent"));
  }

  bool NextRecord(FunctionContext* ctx, RecordWriter* rw) {
    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("pod_name")>(ctx->metadata_state()->pod_name());
    rw->Append<IndexOf("debug_state")>(ctx->metadata_state()->DebugString());
    // no more records.
    return false;
  }
};

/**
 * This UDTF fetches all MDS kvs with the given prefix.
 */
class GetDebugMDWithPrefix final : public carnot::udf::UDTF<GetDebugMDWithPrefix> {
 public:
  using MDSStub = vizier::services::metadata::MetadataService::Stub;
  GetDebugMDWithPrefix() = delete;
  GetDebugMDWithPrefix(std::shared_ptr<MDSStub> stub,
                       std::function<void(grpc::ClientContext*)> add_context_authentication)
      : stub_(stub), add_context_authentication_func_(add_context_authentication) {}

  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ONE_KELVIN; }

  static constexpr auto OutputRelation() {
    return MakeArray(ColInfo("key", types::DataType::STRING, types::PatternType::GENERAL,
                             "The key of the object"),
                     ColInfo("value", types::DataType::STRING, types::PatternType::GENERAL,
                             "Text encoded version of the proto"));
  }

  static constexpr auto InitArgs() {
    return MakeArray(
        UDTFArg::Make<types::DataType::STRING>("prefix", "Prefix key for metadata info"),
        UDTFArg::Make<types::DataType::STRING>(
            "proto", "Fully qualified proto message name to decode values"));
  }

  Status Init(FunctionContext*, types::StringValue prefix, types::StringValue proto) {
    px::vizier::services::metadata::WithPrefixKeyRequest req;
    resp_ = std::make_unique<px::vizier::services::metadata::WithPrefixKeyResponse>();
    idx_ = 0;

    req.set_prefix(prefix);
    req.set_proto(proto);

    grpc::ClientContext ctx;
    add_context_authentication_func_(&ctx);
    auto s = stub_->GetWithPrefixKey(&ctx, req, resp_.get());
    if (!s.ok()) {
      return error::Internal(s.error_message());
    }
    return Status::OK();
  }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    if (idx_ >= resp_->kvs().size()) {
      return false;
    }

    rw->Append<IndexOf("key")>(resp_->kvs().Get(idx_).key());
    rw->Append<IndexOf("value")>(resp_->kvs().Get(idx_).value());
    ++idx_;

    return idx_ < resp_->kvs().size();
  }

 private:
  std::unique_ptr<px::vizier::services::metadata::WithPrefixKeyResponse> resp_;
  int idx_;

  std::shared_ptr<MDSStub> stub_;
  std::function<void(grpc::ClientContext*)> add_context_authentication_func_;
};

/**
 * This UDTF dumps the debug information for all registered tables.
 */
class GetDebugTableInfo final : public carnot::udf::UDTF<GetDebugTableInfo> {
 public:
  GetDebugTableInfo() = delete;
  explicit GetDebugTableInfo(const ::px::table_store::TableStore* table_store)
      : table_store_(table_store) {}
  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ALL_AGENTS; }

  static constexpr auto OutputRelation() {
    return MakeArray(
        ColInfo("asid", types::DataType::INT64, types::PatternType::GENERAL,
                "The short ID of the agent"),
        ColInfo("name", types::DataType::STRING, types::PatternType::GENERAL,
                "The name of this table"),
        ColInfo("id", types::DataType::INT64, types::PatternType::GENERAL, "The id of the table"),
        ColInfo("batches_added", types::DataType::INT64, types::PatternType::GENERAL,
                "The number of batches added to this table in its lifetime"),
        ColInfo("batches_expired", types::DataType::INT64, types::PatternType::GENERAL,
                "The number of batches expired from this table in its lifetime"),
        ColInfo("bytes_added", types::DataType::INT64, types::PatternType::GENERAL,
                "The number of bytes added to this table in its lifetime"),
        ColInfo("num_batches", types::DataType::INT64, types::PatternType::GENERAL,
                "The number of batches active in this table"),
        ColInfo("compacted_batches", types::DataType::INT64, types::PatternType::GENERAL,
                "The number of compacted batches that have been added to cold storage."),
        ColInfo("size", types::DataType::INT64, types::PatternType::GENERAL,
                "The size of this table in bytes"),
        ColInfo("cold_size", types::DataType::INT64, types::PatternType::GENERAL,
                "The number of bytes in cold storage"),
        ColInfo("max_table_size", types::DataType::INT64, types::PatternType::GENERAL,
                "The maximum size of this table"),
        ColInfo("min_time", types::DataType::TIME64NS, types::PatternType::GENERAL,
                "The minimum timestamp currently present in this table. -1 if there is no time_ "
                "column on the table."));
  }
  Status Init(FunctionContext*) {
    table_ids_ = table_store_->GetTableIDs();
    return Status::OK();
  }

  bool NextRecord(FunctionContext* ctx, RecordWriter* rw) {
    if (static_cast<size_t>(current_idx_) >= table_ids_.size()) {
      return false;
    }

    uint64_t selected_id = table_ids_[current_idx_];
    const auto* table = table_store_->GetTable(selected_id);
    auto info = table->GetTableStats();

    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("name")>(table_store_->GetTableName(selected_id));
    rw->Append<IndexOf("id")>(selected_id);
    rw->Append<IndexOf("batches_added")>(info.batches_added);
    rw->Append<IndexOf("batches_expired")>(info.batches_expired);
    rw->Append<IndexOf("bytes_added")>(info.bytes_added);
    rw->Append<IndexOf("num_batches")>(info.num_batches);
    rw->Append<IndexOf("compacted_batches")>(info.compacted_batches);
    rw->Append<IndexOf("size")>(info.bytes);
    rw->Append<IndexOf("cold_size")>(info.cold_bytes);
    rw->Append<IndexOf("max_table_size")>(info.max_table_size);
    rw->Append<IndexOf("min_time")>(info.min_time);

    ++current_idx_;
    return static_cast<size_t>(current_idx_) < table_ids_.size();
  }

 private:
  const ::px::table_store::TableStore* table_store_;
  int current_idx_ = 0;
  std::vector<uint64_t> table_ids_;
};

/**
 * This UDTF fetches information about tracepoints from MDS.
 */
class GetTracepointStatus final : public carnot::udf::UDTF<GetTracepointStatus> {
 public:
  using MDTPStub = vizier::services::metadata::MetadataTracepointService::Stub;
  using TracepointResponse = vizier::services::metadata::GetTracepointInfoResponse;
  GetTracepointStatus() = delete;
  explicit GetTracepointStatus(std::shared_ptr<MDTPStub> stub,
                               std::function<void(grpc::ClientContext*)> add_context_authentication)
      : idx_(0), stub_(stub), add_context_authentication_func_(add_context_authentication) {}

  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ONE_KELVIN; }

  static constexpr auto OutputRelation() {
    return MakeArray(ColInfo("tracepoint_id", types::DataType::UINT128, types::PatternType::GENERAL,
                             "The id of the tracepoint"),
                     ColInfo("tracepoint_id_str", types::DataType::STRING, types::PatternType::GENERAL,
                             "The string id of the tracepoint"),
                     ColInfo("name", types::DataType::STRING, types::PatternType::GENERAL,
                             "The name of the tracepoint"),
                     ColInfo("state", types::DataType::STRING, types::PatternType::GENERAL,
                             "The status of the tracepoint"),
                     ColInfo("status", types::DataType::STRING, types::PatternType::GENERAL,
                             "The status message if not healthy"),
                     ColInfo("output_tables", types::DataType::STRING, types::PatternType::GENERAL,
                             "A list of tables output by the tracepoint"));
    // TODO(zasgar): Add in the create time, and TTL in here after we add those attributes to the
    // GetTracepointInfo RPC call in MDS.
  }

  Status Init(FunctionContext*) {
    px::vizier::services::metadata::GetTracepointInfoRequest req;
    resp_ = std::make_unique<px::vizier::services::metadata::GetTracepointInfoResponse>();

    grpc::ClientContext ctx;
    add_context_authentication_func_(&ctx);
    auto s = stub_->GetTracepointInfo(&ctx, req, resp_.get());
    if (!s.ok()) {
      return error::Internal("Failed to make RPC call to GetTracepointStatus: $0",
                             s.error_message());
    }
    return Status::OK();
  }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    if (resp_->tracepoints_size() == 0) {
      return false;
    }
    const auto& tracepoint_info = resp_->tracepoints(idx_);

    auto u_or_s = ParseUUID(tracepoint_info.id());
    sole::uuid u;
    if (u_or_s.ok()) {
      u = u_or_s.ConsumeValueOrDie();
    }

    auto actual = tracepoint_info.state();
    auto expected = tracepoint_info.expected_state();
    std::string state;

    switch (actual) {
      case statuspb::PENDING_STATE: {
        state = "pending";
        break;
      }
      case statuspb::RUNNING_STATE: {
        state = "running";
        break;
      }
      case statuspb::FAILED_STATE: {
        state = "failed";
        break;
      }
      case statuspb::TERMINATED_STATE: {
        if (actual != expected) {
          state = "terminating";
        } else {
          state = "terminated";
        }
        break;
      }
      default:
        state = "unknown";
    }

    rapidjson::Document tables;
    tables.SetArray();
    for (const auto& table : tracepoint_info.schema_names()) {
      tables.PushBack(internal::StringRef(table), tables.GetAllocator());
    }

    rapidjson::StringBuffer tables_sb;
    rapidjson::Writer<rapidjson::StringBuffer> tables_writer(tables_sb);
    tables.Accept(tables_writer);

    rw->Append<IndexOf("tracepoint_id")>(absl::MakeUint128(u.ab, u.cd));
    rw->Append<IndexOf("tracepoint_id_str")>(u.str());
    rw->Append<IndexOf("name")>(tracepoint_info.name());
    rw->Append<IndexOf("state")>(state);

    rapidjson::Document statuses;
    statuses.SetArray();
    for (const auto& status : tracepoint_info.statuses()) {
      statuses.PushBack(internal::StringRef(status.msg()), statuses.GetAllocator());
    }
    rapidjson::StringBuffer statuses_sb;
    rapidjson::Writer<rapidjson::StringBuffer> statuses_writer(statuses_sb);
    statuses.Accept(statuses_writer);
    rw->Append<IndexOf("status")>(statuses_sb.GetString());

    rw->Append<IndexOf("output_tables")>(tables_sb.GetString());

    ++idx_;
    return idx_ < resp_->tracepoints_size();
  }

 private:
  int idx_ = 0;
  std::unique_ptr<px::vizier::services::metadata::GetTracepointInfoResponse> resp_;
  std::shared_ptr<MDTPStub> stub_;
  std::function<void(grpc::ClientContext*)> add_context_authentication_func_;
};

/**
 * This UDTF fetches information about tracepoints from MDS.
 */
class GetFileSourceStatus final : public carnot::udf::UDTF<GetFileSourceStatus> {
 public:
  using MDFSStub = vizier::services::metadata::MetadataFileSourceService::Stub;
  using FileSourceResponse = vizier::services::metadata::GetFileSourceInfoResponse;
  GetFileSourceStatus() = delete;
  explicit GetFileSourceStatus(std::shared_ptr<MDFSStub> stub,
                               std::function<void(grpc::ClientContext*)> add_context_authentication)
      : idx_(0), stub_(stub), add_context_authentication_func_(add_context_authentication) {}

  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ONE_KELVIN; }

  static constexpr auto OutputRelation() {
    // TODO(ddelnano): Change the file_source_id column to a UINT128 once the pxl lookup from
    // px/pipeline_flow_graph works. That script has a UINT128 stored as a string and needs to
    // be joined with this column
    return MakeArray(ColInfo("file_source_id", types::DataType::STRING,
                             types::PatternType::GENERAL, "The id of the file source"),
                     ColInfo("name", types::DataType::STRING, types::PatternType::GENERAL,
                             "The name of the file source"),
                     ColInfo("state", types::DataType::STRING, types::PatternType::GENERAL,
                             "The state of the file source"),
                     ColInfo("status", types::DataType::STRING, types::PatternType::GENERAL,
                             "The status message if not healthy"),
                     ColInfo("output_tables", types::DataType::STRING, types::PatternType::GENERAL,
                             "A list of tables output by the file source"));
    // TODO(ddelnano): Add in the create time, and TTL in here after we add those attributes to the
    // GetFileSourceInfo RPC call in MDS.
  }

  Status Init(FunctionContext*) {
    px::vizier::services::metadata::GetFileSourceInfoRequest req;
    resp_ = std::make_unique<px::vizier::services::metadata::GetFileSourceInfoResponse>();

    grpc::ClientContext ctx;
    add_context_authentication_func_(&ctx);
    auto s = stub_->GetFileSourceInfo(&ctx, req, resp_.get());
    if (!s.ok()) {
      return error::Internal("Failed to make RPC call to GetFileSourceStatus: $0",
                             s.error_message());
    }
    return Status::OK();
  }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    if (resp_->file_sources_size() == 0) {
      return false;
    }
    const auto& file_source_info = resp_->file_sources(idx_);

    auto u_or_s = ParseUUID(file_source_info.id());
    sole::uuid u;
    if (u_or_s.ok()) {
      u = u_or_s.ConsumeValueOrDie();
    }

    auto actual = file_source_info.state();
    auto expected = file_source_info.expected_state();
    std::string state;

    switch (actual) {
      case statuspb::PENDING_STATE: {
        state = "pending";
        break;
      }
      case statuspb::RUNNING_STATE: {
        state = "running";
        break;
      }
      case statuspb::FAILED_STATE: {
        state = "failed";
        break;
      }
      case statuspb::TERMINATED_STATE: {
        if (actual != expected) {
          state = "terminating";
        } else {
          state = "terminated";
        }
        break;
      }
      default:
        state = "unknown";
    }

    rapidjson::Document tables;
    tables.SetArray();
    for (const auto& table : file_source_info.schema_names()) {
      tables.PushBack(internal::StringRef(table), tables.GetAllocator());
    }

    rapidjson::StringBuffer tables_sb;
    rapidjson::Writer<rapidjson::StringBuffer> tables_writer(tables_sb);
    tables.Accept(tables_writer);

    rw->Append<IndexOf("file_source_id")>(u.str());
    rw->Append<IndexOf("name")>(file_source_info.name());
    rw->Append<IndexOf("state")>(state);

    rapidjson::Document statuses;
    statuses.SetArray();
    for (const auto& status : file_source_info.statuses()) {
      statuses.PushBack(internal::StringRef(status.msg()), statuses.GetAllocator());
    }
    rapidjson::StringBuffer statuses_sb;
    rapidjson::Writer<rapidjson::StringBuffer> statuses_writer(statuses_sb);
    statuses.Accept(statuses_writer);
    rw->Append<IndexOf("status")>(statuses_sb.GetString());

    rw->Append<IndexOf("output_tables")>(tables_sb.GetString());

    ++idx_;
    return idx_ < resp_->file_sources_size();
  }

 private:
  int idx_ = 0;
  std::unique_ptr<px::vizier::services::metadata::GetFileSourceInfoResponse> resp_;
  std::shared_ptr<MDFSStub> stub_;
  std::function<void(grpc::ClientContext*)> add_context_authentication_func_;
};

class GetCronScriptHistory final : public carnot::udf::UDTF<GetCronScriptHistory> {
 public:
  using CronScriptStoreStub = vizier::services::metadata::CronScriptStoreService::Stub;
  GetCronScriptHistory() = delete;
  explicit GetCronScriptHistory(
      std::shared_ptr<CronScriptStoreStub> stub,
      std::function<void(grpc::ClientContext*)> add_context_authentication)
      : stub_(stub), add_context_authentication_func_(std::move(add_context_authentication)) {}

  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ONE_KELVIN; }

  static constexpr auto OutputRelation() {
    return MakeArray(
        ColInfo("script_id", types::DataType::STRING, types::PatternType::GENERAL,
                "The id of the cron script"),
        ColInfo("timestamp", types::DataType::TIME64NS, types::PatternType::GENERAL,
                "The time the script ran"),
        ColInfo("error_message", types::DataType::STRING, types::PatternType::GENERAL,
                "The error message if one exists"),
        ColInfo("execution_time_ns", types::DataType::INT64, types::PatternType::GENERAL,
                "The execution time of the script", types::SemanticType::ST_DURATION_NS),
        ColInfo("compilation_time_ns", types::DataType::INT64, types::PatternType::GENERAL,
                "The compiltation time of the script", types::SemanticType::ST_DURATION_NS),
        ColInfo("bytes_processed", types::DataType::INT64, types::PatternType::GENERAL,
                "The number of bytes processed during script execution",
                types::SemanticType::ST_BYTES),
        ColInfo("records_processed", types::DataType::INT64, types::PatternType::GENERAL,
                "The number of records processed during script execution"));
  }

  Status Init(FunctionContext*) {
    px::vizier::services::metadata::GetAllExecutionResultsRequest req;
    resp_ = std::make_unique<px::vizier::services::metadata::GetAllExecutionResultsResponse>();

    grpc::ClientContext ctx;
    add_context_authentication_func_(&ctx);
    auto s = stub_->GetAllExecutionResults(&ctx, req, resp_.get());
    if (!s.ok()) {
      return error::Internal("Failed to make RPC call to GetTracepointStatus: $0",
                             s.error_message());
    }
    return Status::OK();
  }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    if (resp_->results_size() == 0) {
      return false;
    }
    const auto& result = resp_->results(idx_);
    auto u_or_s = ParseUUID(result.script_id());
    sole::uuid u;
    if (u_or_s.ok()) {
      u = u_or_s.ConsumeValueOrDie();
    }

    rw->Append<IndexOf("script_id")>(u.str());
    rw->Append<IndexOf("timestamp")>(types::Time64NSValue(
        result.timestamp().seconds() * 1000000000 + result.timestamp().nanos()));

    if (result.has_error()) {
      rw->Append<IndexOf("error_message")>(
          std::string(absl::Substitute("$0: $1 $2", result.error().err_code(), result.error().msg(),
                                       result.error().context().DebugString())));
      // set to 0.
      rw->Append<IndexOf("execution_time_ns")>(0);
      rw->Append<IndexOf("compilation_time_ns")>(0);
      rw->Append<IndexOf("bytes_processed")>(0);
      rw->Append<IndexOf("records_processed")>(0);
    } else {
      // Set to empty string.
      rw->Append<IndexOf("error_message")>("");
      const auto& exec_stats = result.execution_stats();
      rw->Append<IndexOf("execution_time_ns")>(exec_stats.execution_time_ns());
      rw->Append<IndexOf("compilation_time_ns")>(exec_stats.compilation_time_ns());
      rw->Append<IndexOf("bytes_processed")>(exec_stats.bytes_processed());
      rw->Append<IndexOf("records_processed")>(exec_stats.records_processed());
    }

    ++idx_;
    return idx_ < resp_->results_size();
  }

 private:
  int idx_ = 0;
  std::unique_ptr<px::vizier::services::metadata::GetAllExecutionResultsResponse> resp_;
  std::shared_ptr<CronScriptStoreStub> stub_;
  std::function<void(grpc::ClientContext*)> add_context_authentication_func_;
};

namespace clickhouse_schema {

/**
 * Maps Pixie DataType to ClickHouse type string.
 * Based on the mapping used in carnot_executable.cc for http_events table.
 */
inline std::string PixieTypeToClickHouseType(types::DataType pixie_type,
                                              const std::string& column_name) {
  switch (pixie_type) {
    case types::DataType::INT64:
      return "Int64";
    case types::DataType::FLOAT64:
      return "Float64";
    case types::DataType::STRING:
      return "String";
    case types::DataType::BOOLEAN:
      return "UInt8";
    case types::DataType::TIME64NS:
      // Use DateTime64(9) for time_ column (nanoseconds)
      // Use DateTime64(3) for event_time column (milliseconds)
      if (column_name == "time_") {
        return "DateTime64(9)";
      } else if (column_name == "event_time") {
        return "DateTime64(3)";
      }
      // Default to DateTime64(9) for other time columns
      return "DateTime64(9)";
    case types::DataType::UINT128:
      // ClickHouse doesn't have native UINT128, use String representation (high:low format)
      return "String";
    default:
      return "String";  // Fallback to String for unsupported types
  }
}

}  // namespace clickhouse_schema

/**
 * This UDTF creates ClickHouse schemas from Pixie DataTable schemas.
 * It fetches table schemas from MDS and creates corresponding tables in ClickHouse.
 */
class CreateClickHouseSchemas final : public carnot::udf::UDTF<CreateClickHouseSchemas> {
 public:
  using MDSStub = vizier::services::metadata::MetadataService::Stub;
  using SchemaResponse = vizier::services::metadata::SchemaResponse;

  CreateClickHouseSchemas() = delete;
  CreateClickHouseSchemas(std::shared_ptr<MDSStub> stub,
                          std::function<void(grpc::ClientContext*)> add_context_authentication)
      : idx_(0), stub_(stub), add_context_authentication_func_(add_context_authentication) {}

  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ONE_KELVIN; }

  static constexpr auto OutputRelation() {
    return MakeArray(ColInfo("table_name", types::DataType::STRING, types::PatternType::GENERAL,
                             "The name of the table"),
                     ColInfo("status", types::DataType::STRING, types::PatternType::GENERAL,
                             "Status of the table creation (success/error)"),
                     ColInfo("message", types::DataType::STRING, types::PatternType::GENERAL,
                             "Additional information or error message"));
  }

  static constexpr auto InitArgs() {
    return MakeArray(
        UDTFArg::Make<types::DataType::STRING>("host", "ClickHouse server host", "'localhost'"),
        UDTFArg::Make<types::DataType::INT64>("port", "ClickHouse server port", 9000),
        UDTFArg::Make<types::DataType::STRING>("username", "ClickHouse username", "'default'"),
        UDTFArg::Make<types::DataType::STRING>("password", "ClickHouse password", "'test_password'"),
        UDTFArg::Make<types::DataType::STRING>("database", "ClickHouse database", "'default'"),
        UDTFArg::Make<types::BOOLEAN>("use_if_not_exists", "Whether to use IF NOT EXISTS in CREATE TABLE statements", true));
  }

  Status Init(FunctionContext*, types::StringValue host, types::Int64Value port,
              types::StringValue username, types::StringValue password,
              types::StringValue database, types::BoolValue use_if_not_exists) {
    // Store ClickHouse connection parameters
    host_ = std::string(host);
    port_ = port.val;
    username_ = std::string(username);
    password_ = std::string(password);
    database_ = std::string(database);
    use_if_not_exists_ = use_if_not_exists.val;

    // Fetch schemas from MDS
    px::vizier::services::metadata::SchemaRequest req;
    px::vizier::services::metadata::SchemaResponse resp;

    grpc::ClientContext ctx;
    add_context_authentication_func_(&ctx);
    auto s = stub_->GetSchemas(&ctx, req, &resp);
    if (!s.ok()) {
      return error::Internal("Failed to make RPC call to metadata service: $0",
                             s.error_message());
    }

    // Connect to ClickHouse
    clickhouse::ClientOptions client_options;
    client_options.SetHost(host_);
    client_options.SetPort(port_);
    client_options.SetUser(username_);
    client_options.SetPassword(password_);
    client_options.SetDefaultDatabase(database_);

    try {
      clickhouse_client_ = std::make_unique<clickhouse::Client>(client_options);
      // Test connection
      clickhouse_client_->Execute("SELECT 1");
    } catch (const std::exception& e) {
      return error::Internal("Failed to connect to ClickHouse at $0:$1 - $2",
                             host_, port_, e.what());
    }

    for (const auto& [rel_table_name, rel] : resp.schema().relation_map()) {
      TableResult result;
      std::string table_name = rel_table_name;
      result.table_name = table_name;

      // Check if table has a time_ column (required for partitioning)
      bool has_time_column = false;
      for (const auto& col : rel.columns()) {
        if (col.column_name() == "time_" &&
            col.column_type() == types::DataType::TIME64NS) {
          has_time_column = true;
          break;
        }
      }

      if (!has_time_column) {
        result.status = "skipped";
        result.message = "Table does not have a time_ TIME64NS column, skipping";
        results_.push_back(result);
        continue;
      }

      std::vector<std::string> names = absl::StrSplit(table_name, '.');
      if (names.size() <= 0 || names.size() > 2) {
        result.status = "error";
        result.message = "Invalid table name with multiple dots";
        results_.push_back(result);
        continue;
      }
      table_name = names[0];

      // Generate CREATE TABLE statement
      std::string create_table_sql = GenerateCreateTableSQL(table_name, rel, use_if_not_exists_);

      // Execute the CREATE TABLE
      try {
        // Drop existing table if not using IF NOT EXISTS
        if (!use_if_not_exists_) {
          clickhouse_client_->Execute(absl::Substitute("DROP TABLE IF EXISTS $0", table_name));
        }

        // Create new table
        clickhouse_client_->Execute(create_table_sql);

        result.status = "success";
        result.message = "Table created successfully";
      } catch (const std::exception& e) {
        result.status = "error";
        result.message = absl::Substitute("Failed to create table: $0", e.what());
      }

      results_.push_back(result);
    }

    return Status::OK();
  }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    if (idx_ >= static_cast<int>(results_.size())) {
      return false;
    }

    const auto& result = results_[idx_];
    rw->Append<IndexOf("table_name")>(result.table_name);
    rw->Append<IndexOf("status")>(result.status);
    rw->Append<IndexOf("message")>(result.message);

    idx_++;
    return idx_ < static_cast<int>(results_.size());
  }

 private:
  struct TableResult {
    std::string table_name;
    std::string status;
    std::string message;
  };

  /**
   * Generates a CREATE TABLE SQL statement for ClickHouse based on Pixie table schema.
   * Follows the pattern from carnot_executable.cc:
   * - Maps Pixie types to ClickHouse types
   * - Adds hostname String column
   * - Adds event_time DateTime64(3) column
   * - Uses ENGINE = MergeTree()
   * - Uses PARTITION BY toYYYYMM(event_time)
   * - Uses ORDER BY (hostname, event_time)
   */
  std::string GenerateCreateTableSQL(const std::string& table_name,
                                      const px::table_store::schemapb::Relation& schema,
                                      bool use_if_not_exists) {
    std::vector<std::string> column_defs;

    // Add columns from schema
    for (const auto& col : schema.columns()) {
      std::string column_name = col.column_name();
      if (column_name == "event_time" || column_name == "hostname") {
        // event_time and hostname are added separately
        continue;
      }
      std::string clickhouse_type = clickhouse_schema::PixieTypeToClickHouseType(
          col.column_type(), column_name);
      column_defs.push_back(absl::Substitute("$0 $1", column_name, clickhouse_type));
    }

    // Add hostname column
    column_defs.push_back("hostname String");

    // Add event_time column for partitioning (will be populated from time_ column)
    column_defs.push_back("event_time DateTime64(3)");

    // Build the CREATE TABLE statement
    std::string columns_str = absl::StrJoin(column_defs, ",\n        ");

    std::string if_not_exists_clause = use_if_not_exists ? "IF NOT EXISTS " : "";
    std::string create_sql = absl::Substitute(R"(
      CREATE TABLE $0$1 (
        $2
      ) ENGINE = MergeTree()
      PARTITION BY toYYYYMM(event_time)
      ORDER BY (hostname, event_time)
    )", if_not_exists_clause, table_name, columns_str);

    return create_sql;
  }

  int idx_ = 0;
  std::vector<TableResult> results_;
  std::shared_ptr<MDSStub> stub_;
  std::function<void(grpc::ClientContext*)> add_context_authentication_func_;
  std::unique_ptr<clickhouse::Client> clickhouse_client_;

  // ClickHouse connection parameters
  std::string host_;
  int port_;
  std::string username_;
  std::string password_;
  std::string database_;
  bool use_if_not_exists_;
};

}  // namespace md
}  // namespace funcs
}  // namespace vizier
}  // namespace px
