#pragma once

#include <arrow/type.h>
#include <memory>
#include <string>
#include <vector>

#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/type_utils.h"

namespace pl {
namespace stirling {

class InfoClassManager;

using ArrowArrayBuilderUPtrVec = std::vector<std::unique_ptr<arrow::ArrayBuilder>>;
using ArrowRecordBatchSPtrVec = std::vector<std::shared_ptr<arrow::RecordBatch>>;

using ColumnWrapperRecordBatch = std::vector<types::SharedColumnWrapper>;
using ColumnWrapperRecordBatchVec = std::vector<std::unique_ptr<ColumnWrapperRecordBatch>>;

using PushDataCallback = std::function<void(uint64_t, std::unique_ptr<ColumnWrapperRecordBatch>)>;
using InfoClassManagerVec = std::vector<std::unique_ptr<InfoClassManager>>;

class DataElement {
 public:
  DataElement() = delete;
  virtual ~DataElement() = default;
  explicit DataElement(const std::string& name, const types::DataType& type)
      : name_(name), type_(type) {}

  const std::string& name() const { return name_; }
  const types::DataType& type() const { return type_; }
  size_t WidthBytes() const { return pl::types::DataTypeWidthBytes(type_); }
  std::shared_ptr<arrow::DataType> arrow_type() { return types::DataTypeToArrowType(type()); }

 protected:
  std::string name_;
  types::DataType type_;
};

using DataElements = std::vector<DataElement>;

}  // namespace stirling
}  // namespace pl
