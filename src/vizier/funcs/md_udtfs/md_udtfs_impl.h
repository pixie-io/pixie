#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <absl/numeric/int128.h>
#include <grpcpp/grpcpp.h>
#include <magic_enum.hpp>

#include "src/carnot/udf/udf.h"
#include "src/common/base/base.h"
#include "src/common/uuid/uuid.h"

namespace pl {
namespace vizier {
namespace funcs {
namespace md {

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
        ColInfo("last_heartbeat_ns", types::DataType::DURATION64NS, types::PatternType::GENERAL,
                "Time since the last heartbeat"));
  }

  Status Init(FunctionContext*) {
    pl::vizier::services::metadata::AgentInfoRequest req;
    resp_ = std::make_unique<pl::vizier::services::metadata::AgentInfoResponse>();

    grpc::ClientContext ctx;
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

/**
 * This UDTF dumps the debug string for the current metadata state.
 */
class GetDebugMDState final : public carnot::udf::UDTF<GetDebugMDState> {
 public:
  // TODO(zasgar/philkuz): Switch this to all agents after we finish support for this in the
  // compiler
  static constexpr auto Executor() { return carnot::udfspb::UDTFSourceExecutor::UDTF_ALL_PEM; }

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

}  // namespace md
}  // namespace funcs
}  // namespace vizier
}  // namespace pl
