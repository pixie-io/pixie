#pragma once

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <absl/numeric/int128.h>
#include <grpcpp/grpcpp.h>
#include <magic_enum.hpp>

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"
#include "src/common/base/base.h"
#include "src/common/uuid/uuid.h"
#include "src/vizier/services/agent/manager/manager.h"

namespace pl {
namespace vizier {
namespace funcs {
namespace md {

std::string GenerateServiceToken();

// TODO(zasgar/michelle): We need to figure out a way to restrict access based on user.
inline void ContextWithServiceAuth(grpc::ClientContext* context) {
  std::string token = GenerateServiceToken();
  context->AddMetadata("authorization", absl::Substitute("bearer $0", token));
}

template <typename TUDTF>
class UDTFWithMDFactory : public carnot::udf::UDTFFactory {
 public:
  UDTFWithMDFactory() = delete;
  explicit UDTFWithMDFactory(const VizierFuncFactoryContext& ctx) : ctx_(ctx) {}

  std::unique_ptr<carnot::udf::AnyUDTF> Make() override {
    return std::make_unique<TUDTF>(ctx_.mds_stub());
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
  explicit UDTFWithTableStoreFactory(const ::pl::table_store::TableStore* table_store)
      : table_store_(table_store) {}

  std::unique_ptr<carnot::udf::AnyUDTF> Make() override {
    return std::make_unique<TUDTF>(table_store_);
  }

 private:
  const ::pl::table_store::TableStore* table_store_;
};
/**
 * This UDTF fetches all the schemas that are available to query from the MDS.
 */
class GetTableSchemas final : public carnot::udf::UDTF<GetTableSchemas> {
 public:
  using MDSStub = vizier::services::metadata::MetadataService::Stub;
  using SchemaResponse = vizier::services::metadata::SchemaResponse;
  GetTableSchemas() = delete;
  explicit GetTableSchemas(std::shared_ptr<MDSStub> stub) : idx_(0), stub_(stub) {}

  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ONE_KELVIN; }

  static constexpr auto OutputRelation() {
    return MakeArray(ColInfo("table_name", types::DataType::STRING, types::PatternType::GENERAL,
                             "The table name"),
                     ColInfo("column_name", types::DataType::STRING, types::PatternType::GENERAL,
                             "The name of the column"),
                     ColInfo("column_type", types::DataType::STRING, types::PatternType::GENERAL,
                             "The type of the column"),
                     ColInfo("column_desc", types::DataType::STRING, types::PatternType::GENERAL,
                             "Description of the column"));
  }

  Status Init(FunctionContext*) {
    pl::vizier::services::metadata::SchemaRequest req;
    pl::vizier::services::metadata::SchemaResponse resp;

    grpc::ClientContext ctx;
    ContextWithServiceAuth(&ctx);
    auto s = stub_->GetSchemas(&ctx, req, &resp);
    if (!s.ok()) {
      return error::Internal("Failed to make RPC call to metadata service");
    }

    // TODO(zasgar): We store the data since it's hard to traverse two maps at once. We should
    // either do that or perhaps have an interface that allows UDTFs to write multiple records in
    // a single invocation.
    for (const auto& [table_name, rel] : resp.schema().relation_map()) {
      for (const auto& col : rel.columns()) {
        relation_info_.emplace_back(table_name, col.column_name(),
                                    std::string(magic_enum::enum_name(col.column_type())),
                                    col.column_desc());
      }
    }
    return Status::OK();
  }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    const auto& r = relation_info_[idx_];
    rw->Append<IndexOf("table_name")>(r.table_name);
    rw->Append<IndexOf("column_name")>(r.column_name);
    rw->Append<IndexOf("column_type")>(r.column_type);
    rw->Append<IndexOf("column_desc")>(r.column_desc);

    idx_++;
    return idx_ < static_cast<int>(relation_info_.size());
  }

 private:
  struct RelationInfo {
    RelationInfo(const std::string& table_name, const std::string& column_name,
                 const std::string& column_type, const std::string& column_desc)
        : table_name(table_name),
          column_name(column_name),
          column_type(column_type),
          column_desc(column_desc) {}
    std::string table_name;
    std::string column_name;
    std::string column_type;
    std::string column_desc;
  };

  int idx_ = 0;
  std::vector<RelationInfo> relation_info_;
  std::shared_ptr<MDSStub> stub_;
};

/**
 * This UDTF fetches all the schemas that are available to query from the MDS.
 */
class GetAgentStatus final : public carnot::udf::UDTF<GetAgentStatus> {
 public:
  using MDSStub = vizier::services::metadata::MetadataService::Stub;
  using SchemaResponse = vizier::services::metadata::SchemaResponse;
  GetAgentStatus() = delete;
  explicit GetAgentStatus(std::shared_ptr<MDSStub> stub) : idx_(0), stub_(stub) {}

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
                "Time (in nanoseconds) since the last heartbeat"));
  }

  Status Init(FunctionContext*) {
    pl::vizier::services::metadata::AgentInfoRequest req;
    resp_ = std::make_unique<pl::vizier::services::metadata::AgentInfoResponse>();

    grpc::ClientContext ctx;
    ContextWithServiceAuth(&ctx);
    auto s = stub_->GetAgentInfo(&ctx, req, resp_.get());
    if (!s.ok()) {
      return error::Internal("Failed to make RPC call to GetAgentInfo");
    }
    return Status::OK();
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

    rw->Append<IndexOf("agent_id")>(absl::MakeUint128(u.ab, u.cd));
    rw->Append<IndexOf("asid")>(agent_info.asid());
    rw->Append<IndexOf("hostname")>(agent_info.info().host_info().hostname());
    rw->Append<IndexOf("ip_address")>(agent_info.info().ip_address());
    rw->Append<IndexOf("agent_state")>(StringValue(magic_enum::enum_name(agent_status.state())));
    rw->Append<IndexOf("create_time")>(agent_info.create_time_ns());
    rw->Append<IndexOf("last_heartbeat_ns")>(agent_status.ns_since_last_heartbeat());

    ++idx_;
    return idx_ < resp_->info_size();
  }

 private:
  int idx_ = 0;
  std::unique_ptr<pl::vizier::services::metadata::AgentInfoResponse> resp_;
  std::shared_ptr<MDSStub> stub_;
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
                     ColInfo("debug_state", types::DataType::STRING, types::PatternType::GENERAL,
                             "The debug state of metadata on the agent"));
  }

  bool NextRecord(FunctionContext* ctx, RecordWriter* rw) {
    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("debug_state")>(ctx->metadata_state()->DebugString());
    // no more records.
    return false;
  }
};

/**
 * This UDTF dumps the debug information for all registered tables.
 */
class GetDebugTableInfo final : public carnot::udf::UDTF<GetDebugTableInfo> {
 public:
  GetDebugTableInfo() = delete;
  explicit GetDebugTableInfo(const ::pl::table_store::TableStore* table_store)
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
                "The number of batches added to this table"),
        ColInfo("batches_expired", types::DataType::INT64, types::PatternType::GENERAL,
                "The number of batches expired from this table"),
        ColInfo("num_batches", types::DataType::INT64, types::PatternType::GENERAL,
                "The number of batches active in this table"),
        ColInfo("size", types::DataType::INT64, types::PatternType::GENERAL,
                "The size of this table in bytes"),
        ColInfo("max_table_size", types::DataType::INT64, types::PatternType::GENERAL,
                "The maximum size of this table"));
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
    auto info = table->GetTableInfo();

    rw->Append<IndexOf("asid")>(ctx->metadata_state()->asid());
    rw->Append<IndexOf("name")>(table_store_->GetTableName(selected_id));
    rw->Append<IndexOf("id")>(selected_id);
    rw->Append<IndexOf("batches_added")>(info.batches_added);
    rw->Append<IndexOf("batches_expired")>(info.batches_expired);
    rw->Append<IndexOf("num_batches")>(info.num_batches);
    rw->Append<IndexOf("size")>(info.bytes);
    rw->Append<IndexOf("max_table_size")>(info.max_table_size);

    ++current_idx_;
    return static_cast<size_t>(current_idx_) < table_ids_.size();
  }

 private:
  const ::pl::table_store::TableStore* table_store_;
  int current_idx_ = 0;
  std::vector<uint64_t> table_ids_;
};
}  // namespace md
}  // namespace funcs
}  // namespace vizier
}  // namespace pl
