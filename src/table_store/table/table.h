/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <absl/synchronization/mutex.h>
#include <arrow/array.h>
#include <arrow/record_batch.h>
#include <algorithm>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <absl/base/internal/spinlock.h>
#include <absl/strings/str_format.h>
#include "src/common/base/base.h"
#include "src/common/metrics/metrics.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/table_store/schema/relation.h"
#include "src/table_store/schema/row_batch.h"
#include "src/table_store/schema/row_descriptor.h"
#include "src/table_store/schemapb/schema.pb.h"
#include "src/table_store/table/table_metrics.h"

DECLARE_int32(table_store_table_size_limit);

namespace px {
namespace table_store {

using RecordBatchSPtr = std::shared_ptr<arrow::RecordBatch>;

struct TableStats {
  int64_t bytes;
  int64_t cold_bytes;
  int64_t num_batches;
  int64_t batches_added;
  int64_t batches_expired;
  int64_t compacted_batches;
  int64_t max_table_size;
  int64_t min_time;
};

struct BatchSlice {
  // All properties with the unsafe_ prefix should not be touched except inside of Table with the
  // proper lock held.
  mutable bool unsafe_is_hot;
  mutable int64_t unsafe_batch_index;
  mutable int64_t unsafe_row_start;
  mutable int64_t unsafe_row_end;

  mutable int64_t generation = -1;
  int64_t uniq_row_start_idx = -1;
  int64_t uniq_row_end_idx = -1;

  int64_t Size() const { return uniq_row_end_idx - uniq_row_start_idx + 1; }
  bool IsValid() const { return uniq_row_start_idx != -1 && uniq_row_end_idx != -1; }
  static BatchSlice Invalid() { return BatchSlice{false, -1, -1, -1}; }
  static BatchSlice Cold(int64_t cold_index, int64_t row_start, int64_t row_end, int64_t generation,
                         std::pair<int64_t, int64_t> row_ids) {
    return BatchSlice{false,      cold_index,    row_start,     row_end,
                      generation, row_ids.first, row_ids.second};
  }
  static BatchSlice Cold(int64_t cold_index, int64_t row_start, int64_t row_end, int64_t generation,
                         int64_t uniq_row_start_idx, int64_t uniq_row_end_idx) {
    return BatchSlice{false,      cold_index,         row_start,       row_end,
                      generation, uniq_row_start_idx, uniq_row_end_idx};
  }
  static BatchSlice Hot(int64_t hot_index, int64_t row_start, int64_t row_end, int64_t generation,
                        std::pair<int64_t, int64_t> row_ids) {
    return BatchSlice{true,       hot_index,     row_start,     row_end,
                      generation, row_ids.first, row_ids.second};
  }
  static BatchSlice Hot(int64_t hot_index, int64_t row_start, int64_t row_end, int64_t generation,
                        int64_t uniq_row_start_idx, int64_t uniq_row_end_idx) {
    return BatchSlice{true,       hot_index,          row_start,       row_end,
                      generation, uniq_row_start_idx, uniq_row_end_idx};
  }
};

class ArrowArrayCompactor {
 public:
  ArrowArrayCompactor(const schema::Relation& rel, arrow::MemoryPool* mem_pool);
  Status AppendColumn(int64_t col_idx, std::shared_ptr<arrow::Array> arr);

  Status Finish();
  const std::vector<std::shared_ptr<arrow::Array>>& output_columns() const {
    return output_columns_;
  }
  int64_t Size() const { return bytes_; }

 private:
  int64_t bytes_ = 0;
  std::vector<std::shared_ptr<arrow::Array>> output_columns_;
  std::vector<std::unique_ptr<arrow::ArrayBuilder>> builders_;
  std::vector<types::DataType> column_types_;

  template <types::DataType TDataType>
  Status AppendColumnTyped(int64_t col_idx, std::shared_ptr<arrow::Array> arr) {
    auto builder_untyped = builders_[col_idx].get();
    auto builder = static_cast<typename types::DataTypeTraits<TDataType>::arrow_builder_type*>(
        builder_untyped);
    auto typed_arr =
        std::static_pointer_cast<typename types::DataTypeTraits<TDataType>::arrow_array_type>(arr);
    PL_RETURN_IF_ERROR(builder->Reserve(typed_arr->length()));
    for (int i = 0; i < typed_arr->length(); ++i) {
      builder->UnsafeAppend(typed_arr->Value(i));
    }
    bytes_ += types::GetArrowArrayBytes<TDataType>(typed_arr.get());
    return Status::OK();
  }

  template <types::DataType TDataType>
  Status FinishTyped(int64_t col_idx) {
    auto builder_untyped = builders_[col_idx].get();
    auto builder = static_cast<typename types::DataTypeTraits<TDataType>::arrow_builder_type*>(
        builder_untyped);
    PL_RETURN_IF_ERROR(builder->Finish(&output_columns_[col_idx]));
    return Status::OK();
  }
};

/**
 * Table stores data in two separate partitions, hot and cold. Hot data is "hot" from the
 * perspective of writes, in other words data is first written to the hot partitiion, and then later
 * moved to the cold partition. Reads can hit both hot and cold data. Hot data can be written in
 * RecordBatch format (i.e. for writes from stirling) or schema::RowBatch format (i.e. for writes
 * from MemorySinkNodes, which are not currently used). Hot data is stored in a deque, while cold
 * data is stored in a ring buffer. Hot data is eventually converted to arrow arrays either during
 * compaction and transfer to cold or during a read. If the conversion happens on read then we store
 * the arrow array in a cache with the hot batch so that future reads, before this batch is
 * transferred to cold, don't also need to convert to arrow.
 *
 * Synchronization Scheme:
 * The hot and cold partitions are synchronized separately with spinlocks. Additionally, the
 * generation of the store is protected by a spinlock.
 *
 * Compaction Scheme:
 * Hot batches are compacted into batches of minimum size min_cold_batch_size_ bytes. The compaction
 * routine should be called periodically but that is not the responsibility of this class.
 *
 * Time and Row Indexing:
 * The first and last values of the time columns for each batch are stored as intervals in
 * (hot/cold)_time_, which internally maintains a sorted list for O(logN) time lookup. Additionally,
 * this class supports multiple batches with the same timestamps. In order to support this, we keep
 * track of an incrementing identifier for each row (we store only the identifiers of the first and
 * last rows in a batch). This allows us to support returning only valid data even if a compaction
 * occurs in the middle of query execution. For example, suppose we have two batches of data all
 * with the same timestamps. Suppose a query reads through all this data. Now after reading the
 * first batch, compaction is called and both batches are compacted together and put into cold
 * storage. Then if the query were to naively try to access the "second" batch since it already saw
 * the "first" batch, there wouldn't be any more data and the query will have skipped all the rows
 * in what was initially the "second" batch. Instead, the BatchSlice object stores the unique row
 * identifiers of the first and last row of that batch, so that when NextBatch is called on that
 * batch it can work out that it needs to return a slice of the batch with the original "second"
 * batch's data.
 */
class Table : public NotCopyable {
  using RecordBatchPtr = std::unique_ptr<px::types::ColumnWrapperRecordBatch>;
  using ArrowArrayPtr = std::shared_ptr<arrow::Array>;
  using ColumnBuffer = std::vector<ArrowArrayPtr>;
  using TimeInterval = std::pair<int64_t, int64_t>;
  using RowIDInterval = std::pair<int64_t, int64_t>;

  struct RecordBatchWithCache {
    RecordBatchPtr record_batch;
    // Whenever we have to convert a hot batch to an arrow array, we store the arrow array in
    // this cache. Compaction will eventually take these arrow arrays and move them into cold.
    mutable std::vector<ArrowArrayPtr> arrow_cache;
    mutable std::vector<bool> cache_validity;
  };

  using RecordOrRowBatch = std::variant<RecordBatchWithCache, schema::RowBatch>;

  static inline constexpr int64_t kDefaultColdBatchMinSize = 64 * 1024;

 public:
  static inline constexpr int64_t kMaxBatchesPerCompactionCall = 256;
  using StopPosition = int64_t;
  static inline std::shared_ptr<Table> Create(std::string_view table_name,
                                              const schema::Relation& relation) {
    // Create naked pointer, because std::make_shared() cannot access the private ctor.
    return std::shared_ptr<Table>(
        new Table(table_name, relation, FLAGS_table_store_table_size_limit));
  }

  /**
   * @brief Construct a new Table object along with its columns. Can be used to create
   * a table (along with columns) based on a subscription message from Stirling.
   *
   * @param relation the relation for the table.
   * @param max_table_size the maximum number of bytes that the table can hold. This is limitless
   * (-1) by default.
   */
  explicit Table(std::string_view table_name, const schema::Relation& relation,
                 size_t max_table_size)
      : Table(table_name, relation, max_table_size, kDefaultColdBatchMinSize) {}

  Table(std::string_view table_name, const schema::Relation& relation, size_t max_table_size,
        size_t min_cold_batch_size);

  /**
   * Get a RowBatch of data corresponding to the passed in BatchSlice.
   * @param slice the BatchSlice to get the data for.
   * @param cols a vector of column indices to get data for.
   * @param mem_pool the arrow memory pool to use if the slice is in hot storage.
   * @return a unique ptr to a RowBatch with the requested data.
   */
  StatusOr<std::unique_ptr<schema::RowBatch>> GetRowBatchSlice(const BatchSlice& slice,
                                                               const std::vector<int64_t>& cols,
                                                               arrow::MemoryPool* mem_pool) const;

  /**
   * Writes a row batch to the table.
   * @param rb Rowbatch to write to the table.
   */
  Status WriteRowBatch(const schema::RowBatch& rb);

  /**
   * Transfers the given record batch (from Stirling) into the Table.
   *
   * @param record_batch the record batch to be appended to the Table.
   * @return status
   */
  Status TransferRecordBatch(std::unique_ptr<px::types::ColumnWrapperRecordBatch> record_batch);

  schema::Relation GetRelation() const;
  StatusOr<std::vector<RecordBatchSPtr>> GetTableAsRecordBatches() const;

  /**
   * @param time the timestamp to search for.
   * @param mem_pool the arrow memory pool.
   * @return the BatchSlice of the first row with timestamp greater than or equal to the given time,
   * until the end of its corresponding row batch.
   */
  StatusOr<BatchSlice> FindBatchSliceGreaterThanOrEqual(int64_t time,
                                                        arrow::MemoryPool* mem_pool) const;

  /**
   * @param time the timestamp to search for.
   * @param mem_pool the arrow memory pool.
   * @return the BatchSlice of the last row with timestamp less than or equal to the given time,
   * until the end of its corresponding row batch.
   */
  StatusOr<StopPosition> FindStopPositionForTime(int64_t time, arrow::MemoryPool* mem_pool) const;

  /**
   * Covert the table and store in passed in proto.
   * @param table_proto The table proto to write to.
   * @return Status of conversion.
   */
  Status ToProto(table_store::schemapb::Table* table_proto) const;

  TableStats GetTableStats() const;

  /**
   * Gets the BatchSlice corresponding to the next batch after the given batch.
   * The BatchSlice will be cut short to ensure it doesn't extend past the given StopPosition.
   * If there are no more batches or the next BatchSlice would be entirely beyond the given
   * StopPosition then an Invalid BatchSlice is returned. Using this function iteratively will
   * ensure that all rows are seen even if a table compaction occurs during the iteration. This
   * means in some cases NextBatch will return a BatchSlice with the same batch index as the passed
   * in batch, but with a different slice of rows.
   *
   * @param slice The BatchSlice to get the next batch for.
   * @param stop The StopPosition (unique row identifier) that the NextBatch should not extend
   * beyond.
   * @return A BatchSlice corresponding to the next slice of rows after the given slice but before
   * the stop position.
   */
  BatchSlice NextBatch(const BatchSlice& slice, StopPosition stop) const;
  BatchSlice NextBatch(const BatchSlice& slice) const { return NextBatch(slice, End()); }
  /**
   * @returns a BatchSlice corresponding to the first batch in the table.
   */
  BatchSlice FirstBatch() const;
  /**
   * @return A stop position 1 row past the end of the table.
   */
  StopPosition End() const;

  /**
   * Reduces the extent of a BatchSlice to ensure that it doesn't include rows past the given stop
   * position.
   * @param slice the BatchSlice to cut short if its past the stop position.
   * @param stop the StopPosition to cut short at.
   * @return a new BatchSlice that doesn't extend past the StopPosition.
   */
  BatchSlice SliceIfPastStop(const BatchSlice& slice, StopPosition stop) const;

  /**
   * Compacts hot batches into min_cold_batch_size_ sized cold batches. Each call to
   * CompactHotToCold will create a maximum of kMaxBatchesPerCompactionCall cold batches.
   * @param mem_pool arrow MemoryPool to be used for creating new cold batches.
   */
  Status CompactHotToCold(arrow::MemoryPool* mem_pool);

 private:
  TableMetrics metrics_;
  Status ExpireRowBatches(int64_t row_batch_size);

  schema::Relation rel_;

  mutable absl::base_internal::SpinLock stats_lock_;
  int64_t batches_expired_ ABSL_GUARDED_BY(stats_lock_) = 0;
  int64_t cold_bytes_ ABSL_GUARDED_BY(stats_lock_) = 0;
  int64_t hot_bytes_ ABSL_GUARDED_BY(stats_lock_) = 0;
  int64_t batches_added_ ABSL_GUARDED_BY(stats_lock_) = 0;
  int64_t compacted_batches_ ABSL_GUARDED_BY(stats_lock_) = 0;
  int64_t max_table_size_ = 0;
  int64_t min_cold_batch_size_;

  mutable absl::Mutex hot_lock_;
  std::deque<RecordOrRowBatch> hot_batches_ ABSL_GUARDED_BY(hot_lock_);

  mutable absl::Mutex cold_lock_;
  std::vector<ColumnBuffer> cold_column_buffers_ ABSL_GUARDED_BY(cold_lock_);

  // The generation lock must be held during compaction and
  // expiration, and anytime one would like to access the unsafe_ attributes of BatchSlice.
  mutable absl::Mutex generation_lock_;
  // Generation of the HotColdDataStore is incremented whenever a change to the store would
  // invalidate some BatchSlice', eg. during compaction or hot expiration.
  int64_t generation_ ABSL_GUARDED_BY(generation_lock_);

  // We store ring buffer properties at the table level rather than for each individual Column.
  int64_t ring_front_idx_ ABSL_GUARDED_BY(cold_lock_) = 0;
  int64_t ring_back_idx_ ABSL_GUARDED_BY(cold_lock_) = -1;
  int64_t ring_capacity_ ABSL_GUARDED_BY(cold_lock_);

  // Counter to assign a unique row ID to each row. Synchronized by hot_lock_ since its only
  // accessed on a hot write.
  int64_t next_row_id_ ABSL_GUARDED_BY(hot_lock_) = 0;
  std::deque<RowIDInterval> hot_row_ids_ ABSL_GUARDED_BY(hot_lock_);
  std::deque<TimeInterval> hot_time_ ABSL_GUARDED_BY(hot_lock_);
  std::deque<RowIDInterval> cold_row_ids_ ABSL_GUARDED_BY(cold_lock_);
  std::deque<TimeInterval> cold_time_ ABSL_GUARDED_BY(cold_lock_);

  int64_t time_col_idx_ = -1;

  Status WriteHot(RecordBatchPtr record_batch);
  Status WriteHot(const schema::RowBatch& rb);
  Status UpdateTimeRowIndices(const schema::RowBatch& rb) ABSL_EXCLUSIVE_LOCKS_REQUIRED(hot_lock_);
  Status UpdateTimeRowIndices(types::ColumnWrapperRecordBatch* record_batch)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(hot_lock_);

  Status ExpireBatch();
  Status ExpireHot();
  StatusOr<bool> ExpireCold();
  Status CompactSingleBatch(arrow::MemoryPool* mem_pool);

  Status AddBatchSliceToRowBatch(const BatchSlice& slice, const std::vector<int64_t>& cols,
                                 schema::RowBatch* output_rb, arrow::MemoryPool* mem_pool) const;
  ArrowArrayPtr GetHotColumnUnlocked(const RecordBatchWithCache* record_batch_ptr, int64_t col_idx,
                                     arrow::MemoryPool* mem_pool) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(hot_lock_);

  int64_t NumBatches() const;
  int64_t ColdBatchLengthUnlocked(int64_t ring_index) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(cold_lock_);
  int64_t HotBatchLengthUnlocked(int64_t hot_index) const ABSL_EXCLUSIVE_LOCKS_REQUIRED(hot_lock_);

  // Returns the unique identifier of the last row less than or equal to the given time.
  int64_t FindStopTime(int64_t time, arrow::MemoryPool* mem_pool) const;

  // Returns the index into cold_row_ids_ or cold_time_ given the ring buffer location.
  int64_t RingVectorIndexUnlocked(int64_t ring_index) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(cold_lock_);
  // Returns the index into the ring buffer given a vector index into cold_row_ids_ or cold_time_.
  int64_t RingIndexUnlocked(int64_t vector_index) const ABSL_EXCLUSIVE_LOCKS_REQUIRED(cold_lock_);
  int64_t RingSizeUnlocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(cold_lock_);
  int64_t RingNextAddrUnlocked(int64_t ring_index) const ABSL_EXCLUSIVE_LOCKS_REQUIRED(cold_lock_);
  Status AdvanceRingBufferUnlocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(cold_lock_);

  Status UpdateSliceUnlocked(const BatchSlice& slice) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(generation_lock_);

  BatchSlice NextBatchWithoutStop(const BatchSlice& slice) const;
};

}  // namespace table_store
}  // namespace px
