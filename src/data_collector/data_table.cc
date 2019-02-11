#include <glog/logging.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/error.h"
#include "src/common/macros.h"
#include "src/data_collector/data_table.h"

namespace pl {
namespace datacollector {

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
  PL_CHECK_OK(InitBuffers());
}

ArrowDataTable::ArrowDataTable(const InfoClassSchema& schema)
    : DataTable(TableType::Arrow, schema) {
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
  DCHECK(columns_ == nullptr);

  columns_ = std::make_unique<ColumnWrapperRecordBatch>();

  for (uint32_t field_idx = 0; field_idx < table_schema_->NumFields(); ++field_idx) {
    DataType type = (*table_schema_)[field_idx].type();
    switch (type) {
      case DataType::INT64: {
        columns_->push_back(ColumnWrapper::Make(DataType::INT64, max_num_rows_));
      } break;
      case DataType::FLOAT64: {
        columns_->push_back(ColumnWrapper::Make(DataType::FLOAT64, max_num_rows_));
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

  arrow_arrays_ = std::make_unique<std::vector<std::unique_ptr<arrow::ArrayBuilder>>>();

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
  // FIXME(oazizi): Need to handle this case, with one or more intermediate pushes.
  // Note that currently pushes are handled by the data collector top-level, so
  // need to find the right architecture for this.
  CHECK(current_row_ + num_rows < max_num_rows_);

  for (uint32_t row = 0; row < num_rows; ++row) {
    for (uint32_t field_idx = 0; field_idx < table_schema_->NumFields(); ++field_idx) {
      uint8_t* element_ptr = data + (row * row_size_) + offsets_[field_idx];

      DataType type = (*table_schema_)[field_idx].type();
      switch (type) {
        case DataType::INT64: {
          auto* val_ptr = reinterpret_cast<int64_t*>(element_ptr);
          auto column = std::static_pointer_cast<carnot::udf::Int64ValueColumnWrapper>(
              (*columns_)[field_idx]);
          (*column)[current_row_] = *val_ptr;
        } break;
        case DataType::FLOAT64: {
          auto* val_ptr = reinterpret_cast<double*>(element_ptr);
          auto column = std::static_pointer_cast<carnot::udf::Float64ValueColumnWrapper>(
              (*columns_)[field_idx]);
          (*column)[current_row_] = *val_ptr;
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

StatusOr<std::unique_ptr<ColumnWrapperRecordBatch>>
ColumnWrapperDataTable::SealTableColumnWrapper() {
  auto tmp = std::move(columns_);
  Status s = InitBuffers();
  PL_RETURN_IF_ERROR(s);
  return tmp;
}

StatusOr<std::shared_ptr<arrow::RecordBatch>> ArrowDataTable::SealTableArrow() {
  std::vector<std::shared_ptr<arrow::Array>> arrow_cols(table_schema_->NumFields());

  auto schema = ArrowSchema();

  for (uint32_t field_idx = 0; field_idx < table_schema_->NumFields(); ++field_idx) {
    auto s = (*arrow_arrays_)[field_idx]->Finish(&(arrow_cols[field_idx]));
    CHECK(s.ok());
  }

  arrow_arrays_ = nullptr;

  // Create Arrow Record Batch.
  auto record_batch = arrow::RecordBatch::Make(schema, current_row_, arrow_cols);

  PL_RETURN_IF_ERROR(InitBuffers());

  return record_batch;
}

}  // namespace datacollector
}  // namespace pl
