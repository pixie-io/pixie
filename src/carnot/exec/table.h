#pragma once

#include <arrow/array.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "src/carnot/exec/row_batch.h"
#include "src/carnot/exec/row_descriptor.h"
#include "src/carnot/plan/relation.h"
#include "src/carnot/udf/udf.h"
#include "src/common/status.h"
#include "src/common/statusor.h"
#include "src/stirling/data_table.h"

namespace pl {
namespace carnot {
namespace exec {

/**
 * A Column is batched into equally-sized Arrow Arrays.
 */
class Column {
 public:
  /**
   * Creates a column with the given type.
   *
   * @ param data_type the type of the column.
   */
  explicit Column(udf::UDFDataType data_type, const std::string& name)
      : name_(name), data_type_(data_type) {}

  /**
   * @ return the data type for the column.
   */
  udf::UDFDataType data_type() const { return data_type_; }

  /**
   * @ return the number of batches in the column.
   */
  int64_t numBatches() const { return batches_.size(); }

  /**
   * Add a new batch to the column. The batch must be the correct Arrow datatype.
   *
   * @ param batch the batch to add to the column.
   */
  Status AddBatch(const std::shared_ptr<arrow::Array>& batch);

  /**
   * @ param i the index to get the batch from.
   */
  std::shared_ptr<arrow::Array> batch(size_t i) {
    DCHECK(i < batches_.size()) << absl::StrFormat(
        "batches_[%d] does not exist, batches_ is size %d", i, batches_.size());
    return batches_[i];
  }

  std::string name() { return name_; }

 private:
  std::string name_;
  udf::UDFDataType data_type_;

  std::vector<std::shared_ptr<arrow::Array>> batches_;
};

/**
 * A Table consists of columns that follow a given row descriptor.
 */
class Table {
 public:
  /**
   * Creates a Table.
   *
   * @ param desc the descriptor which describes the schema of the row batch
   */
  explicit Table(RowDescriptor desc) : desc_(desc) { columns_.reserve(desc_.size()); }

  /**
   * @brief Construct a new Table object along with its columns. Can be used to create
   * a table (along with columns) based on a subscription message from Stirling.
   *
   * @param relation the relation for the table.
   */
  explicit Table(const plan::Relation& relation) : desc_(relation.col_types()) {
    uint64_t num_cols = desc_.size();
    columns_.reserve(num_cols);
    for (uint64_t i = 0; i < num_cols; ++i) {
      PL_CHECK_OK(AddColumn(
          std::make_shared<Column>(relation.GetColumnType(i), relation.GetColumnName(i))));
    }
  }

  /**
   * Adds a column to the table. The column must be the correct type and be the same size as the
   * other columns.
   */
  Status AddColumn(std::shared_ptr<Column> col);

  /**
   * @ param i the index of the column to get.
   */
  std::shared_ptr<Column> GetColumn(size_t i) {
    DCHECK(i < columns_.size()) << absl::StrFormat(
        "columns_[%d] does not exist, columns_ is size %d", i, columns_.size());
    return columns_[i];
  }

  /**
   * @ param i the index of the RowBatch to get.
   * @ param cols the indices of the columns to get
   */
  StatusOr<std::unique_ptr<RowBatch>> GetRowBatch(int64_t row_batch_idx, std::vector<int64_t> cols,
                                                  arrow::MemoryPool* mem_pool);

  /**
   * @ param rb Rowbatch to write to the table.
   */
  Status WriteRowBatch(RowBatch rb);

  Status TransferRecordBatch(std::unique_ptr<pl::stirling::ColumnWrapperRecordBatch> record_batch) {
    // Check for matching types
    auto received_num_columns = record_batch->size();
    auto expected_num_columns = desc_.size();
    CHECK_EQ(expected_num_columns, received_num_columns)
        << absl::StrFormat("Table schema mismatch: expected=%u received=%u)", expected_num_columns,
                           received_num_columns);

    uint32_t i = 0;
    for (const auto& col : *record_batch) {
      auto received_type = col->DataType();
      auto expected_type = desc_.type(i);
      DCHECK_EQ(expected_type, received_type)
          << absl::StrFormat("Type mismatch [column=%u]: expected=%s received=%s", i,
                             ToString(expected_type), ToString(received_type));
      ++i;
    }

    hot_columns_.push_back(std::move(record_batch));

    return Status::OK();
  }

  /**
   * @return number of column batches.
   */
  int64_t NumBatches();

  plan::Relation GetRelation();

 private:
  RowDescriptor desc_;
  std::vector<std::shared_ptr<Column>> columns_;
  // TODO(michelle): (PL-388) Change hot_columns_ to a list-based queue.
  std::vector<std::unique_ptr<pl::stirling::ColumnWrapperRecordBatch>> hot_columns_;
  std::unordered_map<std::string, std::shared_ptr<Column>> name_to_column_map_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
