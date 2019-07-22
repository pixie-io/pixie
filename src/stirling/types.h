#pragma once

#include <arrow/type.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/type_utils.h"

namespace pl {
namespace stirling {

class InfoClassManager;

using ArrowArrayBuilderUPtrVec = std::vector<std::unique_ptr<arrow::ArrayBuilder>>;
using ArrowRecordBatchSPtrVec = std::vector<std::shared_ptr<arrow::RecordBatch>>;

using PushDataCallback =
    std::function<void(uint64_t, std::unique_ptr<types::ColumnWrapperRecordBatch>)>;
using InfoClassManagerVec = std::vector<std::unique_ptr<InfoClassManager>>;

class DataElement {
 public:
  constexpr DataElement() = delete;
  constexpr DataElement(std::string_view name, types::DataType type, types::PatternType ptype)
      : name_(name), type_(type), ptype_(ptype) {}

  constexpr const std::string_view& name() const { return name_; }
  constexpr const types::DataType& type() const { return type_; }
  std::shared_ptr<arrow::DataType> arrow_type() { return types::DataTypeToArrowType(type()); }

 protected:
  const std::string_view name_;
  types::DataType type_;
  types::PatternType ptype_;
};

class DataTableSchema {
 public:
  // TODO(oazizi): This constructor should only be called at compile-time. Need to enforce this.
  template <std::size_t N>
  constexpr DataTableSchema(std::string_view name, const DataElement (&elements)[N])
      : name_(name), elements_(elements) {}
  constexpr std::string_view name() const { return name_; }
  constexpr ConstVectorView<DataElement> elements() const { return elements_; }

  // Warning: use at compile-time only!
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
    // CHECK(i != elements_.size()) << "Could not find key";

    return i;
  }

 private:
  const std::string_view name_;
  const ConstVectorView<DataElement> elements_;
};

// Initializes record_batch so that it has data fields that matches data_elements' spec.
// Template to cover both ConstVectorView<DataElement> and std::vector<InfoClassElement>.
// TODO(oazizi): No point in returning a Status for this function.
template <class T>
void InitRecordBatch(const T& data_elements, uint32_t target_capacity,
                     types::ColumnWrapperRecordBatch* record_batch) {
  for (const auto& element : data_elements) {
    pl::types::DataType type = element.type();

#define TYPE_CASE(_dt_)                           \
  auto col = types::ColumnWrapper::Make(_dt_, 0); \
  col->Reserve(target_capacity);                  \
  record_batch->push_back(col);
    PL_SWITCH_FOREACH_DATATYPE(type, TYPE_CASE);
#undef TYPE_CASE
  }
}

}  // namespace stirling
}  // namespace pl
