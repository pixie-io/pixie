#include "src/stirling/types.h"

#include <google/protobuf/repeated_field.h>
#include "src/stirling/dynamic_tracing/ir/physicalpb/physical.pb.h"

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

namespace {

BackedDataElements CreateDataElements(
    const google::protobuf::RepeatedPtrField<::pl::stirling::dynamic_tracing::ir::physical::Field>&
        repeated_fields) {
  using dynamic_tracing::ir::shared::ScalarType;

  // clang-format off
  static const std::map<ScalarType, types::DataType> kTypeMap = {
          {ScalarType::BOOL, types::DataType::BOOLEAN},

          {ScalarType::SHORT, types::DataType::INT64},
          {ScalarType::USHORT, types::DataType::INT64},
          {ScalarType::INT, types::DataType::INT64},
          {ScalarType::UINT, types::DataType::INT64},
          {ScalarType::LONG, types::DataType::INT64},
          {ScalarType::ULONG, types::DataType::INT64},
          {ScalarType::LONGLONG, types::DataType::INT64},
          {ScalarType::ULONGLONG, types::DataType::INT64},

          {ScalarType::INT8, types::DataType::INT64},
          {ScalarType::INT16, types::DataType::INT64},
          {ScalarType::INT32, types::DataType::INT64},
          {ScalarType::INT64, types::DataType::INT64},
          {ScalarType::UINT8, types::DataType::INT64},
          {ScalarType::UINT16, types::DataType::INT64},
          {ScalarType::UINT32, types::DataType::INT64},
          {ScalarType::UINT64, types::DataType::INT64},

          {ScalarType::FLOAT, types::DataType::FLOAT64},
          {ScalarType::DOUBLE, types::DataType::FLOAT64},

          {ScalarType::STRING, types::DataType::STRING},

          // Will be converted to a hex string.
          {ScalarType::BYTE_ARRAY, types::DataType::STRING},

          // Will be converted to JSON string.
          {ScalarType::STRUCT_BLOB, types::DataType::STRING},
  };
  // clang-format on

  BackedDataElements elements(repeated_fields.size());

  // Insert the special upid column.
  // TODO(yzhao): Make sure to have a structured way to let the IR to express the upid.
  elements.emplace_back("upid", "", types::DataType::UINT128);

  for (int i = 0; i < repeated_fields.size(); ++i) {
    const auto& field = repeated_fields[i];

    if (field.name() == "tgid_" || field.name() == "tgid_start_time_") {
      // We already automatically added the upid column.
      // These will get merged into the UPID, so skip.
      continue;
    }

    types::DataType data_type;

    auto iter = kTypeMap.find(field.type());
    if (iter == kTypeMap.end()) {
      LOG(DFATAL) << absl::Substitute("Unrecognized base type: $0", field.type());
      data_type = types::DataType::DATA_TYPE_UNKNOWN;
    } else {
      data_type = iter->second;
    }

    if (field.name() == "time_") {
      data_type = types::DataType::TIME64NS;
    }

    // TODO(oazizi): See if we need to find a way to define SemanticTypes and PatternTypes.
    elements.emplace_back(field.name(), "", data_type);
  }

  return elements;
}

BackedDataElements CreateDataElements(const std::vector<types::DataType>& columns) {
  BackedDataElements elements(columns.size());

  for (size_t i = 0; i < columns.size(); ++i) {
    elements.emplace_back(absl::StrCat("Column ", i), "", columns[i]);
  }

  return elements;
}

}  // namespace

std::unique_ptr<DynamicDataTableSchema> DynamicDataTableSchema::Create(
    const dynamic_tracing::BCCProgram::PerfBufferSpec& output_spec) {
  return absl::WrapUnique(new DynamicDataTableSchema(
      output_spec.name, CreateDataElements(output_spec.output.fields())));
}

std::unique_ptr<DynamicDataTableSchema> DynamicDataTableSchema::Create(
    std::string_view output_name, const std::vector<types::DataType>& columns) {
  return absl::WrapUnique(new DynamicDataTableSchema(output_name, CreateDataElements(columns)));
}

}  // namespace stirling
}  // namespace pl
