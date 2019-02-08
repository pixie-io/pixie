#pragma once

#include <arrow/array.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/strings/str_format.h"
#include "src/carnot/exec/row_batch.h"
#include "src/carnot/exec/row_descriptor.h"
#include "src/carnot/udf/udf.h"
#include "src/common/status.h"
#include "src/common/statusor.h"

namespace pl {
namespace carnot {
namespace exec {

/**
 * A Column is chunked into equally-sized Arrow Arrays.
 */
class Column {
 public:
  /**
   * Creates a column with the given type.
   *
   * @ param data_type the type of the column.
   */
  explicit Column(udf::UDFDataType data_type) : data_type_(data_type) {}

  /**
   * @ return the data type for the column.
   */
  udf::UDFDataType data_type() const { return data_type_; }

  /**
   * @ return the number of chunks in the column.
   */
  int64_t numChunks() const { return chunks_.size(); }

  /**
   * Add a new chunk to the column. The chunk must be the correct Arrow datatype.
   *
   * @ param chunk the chunk to add to the column.
   */
  Status AddChunk(const std::shared_ptr<arrow::Array>& chunk);

  /**
   * @ param i the index to get the chunk from.
   */
  std::shared_ptr<arrow::Array> chunk(size_t i) {
    DCHECK(i < chunks_.size()) << absl::StrFormat("chunks_[%d] does not exist, chunks_ is size %d",
                                                  i, chunks_.size());
    return chunks_[i];
  }

 private:
  udf::UDFDataType data_type_;

  std::vector<std::shared_ptr<arrow::Array>> chunks_;
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
  StatusOr<std::unique_ptr<RowBatch>> GetRowBatch(int64_t i, std::vector<int64_t> cols);

  /**
   * @return number of column batches.
   */
  int64_t numBatches() {
    if (columns_.size() > 0) {
      return columns_[0]->numChunks();
    }
    return 0;
  }

 private:
  RowDescriptor desc_;
  std::vector<std::shared_ptr<Column>> columns_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
