#include <glog/logging.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/error.h"
#include "src/common/macros.h"
#include "src/stirling/data_table.h"

namespace pl {
namespace stirling {

using carnot::udf::ColumnWrapper;
using types::DataType;

std::string DataTypeToString(DataType type) {
  switch (type) {
    case DataType::BOOLEAN:
      return "BOOLEAN";
    case DataType::INT64:
      return "INT64";
    case DataType::FLOAT64:
      return "FLOAT64";
    case DataType::STRING:
      return "STRING";
    default:
      DCHECK(false) << "No ToString() for this DataType";
      return "UNKNOWN";
  }
}

DataTable::DataTable(enum TableType table_type, const InfoClassSchema& schema)
    : table_type_(table_type) {
  Status s = RegisterSchema(schema);
  CHECK(s.ok());
}

ColumnWrapperDataTable::ColumnWrapperDataTable(const InfoClassSchema& schema)
    : DataTable(TableType::ColumnWrapper, schema) {
  sealed_batches_ = std::make_unique<std::vector<std::unique_ptr<ColumnWrapperRecordBatch>>>();

  PL_CHECK_OK(InitBuffers());
}

ArrowDataTable::ArrowDataTable(const InfoClassSchema& schema)
    : DataTable(TableType::Arrow, schema) {
  sealed_batches_ = std::make_unique<std::vector<std::shared_ptr<arrow::RecordBatch>>>();

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

std::shared_ptr<arrow::Schema> DataTable::ArrowSchema() {
  std::vector<std::shared_ptr<arrow::Field>> schema_vector(table_schema_->NumFields());

  // Make an Arrow Schema
  for (uint32_t field_idx = 0; field_idx < table_schema_->NumFields(); ++field_idx) {
    auto name = (*table_schema_)[field_idx].name();
    auto type = (*table_schema_)[field_idx].arrow_type();
    schema_vector[field_idx] = arrow::field(name, type);
  }
  auto schema = std::make_shared<arrow::Schema>(schema_vector);

  return schema;
}

Status ColumnWrapperDataTable::InitBuffers() {
  DCHECK(record_batch_ == nullptr);

  record_batch_ = std::make_unique<ColumnWrapperRecordBatch>();

  for (uint32_t field_idx = 0; field_idx < table_schema_->NumFields(); ++field_idx) {
    DataType type = (*table_schema_)[field_idx].type();
    switch (type) {
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
      default:
        return error::Unimplemented("Unrecognized type: $0", DataTypeToString(type));
    }
  }

  current_row_ = 0;

  return Status::OK();
}

Status ArrowDataTable::InitBuffers() {
  DCHECK(arrow_arrays_ == nullptr);

  arrow_arrays_ = std::make_unique<ArrowRecordBatch>();

  auto mem_pool = arrow::default_memory_pool();

  for (uint32_t field_idx = 0; field_idx < table_schema_->NumFields(); ++field_idx) {
    DataType type = (*table_schema_)[field_idx].type();
    switch (type) {
      case DataType::INT64: {
        std::unique_ptr<arrow::ArrayBuilder> col = std::make_unique<arrow::Int64Builder>();
        PL_RETURN_IF_ERROR(arrow::MakeBuilder(mem_pool, arrow::int64(), &col));
        arrow_arrays_->push_back(std::move(col));
      } break;
      case DataType::FLOAT64: {
        std::unique_ptr<arrow::ArrayBuilder> col = std::make_unique<arrow::DoubleBuilder>();
        PL_RETURN_IF_ERROR(arrow::MakeBuilder(mem_pool, arrow::float64(), &col));
        arrow_arrays_->push_back(std::move(col));
      } break;
      default:
        return error::Unimplemented("Unrecognized type: $0", DataTypeToString(type));
    }
  }

  current_row_ = 0;

  return Status::OK();
}

// Given raw data and a schema, append the data to the existing Column Wrappers.
Status ColumnWrapperDataTable::AppendData(uint8_t* const data, uint64_t num_rows) {
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
        case DataType::INT64: {
          auto* val_ptr = reinterpret_cast<int64_t*>(element_ptr);
          auto column = std::static_pointer_cast<carnot::udf::Int64ValueColumnWrapper>(
              (*record_batch_)[field_idx]);
          column->Append(*val_ptr);
        } break;
        case DataType::FLOAT64: {
          auto* val_ptr = reinterpret_cast<double*>(element_ptr);
          auto column = std::static_pointer_cast<carnot::udf::Float64ValueColumnWrapper>(
              (*record_batch_)[field_idx]);
          column->Append(*val_ptr);
        } break;
        default:
          return error::Unimplemented("Unrecognized type: $0", DataTypeToString(type));
      }
    }

    ++current_row_;
  }

  return Status::OK();
}

// Given raw data and a schema, append the data to the existing Arrow Arrays.
Status ArrowDataTable::AppendData(uint8_t* const data, uint64_t num_rows) {
  // TODO(oazizi): Does it make sense to call SealActiveRecordBatch() during the AppendData?
  //               (see comments in ColumnWrapperDataTable::AppendData)  //
  // TODO(oazizi): Investigate Append vs UnsafeAppend.

  for (uint32_t row = 0; row < num_rows; ++row) {
    for (uint32_t field_idx = 0; field_idx < table_schema_->NumFields(); ++field_idx) {
      uint8_t* element_ptr = data + (row * row_size_) + offsets_[field_idx];

      DataType type = (*table_schema_)[field_idx].type();
      switch (type) {
        case DataType::INT64: {
          auto* val_ptr = reinterpret_cast<int64_t*>(element_ptr);
          auto* array = static_cast<arrow::Int64Builder*>((*arrow_arrays_)[field_idx].get());
          PL_RETURN_IF_ERROR(array->Append(*val_ptr));
        } break;
        case DataType::FLOAT64: {
          auto* val_ptr = reinterpret_cast<double*>(element_ptr);
          auto* array = static_cast<arrow::DoubleBuilder*>((*arrow_arrays_)[field_idx].get());
          PL_RETURN_IF_ERROR(array->Append(*val_ptr));
        } break;
        default:
          return error::Unimplemented("Unrecognized type: $0", DataTypeToString(type));
      }
    }
  }

  return Status::OK();
}

StatusOr<std::unique_ptr<ColumnWrapperRecordBatchVector>>
ColumnWrapperDataTable::GetColumnWrapperRecordBatches() {
  Status s = SealActiveRecordBatch();
  PL_RETURN_IF_ERROR(s);

  auto sealed_batches_ptr = std::move(sealed_batches_);

  sealed_batches_ = std::make_unique<ColumnWrapperRecordBatchVector>();

  return std::move(sealed_batches_ptr);
}

StatusOr<std::unique_ptr<ArrowRecordBatchVector>> ArrowDataTable::GetArrowRecordBatches() {
  Status s = SealActiveRecordBatch();
  PL_RETURN_IF_ERROR(s);

  auto sealed_batches_ptr = std::move(sealed_batches_);

  sealed_batches_ = std::make_unique<ArrowRecordBatchVector>();

  return std::move(sealed_batches_ptr);
}

Status ColumnWrapperDataTable::SealActiveRecordBatch() {
  for (uint32_t i = 0; i < table_schema_->NumFields(); ++i) {
    auto col = (*record_batch_)[i];
    col->ShrinkToFit();
  }
  sealed_batches_->push_back(std::move(record_batch_));
  Status s = InitBuffers();
  PL_RETURN_IF_ERROR(s);

  return Status::OK();
}

Status ArrowDataTable::SealActiveRecordBatch() {
  std::vector<std::shared_ptr<arrow::Array>> arrow_cols(table_schema_->NumFields());

  auto schema = ArrowSchema();

  for (uint32_t field_idx = 0; field_idx < table_schema_->NumFields(); ++field_idx) {
    auto s = (*arrow_arrays_)[field_idx]->Finish(&(arrow_cols[field_idx]));
    CHECK(s.ok());
  }

  arrow_arrays_ = nullptr;

  // Create Arrow Record Batch.
  auto record_batch = arrow::RecordBatch::Make(schema, current_row_, arrow_cols);

  sealed_batches_->push_back(record_batch);

  PL_RETURN_IF_ERROR(InitBuffers());

  return Status::OK();
}

}  // namespace stirling
}  // namespace pl
