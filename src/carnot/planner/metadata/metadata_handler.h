#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/carnot/planner/ir/pattern_match.h"
#include "src/common/base/error.h"
#include "src/common/base/status.h"
#include "src/common/base/statusor.h"
#include "src/shared/metadatapb/metadata.pb.h"
#include "src/shared/types/proto/wrapper/types_pb_wrapper.h"

namespace pl {
namespace carnot {
namespace planner {

using ::pl::shared::metadatapb::MetadataType;

class NameMetadataProperty : public MetadataProperty {
 public:
  explicit NameMetadataProperty(MetadataType metadata_type, std::vector<MetadataType> key_columns)
      : MetadataProperty(metadata_type, types::DataType::STRING, key_columns) {}
  // TODO(nserrino): Remove this requirement and store entity names without namespace prepended.
  // Expect format to be "<namespace>/<value>"
  bool ExprFitsFormat(ExpressionIR* ir_node) const override {
    DCHECK(ir_node->type() == IRNodeType::kString);
    std::string value = static_cast<StringIR*>(ir_node)->str();
    std::vector<std::string> split_str = absl::StrSplit(value, "/");
    return split_str.size() == 2;
  }
  std::string ExplainFormat() const override { return "String with format <namespace>/<name>."; }
};

class IdMetadataProperty : public MetadataProperty {
 public:
  explicit IdMetadataProperty(MetadataType metadata_type, std::vector<MetadataType> key_columns)
      : MetadataProperty(metadata_type, types::DataType::STRING, key_columns) {}
  // TODO(philkuz) udate this fits format when we have a better idea what the format should be.
  // ExprFitsFormat always evaluates to true because id format is not yet defined.
  bool ExprFitsFormat(ExpressionIR*) const override { return true; }
  std::string ExplainFormat() const override { return ""; }
};

class Int64MetadataProperty : public MetadataProperty {
 public:
  explicit Int64MetadataProperty(MetadataType metadata_type, std::vector<MetadataType> key_columns)
      : MetadataProperty(metadata_type, types::DataType::INT64, key_columns) {}
  bool ExprFitsFormat(ExpressionIR* expr) const override { return Match(expr, Int()); }
  std::string ExplainFormat() const override { return ""; }
};

class MetadataHandler {
 public:
  StatusOr<MetadataProperty*> GetProperty(const std::string& md_name) const;
  bool HasProperty(const std::string& md_name) const;
  static std::unique_ptr<MetadataHandler> Create();

 private:
  MetadataHandler() {}
  MetadataProperty* AddProperty(std::unique_ptr<MetadataProperty> md_property);
  void AddMapping(const std::string& name, MetadataProperty* property);
  template <typename Property>
  void AddObject(MetadataType md_type, const std::vector<std::string>& aliases,
                 const std::vector<MetadataType>& key_columns);

  std::vector<std::unique_ptr<MetadataProperty>> property_pool;
  std::unordered_map<std::string, MetadataProperty*> metadata_map;
};

}  // namespace planner
}  // namespace carnot
}  // namespace pl
