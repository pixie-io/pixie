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

#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/shared/metadata/metadata_state.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/type_utils.h"
#include "src/stirling/proto/stirling.pb.h"

namespace px {
namespace stirling {

using DataPushCallback = std::function<Status(uint32_t, types::TabletID,
                                              std::unique_ptr<types::ColumnWrapperRecordBatch>)>;

using AgentMetadataType = std::shared_ptr<const px::md::AgentMetadataState>;

/**
 * The callback function signature to fetch new metadata.
 */
using AgentMetadataCallback = std::function<AgentMetadataType()>;

class DataElement {
 public:
  constexpr DataElement() = delete;
  constexpr DataElement(std::string_view name, std::string_view desc, types::DataType type,
                        types::SemanticType stype, types::PatternType ptype,
                        const std::map<int64_t, std::string_view>* decoder = {})
      : name_(name), desc_(desc), type_(type), stype_(stype), ptype_(ptype), decoder_(decoder) {
    // Note: Ideally, we'd call CheckSchema() here because GCC chokes.
    // This is because we use initializers, in our table definitions (e.g. http_table.h),
    // for which GCC wants to call the default constructor before setting the values.
    // Clang does this differently, so has no problem with it.
    //
    // Workaround is to make CheckSchema() public, and let DataTableSchema call it as part of
    // its enforcement. With the workaround, standalone DataElements are not enforced,
    // but we never effectively have standalone DataElements, so we're protected.
  }

  constexpr std::string_view name() const { return name_; }
  constexpr types::DataType type() const { return type_; }
  constexpr types::PatternType ptype() const { return ptype_; }
  constexpr types::SemanticType stype() const { return stype_; }
  constexpr std::string_view desc() const { return desc_; }
  constexpr const std::map<int64_t, std::string_view>* decoder() const { return decoder_; }
  stirlingpb::Element ToProto() const;

  constexpr void CheckSchema() const {
    COMPILE_TIME_ASSERT(!name_.empty(), "Element name may not be empty.");
    COMPILE_TIME_ASSERT(type_ != types::DataType::DATA_TYPE_UNKNOWN,
                        "Element may not have unknown data type.");
    COMPILE_TIME_ASSERT(ptype_ != types::PatternType::UNSPECIFIED,
                        "Element may not have unspecified pattern type.");
    if (decoder_ != nullptr) {
      COMPILE_TIME_ASSERT(type_ == types::DataType::INT64,
                          "Enum decoders are only valid for columns with type INT64");
      COMPILE_TIME_ASSERT(ptype_ == types::PatternType::GENERAL_ENUM,
                          "Enum decoders are only valid for columns with pattern type ENUM");
    }
  }

 protected:
  const std::string_view name_;
  const std::string_view desc_;
  const types::DataType type_ = types::DataType::DATA_TYPE_UNKNOWN;
  const types::SemanticType stype_ = types::SemanticType::ST_NONE;
  const types::PatternType ptype_ = types::PatternType::UNSPECIFIED;
  const std::map<int64_t, std::string_view>* decoder_ = nullptr;
};

class DataTableSchema {
 public:
  // TODO(oazizi): This constructor should only be called at compile-time. Need to enforce this.
  template <std::size_t N>
  constexpr DataTableSchema(std::string_view name, std::string_view desc,
                            const DataElement (&elements)[N])
      : name_(name), desc_(desc), elements_(elements), tabletized_(false) {
    CheckSchema();
  }

  template <std::size_t N>
  constexpr DataTableSchema(std::string_view name, std::string_view desc,
                            const DataElement (&elements)[N],
                            std::string_view tabletization_key_name)
      : name_(name),
        desc_(desc),
        elements_(elements),
        tabletized_(true),
        tabletization_key_(ColIndex(tabletization_key_name)) {
    CheckSchema();
  }

  DataTableSchema(std::string_view name, std::string_view desc,
                  const std::vector<DataElement>& elements)
      : name_(name), desc_(desc), elements_(elements.data(), elements.size()), tabletized_(false) {
    CheckSchema();
  }

  constexpr std::string_view name() const { return name_; }
  constexpr std::string_view desc() const { return desc_; }
  constexpr bool tabletized() const { return tabletized_; }
  constexpr size_t tabletization_key() const { return tabletization_key_; }
  constexpr ArrayView<DataElement> elements() const { return elements_; }

  // Warning: use at compile-time only!
  // TODO(oazizi): Convert to consteval when C++20 is supported, to ensure compile-time use only.
  constexpr uint32_t ColIndex(std::string_view key) const {
    uint32_t i = 0;
    for (i = 0; i < elements_.size(); i++) {
      if (elements_[i].name() == key) {
        break;
      }
    }

    // Check that we found the index.
    // This prevents compilation during constexpr evaluation (which is awesome!).
    COMPILE_TIME_ASSERT(i != elements_.size(), "Could not find key");

    return i;
  }

  constexpr std::string_view ColName(size_t i) const {
    DCHECK(i < elements_.size());
    return elements_[i].name();
  }

  stirlingpb::TableSchema ToProto() const;

 private:
  constexpr void CheckSchema() {
    COMPILE_TIME_ASSERT(!name_.empty(), "Table name may not be empty.");

    if (tabletized_) {
      COMPILE_TIME_ASSERT(tabletization_key_ != std::numeric_limits<size_t>::max(),
                          "Tabletization key name must be initialized.");
      //      // TODO(oazizi): Add support for other types of tabletization keys.
      //      COMPILE_TIME_ASSERT(tabletization_element_.type() == types::DataType::INT64,
      //                          "Tabletization key must currently be of type INT64.");
      //      COMPILE_TIME_ASSERT(tabletization_element_.ptype() != types::PatternType::UNSPECIFIED,
      //                          "Tabletization key must have a pattern type");
    }

    for (size_t i = 0; i < elements_.size(); ++i) {
      elements_[i].CheckSchema();
    }
  }

  const std::string_view name_;
  const std::string_view desc_;
  const ArrayView<DataElement> elements_;
  const bool tabletized_ = false;
  size_t tabletization_key_ = std::numeric_limits<size_t>::max();

  static constexpr std::chrono::milliseconds kDefaultPushPeriod{1000};
};

class BackedDataElements {
 public:
  // Default constructor is for compatibility with StatusOr only.
  // Do not use. A DCHECK will fire if emplace_back is called.
  BackedDataElements() : size_(0) {}

  explicit BackedDataElements(size_t size);

  // Disallow copies: the pointers in elements_ would be invalid.
  BackedDataElements(const BackedDataElements& other) = delete;
  BackedDataElements& operator=(const BackedDataElements& other) = delete;

  // Move operators are safe. These are used by DynamicDataTableSchema and StatusOr.
  BackedDataElements(BackedDataElements&& other) = default;
  BackedDataElements& operator=(BackedDataElements&& other) = default;

  void emplace_back(std::string name, std::string description, types::DataType type,
                    types::SemanticType stype = types::SemanticType::ST_NONE,
                    types::PatternType ptype = types::PatternType::UNSPECIFIED);

  const std::vector<DataElement>& elements() const { return elements_; }

 private:
  std::vector<DataElement> elements_;

  // Backing store for the string views in elements.
  std::vector<std::string> names_;
  std::vector<std::string> descriptions_;

  size_t size_ = 0;
  size_t pos_ = 0;
};

/**
 * A wrapper around DataTableSchema that also holds storage for the elements.
 * DataTableSchema itself does not hold elements, but rather has views into the elements.
 * This normally works for static tables because the elements are constexpr.
 * In the case of dynamic tables, however, we need to create and store the elements.
 */
class DynamicDataTableSchema {
 public:
  static std::unique_ptr<DynamicDataTableSchema> Create(std::string_view name,
                                                        std::string_view desc,
                                                        BackedDataElements elements);

  const DataTableSchema& Get() { return table_schema_; }

 private:
  DynamicDataTableSchema(std::string_view name, std::string_view desc, BackedDataElements elements)
      : name_(name),
        desc_(desc),
        elements_(std::move(elements)),
        table_schema_(name_, desc_, elements_.elements()) {}

  std::string name_;
  std::string desc_;

  // Keep the copy of the elements, because table_schema_ has views into this data,
  // particularly the name and description (i.e. strings).
  BackedDataElements elements_;

  // The main data structure, which mostly has views into elements_, and by extension
  // output_struct_.
  DataTableSchema table_schema_;
};

}  // namespace stirling
}  // namespace px
