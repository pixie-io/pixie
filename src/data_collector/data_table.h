#pragma once

#include <memory>
#include <vector>

#include "src/carnot/udf/column_wrapper.h"
#include "src/common/statusor.h"
#include "src/data_collector/data_table_schema.h"
#include "src/data_collector/info_class_schema.h"
#include "third_party/arrow/cpp/src/arrow/type.h"

namespace pl {
namespace datacollector {

using ColumnWrapperRecordBatch = std::vector<carnot::udf::SharedColumnWrapper>;

enum class TableType { Null, ColumnWrapper, Arrow };

class DataTable {
 public:
  DataTable() = delete;
  virtual ~DataTable() = default;
  explicit DataTable(TableType table_type, const InfoClassSchema& schema);

  /**
   * @brief Return the schema of the table in Arrow format
   *
   * @return shared pointer to arrow Schema
   */
  std::shared_ptr<arrow::Schema> ArrowSchema();

  /**
   * @brief Given raw data, append the data to the existing Data Tables.
   *
   * @param data pointer to buffer of raw data (schema implicit).
   * @param num_rows number of rows/records in the buffer.
   *
   * @return Status
   */
  virtual Status AppendData(uint8_t* const data, uint64_t num_rows) = 0;

  /**
   * @brief Seal the data collected so far and relinquish ownership.
   *
   * @return pointer to a vector of data columns in ColumnWrapper format.
   */
  virtual StatusOr<std::unique_ptr<ColumnWrapperRecordBatch>> SealTableColumnWrapper() {
    return error::Unimplemented("Ensure you are using the right type of Data Table");
  }

  /**
   * @brief Seal the data collected so far and relinquish ownership.
   *
   * @return pointer to an Arrow RecordBatch.
   */
  virtual StatusOr<std::shared_ptr<arrow::RecordBatch>> SealTableArrow() {
    return error::Unimplemented("Ensure you are using the right type of Data Table");
  }

  /**
   * @brief Return current occupancy of the Data Table.
   *
   * @return uint32_t occupancy
   */
  uint32_t Occupancy() { return current_row_; }

  /**
   * @brief Occupancy of the Data Table as a percentage of size.
   *
   * @return double percent occupancy
   */
  double OccupancyPct() { return 1.0 * current_row_ / max_num_rows_; }

  /**
   * @brief Type of table used under the hood.
   *
   * @return TableType enum that defines the table type.
   */
  TableType TableType() { return table_type_; }

 protected:
  // Given an InfoClassSchema, generate the appropriate table. Helper for constructor.
  Status RegisterSchema(const InfoClassSchema& schema);

  // Create table buffers, based on the registered schema.
  virtual Status InitBuffers() = 0;

  // Table schema
  std::unique_ptr<DataTableSchema> table_schema_;

  // Pre-computed offsets into raw data buffers, according to schema.
  std::vector<uint32_t> offsets_;

  // Pre-computed row size, according to schema.
  uint32_t row_size_;

  // Current row in the table, where items will be appended.
  uint64_t current_row_;

  // ColumnWrapper specific members
  static constexpr uint64_t max_num_rows_ = 1024;

 private:
  // Type of table used under the hood.
  enum TableType table_type_ = TableType::Null;
};

class ColumnWrapperDataTable : public DataTable {
 public:
  ColumnWrapperDataTable() = delete;
  explicit ColumnWrapperDataTable(const InfoClassSchema& schema);
  Status AppendData(uint8_t* const data, uint64_t num_rows) override;
  StatusOr<std::unique_ptr<ColumnWrapperRecordBatch>> SealTableColumnWrapper() override;

 private:
  Status InitBuffers() override;
  std::unique_ptr<ColumnWrapperRecordBatch> columns_;
};

class ArrowDataTable : public DataTable {
 public:
  ArrowDataTable() = delete;
  explicit ArrowDataTable(const InfoClassSchema& schema);
  Status AppendData(uint8_t* const data, uint64_t num_rows) override;
  StatusOr<std::shared_ptr<arrow::RecordBatch>> SealTableArrow() override;

 private:
  Status InitBuffers() override;
  std::unique_ptr<std::vector<std::unique_ptr<arrow::ArrayBuilder>>> arrow_arrays_;
};

}  // namespace datacollector
}  // namespace pl
