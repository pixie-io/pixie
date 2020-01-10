#pragma once

#include <arrow/type.h>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/shared/metadata/metadata_state.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/type_utils.h"
#include "src/stirling/proto/stirling.pb.h"

namespace pl {
namespace stirling {

using ArrowArrayBuilderUPtrVec = std::vector<std::unique_ptr<arrow::ArrayBuilder>>;
using ArrowRecordBatchSPtrVec = std::vector<std::shared_ptr<arrow::RecordBatch>>;

using PushDataCallback = std::function<void(uint32_t, types::TabletID,
                                            std::unique_ptr<types::ColumnWrapperRecordBatch>)>;

using AgentMetadataType = std::shared_ptr<const pl::md::AgentMetadataState>;
/**
 * The callback function signature to fetch new metadata.
 */
using AgentMetadataCallback = std::function<AgentMetadataType()>;

// TODO(oazizi/yzhao): Consider change this to a struct.
class DataElement {
 public:
  constexpr DataElement() = delete;
  constexpr DataElement(std::string_view name, types::DataType type, types::PatternType ptype,
                        std::string_view desc = {})
      : name_(name), type_(type), ptype_(ptype), desc_(desc) {}

  constexpr const std::string_view& name() const { return name_; }
  constexpr const types::DataType& type() const { return type_; }
  constexpr const types::PatternType& ptype() const { return ptype_; }
  constexpr const std::string_view& desc() const { return desc_; }
  stirlingpb::Element ToProto() const;

 protected:
  const std::string_view name_;
  types::DataType type_;
  types::PatternType ptype_;
  const std::string_view desc_;
};

class DataTableSchema {
 public:
  // TODO(oazizi): This constructor should only be called at compile-time. Need to enforce this.
  template <std::size_t N>
  constexpr DataTableSchema(std::string_view name, const DataElement (&elements)[N])
      : name_(name), elements_(elements), tabletized_(false) {
    CheckSchema();
  }

  template <std::size_t N>
  constexpr DataTableSchema(std::string_view name, const DataElement (&elements)[N],
                            std::string_view tabletization_key_name)
      : name_(name),
        elements_(elements),
        tabletized_(true),
        tabletization_key_(ColIndex(tabletization_key_name)) {
    CheckSchema();
  }

  constexpr std::string_view name() const { return name_; }
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

    // Alternative form, but this one checks at run-time as well as compile-time.
    // TODO(oazizi): Remove this line once function is converted to consteval (C++20).
    CHECK(i != elements_.size()) << "Could not find key";

    return i;
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
      COMPILE_TIME_ASSERT(elements_[i].name() != "", "Element name may not be empty.");
      COMPILE_TIME_ASSERT(elements_[i].type() != types::DataType::DATA_TYPE_UNKNOWN,
                          "Element may not have unknown data type.");
      COMPILE_TIME_ASSERT(elements_[i].ptype() != types::PatternType::UNSPECIFIED,
                          "Element may not have unspecified pattern type.");
    }
  }

  const std::string_view name_;
  const ArrayView<DataElement> elements_;
  const bool tabletized_ = false;
  size_t tabletization_key_ = std::numeric_limits<size_t>::max();
};

}  // namespace stirling
}  // namespace pl
