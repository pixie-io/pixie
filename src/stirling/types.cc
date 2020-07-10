#include "src/stirling/types.h"

namespace pl {
namespace stirling {

stirlingpb::Element DataElement::ToProto() const {
  stirlingpb::Element element_proto;
  element_proto.set_name(std::string(name_));
  element_proto.set_type(type_);
  element_proto.set_ptype(ptype_);
  element_proto.set_stype(stype_);
  element_proto.set_desc(std::string(desc_));
  if (decoder_ != nullptr) {
    google::protobuf::Map<int64_t, std::string>* decoder = element_proto.mutable_decoder();
    *decoder = google::protobuf::Map<int64_t, std::string>(decoder_->begin(), decoder_->end());
  }
  return element_proto;
}

stirlingpb::TableSchema DataTableSchema::ToProto() const {
  stirlingpb::TableSchema table_schema_proto;

  // Populate the proto with Elements.
  for (auto element : elements_) {
    stirlingpb::Element* element_proto_ptr = table_schema_proto.add_elements();
    element_proto_ptr->MergeFrom(element.ToProto());
  }
  table_schema_proto.set_name(std::string(name_));
  table_schema_proto.set_tabletized(tabletized_);
  table_schema_proto.set_tabletization_key(tabletization_key_);

  return table_schema_proto;
}

StatusOr<std::unique_ptr<DynamicDataTableSchema>> DynamicDataTableSchema::Create(
    dynamic_tracing::ir::physical::Struct output_struct) {
  using dynamic_tracing::ir::shared::ScalarType;

  auto output_struct_ptr =
      std::make_unique<dynamic_tracing::ir::physical::Struct>(std::move(output_struct));

  // clang-format off
  static const std::map<ScalarType, types::DataType> kTypeMap = {
          {ScalarType::BOOL, types::DataType::BOOLEAN},
          {ScalarType::INT, types::DataType::INT64},
          {ScalarType::INT8, types::DataType::INT64},
          {ScalarType::INT16, types::DataType::INT64},
          {ScalarType::INT32, types::DataType::INT64},
          {ScalarType::INT64, types::DataType::INT64},
          {ScalarType::UINT, types::DataType::INT64},
          {ScalarType::UINT8, types::DataType::INT64},
          {ScalarType::UINT16, types::DataType::INT64},
          {ScalarType::UINT32, types::DataType::INT64},
          {ScalarType::UINT64, types::DataType::INT64},
          {ScalarType::FLOAT, types::DataType::FLOAT64},
          {ScalarType::DOUBLE, types::DataType::FLOAT64},
  };
  // clang-format on

  std::vector<DataElement> elements;
  for (const auto& field : output_struct_ptr->fields()) {
    types::DataType data_type;

    auto iter = kTypeMap.find(field.type().scalar());
    if (iter == kTypeMap.end()) {
      LOG(DFATAL) << absl::Substitute("Unrecognized base type: $0", field.type().scalar());
      data_type = types::DataType::DATA_TYPE_UNKNOWN;
    } else {
      data_type = iter->second;
    }

    // TODO(oazizi): See if we need to find a way to define SemanticTypes and PatternTypes.
    elements.emplace_back(field.name(), field.name(), data_type, types::SemanticType::ST_NONE,
                          types::PatternType::UNSPECIFIED);
  }

  std::string_view name = output_struct_ptr->name();
  return std::unique_ptr<DynamicDataTableSchema>(
      new DynamicDataTableSchema(std::move(output_struct_ptr), name, elements));
}

}  // namespace stirling
}  // namespace pl
