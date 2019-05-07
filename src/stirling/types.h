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
  DataElement() = delete;
  virtual ~DataElement() = default;
  explicit DataElement(std::string name, const types::DataType& type)
      : name_(std::move(name)), type_(type) {}

  const std::string& name() const { return name_; }
  const types::DataType& type() const { return type_; }
  size_t WidthBytes() const { return pl::types::DataTypeWidthBytes(type_); }
  std::shared_ptr<arrow::DataType> arrow_type() { return types::DataTypeToArrowType(type()); }

 protected:
  std::string name_;
  types::DataType type_;
};

using DataElements = std::vector<DataElement>;

class DataTableSchema {
 public:
  DataTableSchema(std::string_view name, const DataElements& elements)
      : name_(name), elements_(elements) {}
  const std::string& name() const { return name_; }
  const DataElements& elements() const { return elements_; }

 private:
  std::string name_;
  DataElements elements_;
};

// Initializes record_batch so that it has data fields that matches data_elements' spec.
Status InitRecordBatch(const std::vector<DataElement>& data_elements, int target_capacity,
                       types::ColumnWrapperRecordBatch* record_batch);

}  // namespace stirling
}  // namespace pl
