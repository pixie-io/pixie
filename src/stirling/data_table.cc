#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/common.h"
#include "src/shared/types/type_utils.h"
#include "src/stirling/data_table.h"

namespace pl {
namespace stirling {

using types::ColumnWrapper;
using types::DataType;

DataTable::DataTable(const InfoClassSchema& schema) {
  Status s = RegisterSchema(schema);
  sealed_batches_ =
      std::make_unique<std::vector<std::unique_ptr<types::ColumnWrapperRecordBatch>>>();

  PL_CHECK_OK(InitBuffers());
}

// Given an InfoClassSchema, generate the appropriate table.
Status DataTable::RegisterSchema(const InfoClassSchema& schema) {
  table_schema_ = std::make_unique<DataTableSchema>(schema);

  size_t current_offset = 0;
  for (uint32_t i = 0; i < table_schema_->NumFields(); ++i) {
    auto& elementInfo = (*table_schema_)[i];
    elementInfo.SetOffset(current_offset);
    offsets_.push_back(current_offset);
    current_offset += elementInfo.WidthBytes();
  }

  row_size_ = current_offset;

  return Status::OK();
}

Status DataTable::InitBuffers() {
  DCHECK(record_batch_ == nullptr);

  record_batch_ = std::make_unique<types::ColumnWrapperRecordBatch>();

  for (uint32_t field_idx = 0; field_idx < table_schema_->NumFields(); ++field_idx) {
    DataType type = (*table_schema_)[field_idx].type();
    switch (type) {
      case DataType::TIME64NS: {
        auto col = ColumnWrapper::Make(DataType::TIME64NS, 0);
        col->Reserve(target_capacity_);
        record_batch_->push_back(col);
      } break;
      case DataType::INT64: {
        auto col = ColumnWrapper::Make(DataType::INT64, 0);
        col->Reserve(target_capacity_);
        record_batch_->push_back(col);
      } break;
      case DataType::FLOAT64: {
        auto col = ColumnWrapper::Make(DataType::FLOAT64, 0);
        col->Reserve(target_capacity_);
        record_batch_->push_back(col);
      } break;
      case DataType::STRING: {
        auto col = ColumnWrapper::Make(DataType::STRING, 0);
        col->Reserve(target_capacity_);
        record_batch_->push_back(col);
      } break;
      default:
        return error::Unimplemented("Unrecognized type: $0", ToString(type));
    }
  }

  current_row_ = 0;

  return Status::OK();
}

StatusOr<std::unique_ptr<types::ColumnWrapperRecordBatchVec>> DataTable::GetRecordBatches() {
  Status s = SealActiveRecordBatch();
  PL_RETURN_IF_ERROR(s);

  auto sealed_batches_ptr = std::move(sealed_batches_);

  sealed_batches_ = std::make_unique<types::ColumnWrapperRecordBatchVec>();

  return std::move(sealed_batches_ptr);
}

Status DataTable::SealActiveRecordBatch() {
  for (uint32_t i = 0; i < table_schema_->NumFields(); ++i) {
    auto col = (*record_batch_)[i];
    col->ShrinkToFit();
  }
  sealed_batches_->push_back(std::move(record_batch_));
  Status s = InitBuffers();
  PL_RETURN_IF_ERROR(s);

  return Status::OK();
}

}  // namespace stirling
}  // namespace pl
