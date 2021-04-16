#pragma once

#include <arrow/array.h>
#include <arrow/record_batch.h>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <absl/base/internal/spinlock.h>
#include <absl/strings/str_format.h>
#include "src/common/base/base.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/table_store/schema/relation.h"
#include "src/table_store/schema/row_batch.h"
#include "src/table_store/schema/row_descriptor.h"
#include "src/table_store/schemapb/schema.pb.h"

DECLARE_int32(table_store_table_size_limit);

namespace px {
namespace table_store {

using RecordBatchSPtr = std::shared_ptr<arrow::RecordBatch>;

struct BatchPosition {
  int64_t batch_idx = 0;
  int64_t row_idx = 0;
  bool FoundValidBatches() { return batch_idx != -1; }
};

struct TableStats {
  int64_t bytes;
  int64_t num_batches;
  int64_t batches_added;
  int64_t batches_expired;
  int64_t max_table_size;
};

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
  explicit Column(types::DataType data_type, std::string name)
      : name_(std::move(name)), data_type_(data_type) {}

  /**
   * @ return the data type for the column.
   */
  types::DataType data_type() const { return data_type_; }

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
   * Delete the next batch in the column.
   * @return a status of whether deletion was successful.
   */
  Status DeleteNextBatch();

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
  types::DataType data_type_;

  std::deque<std::shared_ptr<arrow::Array>> batches_;
};

/**
 * A Table consists of columns that follow a given row descriptor.
 */
class Table : public NotCopyable {
 public:
  static inline std::shared_ptr<Table> Create(const schema::Relation& relation) {
    // Create naked pointer, because std::make_shared() cannot access the private ctor.
    return std::shared_ptr<Table>(new Table(relation, FLAGS_table_store_table_size_limit));
  }

  /**
   * @brief Construct a new Table object along with its columns. Can be used to create
   * a table (along with columns) based on a subscription message from Stirling.
   *
   * @param relation the relation for the table.
   * @param max_table_size the maximum number of bytes that the table can hold. This is limitless
   * (-1) by default.
   */
  explicit Table(const schema::Relation& relation, int64_t max_table_size);

  /**
   * @ param i the index of the column to get.
   */
  std::shared_ptr<Column> GetColumn(size_t i) const {
    DCHECK(i < columns_.size()) << absl::StrFormat(
        "columns_[%d] does not exist, columns_ is size %d", i, columns_.size());
    return columns_[i];
  }

  /**
   * @ param i the index of the RowBatch to get.
   * @ param cols the indices of the columns to get
   */
  StatusOr<std::unique_ptr<schema::RowBatch>> GetRowBatch(int64_t row_batch_idx,
                                                          std::vector<int64_t> cols,
                                                          arrow::MemoryPool* mem_pool) const;
  /**
   * Get a slice of the row batch at the given index.
   * @ param i the index of the RowBatch to get.
   * @ param cols the indices of the columns to get.
   * @ param mem_pool the arrow memory pool.
   * @ param offset the first index of the slice of the RowBatch.
   * @ param end the ending index of the slice of the RowBatch (not inclusive).
   */
  StatusOr<std::unique_ptr<schema::RowBatch>> GetRowBatchSlice(int64_t row_batch_idx,
                                                               std::vector<int64_t> cols,
                                                               arrow::MemoryPool* mem_pool,
                                                               int64_t offset, int64_t end) const;

  /**
   * @ param rb Rowbatch to write to the table.
   */
  Status WriteRowBatch(schema::RowBatch rb);

  /**
   * Transfers the given record batch (from Stirling) into the Table.
   *
   * @param record_batch the record batch to be appended to the Table.
   * @return status
   */
  Status TransferRecordBatch(std::unique_ptr<px::types::ColumnWrapperRecordBatch> record_batch);

  /**
   * @return number of column batches.
   */
  int64_t NumBatches() const;

  /**
   * @return number of columns.
   */
  int64_t NumColumns() const { return columns_.size(); }

  /**
   * @return the size of the table in bytes.
   */
  int64_t NumBytes() const { return bytes_; }

  schema::Relation GetRelation() const;
  StatusOr<std::vector<RecordBatchSPtr>> GetTableAsRecordBatches() const;

  /**
   * @param the timestamp to search for.
   * @param mem_pool the arrow memory pool to use.
   * @return the batch position (batch number and row number in that batch) of the row with the
   * first timestamp greater than or equal to the given time.
   */
  BatchPosition FindBatchPositionGreaterThanOrEqual(int64_t time, arrow::MemoryPool* mem_pool);

  // TODO(michellenguyen, PL-404): Time should always be column 0.
  int64_t FindTimeColumn();

  /**
   * Covert the table and store in passed in proto.
   * @param table_proto The table proto to write to.
   * @return Status of conversion.
   */
  Status ToProto(table_store::schemapb::Table* table_proto) const;

  TableStats GetTableStats() const;

 private:
  /**
   * Adds a column to the table. The column must have the same type as the column expected by the
   * relation and be the same size as the other columns.
   */
  Status AddColumn(std::shared_ptr<Column> col);

  Status ExpireRowBatches(int64_t row_batch_size);
  Status DeleteNextRowBatch();

  int64_t FindBatchGreaterThanOrEqual(int64_t time_col_idx, int64_t time,
                                      arrow::MemoryPool* mem_pool)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(cold_batches_lock_);
  int64_t FindBatchGreaterThanOrEqual(int64_t time_col_idx, int64_t time,
                                      arrow::MemoryPool* mem_pool, int64_t start, int64_t end)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(cold_batches_lock_);
  std::shared_ptr<arrow::Array> GetColumnBatch(int64_t col, int64_t batch,
                                               arrow::MemoryPool* mem_pool)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(cold_batches_lock_);
  int64_t NumBatchesUnlocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(cold_batches_lock_);
  schema::RowDescriptor desc_;
  std::vector<std::shared_ptr<Column>> columns_;
  // TODO(michellenguyen, PL-388): Change hot_batches_ to a list-based queue.
  std::unordered_map<std::string, std::shared_ptr<Column>> name_to_column_map_;

  mutable std::deque<std::unique_ptr<px::types::ColumnWrapperRecordBatch>> hot_batches_;
  mutable absl::base_internal::SpinLock hot_batches_lock_;

  mutable absl::base_internal::SpinLock cold_batches_lock_;

  int64_t batches_expired_ = 0;
  int64_t bytes_ = 0;
  int64_t batches_added_ = 0;
  int64_t max_table_size_ = 0;
};

}  // namespace table_store
}  // namespace px
