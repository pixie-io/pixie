#include "src/stirling/types.h"

#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"

namespace pl {
namespace stirling {

using pl::types::DataType;

Status InitRecordBatch(const DataElements& data_elements, int target_capacity,
                       types::ColumnWrapperRecordBatch* record_batch) {
  for (const auto& element : data_elements) {
    DataType type = element.type();
    switch (type) {
      case DataType::TIME64NS: {
        auto col = types::ColumnWrapper::Make(DataType::TIME64NS, 0);
        col->Reserve(target_capacity);
        record_batch->push_back(col);
      } break;
      case DataType::INT64: {
        auto col = types::ColumnWrapper::Make(DataType::INT64, 0);
        col->Reserve(target_capacity);
        record_batch->push_back(col);
      } break;
      case DataType::FLOAT64: {
        auto col = types::ColumnWrapper::Make(DataType::FLOAT64, 0);
        col->Reserve(target_capacity);
        record_batch->push_back(col);
      } break;
      case DataType::STRING: {
        auto col = types::ColumnWrapper::Make(DataType::STRING, 0);
        col->Reserve(target_capacity);
        record_batch->push_back(col);
      } break;
      default:
        return error::Unimplemented("Unrecognized type: $0", ToString(type));
    }
  }
  return Status();
}

}  // namespace stirling
}  // namespace pl
