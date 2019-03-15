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
  sealed_batches_ = std::make_unique<std::vector<std::unique_ptr<ColumnWrapperRecordBatch>>>();

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

  record_batch_ = std::make_unique<ColumnWrapperRecordBatch>();

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

// Given raw data and a schema, append the data to the existing Column Wrappers.
Status DataTable::AppendData(uint8_t* const data, uint64_t num_rows) {
  // TODO(oazizi): Does it make sense to call SealActiveRecordBatch() during the AppendData?
  //               Some scenarios to consider
  //               (a) num_rows is very large (> target capacity)
  //               (b) num_rows would cause us to go above the target capacity.
  // TODO(oazizi): Investigate ColumnWrapper::Append vs ColumnWrapper::operator[].

  for (uint32_t row = 0; row < num_rows; ++row) {
    for (uint32_t field_idx = 0; field_idx < table_schema_->NumFields(); ++field_idx) {
      uint8_t* element_ptr = data + (row * row_size_) + offsets_[field_idx];

      DataType type = (*table_schema_)[field_idx].type();
      switch (type) {
        case DataType::TIME64NS: {
          auto* val_ptr = reinterpret_cast<int64_t*>(element_ptr);
          auto column = std::static_pointer_cast<types::Time64NSValueColumnWrapper>(
              (*record_batch_)[field_idx]);
          column->Append(types::Time64NSValue(*val_ptr));
        } break;
        case DataType::INT64: {
          auto* val_ptr = reinterpret_cast<int64_t*>(element_ptr);
          auto column =
              std::static_pointer_cast<types::Int64ValueColumnWrapper>((*record_batch_)[field_idx]);
          column->Append(*val_ptr);
        } break;
        case DataType::FLOAT64: {
          auto* val_ptr = reinterpret_cast<double*>(element_ptr);
          auto column = std::static_pointer_cast<types::Float64ValueColumnWrapper>(
              (*record_batch_)[field_idx]);
          column->Append(*val_ptr);
        } break;
        case DataType::STRING: {
          auto* val_ptr = reinterpret_cast<uint64_t*>(element_ptr);
          char* str_ptr = reinterpret_cast<char*>(*val_ptr);
          auto column = std::static_pointer_cast<types::StringValueColumnWrapper>(
              (*record_batch_)[field_idx]);
          column->Append(str_ptr);
        } break;
        default:
          return error::Unimplemented("Unrecognized type: $0", ToString(type));
      }
    }

    ++current_row_;
  }

  return Status::OK();
}

StatusOr<std::unique_ptr<ColumnWrapperRecordBatchVec>> DataTable::GetRecordBatches() {
  Status s = SealActiveRecordBatch();
  PL_RETURN_IF_ERROR(s);

  auto sealed_batches_ptr = std::move(sealed_batches_);

  sealed_batches_ = std::make_unique<ColumnWrapperRecordBatchVec>();

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
