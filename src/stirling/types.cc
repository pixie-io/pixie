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

std::unique_ptr<DynamicDataTableSchema> DynamicDataTableSchema::Create(
    std::string_view output_name, BackedDataElements elements) {
  return absl::WrapUnique(new DynamicDataTableSchema(output_name, std::move(elements)));
}

BackedDataElements::BackedDataElements(size_t size) : size_(size), pos_(0) {
  // Constructor forces an initial size. It is critical to size names_ and descriptions_
  // up-front, otherwise a reallocation will cause the string_views from elements_ into these
  // structures to become invalid.
  names_.resize(size_);
  descriptions_.resize(size_);
  elements_.reserve(size_);
}

void BackedDataElements::emplace_back(std::string name, std::string description,
                                      types::DataType type, types::SemanticType stype,
                                      types::PatternType ptype) {
  DCHECK(pos_ < size_);

  names_[pos_] = std::move(name);
  descriptions_[pos_] = std::move(description);
  elements_.emplace_back(names_[pos_], descriptions_[pos_], type, stype, ptype);
  ++pos_;
}

}  // namespace stirling
}  // namespace pl
