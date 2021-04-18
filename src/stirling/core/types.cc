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

#include "src/stirling/core/types.h"

namespace px {
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
  table_schema_proto.set_desc(std::string(desc_));
  table_schema_proto.set_tabletized(tabletized_);
  table_schema_proto.set_tabletization_key(tabletization_key_);

  return table_schema_proto;
}

std::unique_ptr<DynamicDataTableSchema> DynamicDataTableSchema::Create(
    std::string_view name, std::string_view desc, BackedDataElements elements) {
  return absl::WrapUnique(new DynamicDataTableSchema(name, desc, std::move(elements)));
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
}  // namespace px
