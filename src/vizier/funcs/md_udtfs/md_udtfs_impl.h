#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <grpcpp/grpcpp.h>
#include <magic_enum.hpp>

#include "src/carnot/udf/udf.h"
#include "src/common/base/base.h"

namespace pl {
namespace vizier {
namespace funcs {
namespace md {

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

  Status Init(FunctionContext*) { return Status::OK(); }

  bool NextRecord(FunctionContext*, RecordWriter* rw) {
    if (idx_ == 0) {
      pl::vizier::services::metadata::SchemaRequest req;
      pl::vizier::services::metadata::SchemaResponse resp;

      grpc::ClientContext ctx;
      auto s = stub_->GetSchemas(&ctx, req, &resp);
      if (!s.ok()) {
        LOG(ERROR) << "Failed to make RPC call";
        return false;
      }

      // TODO(zasgar): We store the data since it's hard to traverse two maps at once. We should
      // either do that or perhaps have an interface that allows UDTFs to write multiple records in
      // a single invocation.
      for (const auto& [table_name, rel] : resp.schema().relation_map()) {
        for (const auto& col : rel.columns()) {
          relation_info_.emplace_back(table_name, col.column_name(),
                                      std::string(magic_enum::enum_name(col.column_type())),
                                      "description goes here");
        }
      }
    }

    const auto& r = relation_info_[idx_];
    rw->Append<IndexOf("table_name")>(r.table_name);
    rw->Append<IndexOf("column_name")>(r.column_name);
    rw->Append<IndexOf("column_type")>(r.column_type);
    rw->Append<IndexOf("column_desc")>(r.column_desc);

    idx_++;
    return idx_ < static_cast<int>(relation_info_.size());
  }

  class Factory : public carnot::udf::UDTFFactory {
   public:
    Factory() = delete;
    explicit Factory(const VizierFuncFactoryContext& ctx) : ctx_(ctx) {}

    std::unique_ptr<AnyUDTF> Make() override {
      return std::make_unique<GetTableSchemas>(ctx_.mds_stub());
    }

   private:
    const VizierFuncFactoryContext& ctx_;
  };

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

}  // namespace md
}  // namespace funcs
}  // namespace vizier
}  // namespace pl
