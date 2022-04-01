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

#include <deque>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "src/shared/types/types.h"
#include "src/table_store/schema/relation.h"
#include "src/table_store/schema/row_batch.h"
#include "src/table_store/table/internal/record_or_row_batch.h"
#include "src/table_store/table/internal/types.h"

namespace px {
namespace table_store {
namespace internal {

/**
 * BatchSizeAccountantNonMutableState is an optimization to allow calculating BatchStats without
 * holding the lock that synchronizes BatchSizeAccountant. Since all fields in the non-mutable
 * state, are only set at the time of creating the accountant, and then never modified, we can
 * access these fields without holding the lock that synchronizes BatchSizeAccountant (without
 * breaking abseil's lock synchronization macros, eg. ABSL_GUARDED_BY. Calling
 * `batch_size_accountant.NonMutableState()` is always thread safe (because its guaranteed to not be
 * mutated). So callers can wrap it in `ABSL_TS_UNCHECKED_READ(...)` to avoid warnings related to
 * ABSL_GUARDED_BY, etc.
 */
struct BatchSizeAccountantNonMutableState {
  uint64_t compacted_size;
  uint32_t num_cols;
  uint64_t per_row_fixed_size;
  std::vector<int64_t> variable_cols_indices;
};

class BatchSizeAccountant {
 public:
  struct BatchStats {
    uint64_t num_rows = 0;
    uint64_t bytes = 0;
    std::unordered_map<int64_t, std::vector<uint64_t>> variable_cols_row_bytes;
  };

  struct CompactedBatchSpec {
    uint64_t num_rows;
    uint64_t bytes;
    std::vector<uint64_t> variable_col_bytes;
    struct HotSlice {
      uint32_t start_row = 0;
      uint32_t end_row = 0;
      // If true, this is the last slice that uses the current hot batch.
      bool last_slice_for_batch = false;

     private:
      // These fields are made private since they are only used internally to BatchSizeAccountant,
      // in order to maintain the fields of the CompactedBatchSpec, when hot slices are expired.
      uint64_t bytes = 0;
      std::vector<uint64_t> variable_col_bytes;
      friend class BatchSizeAccountant;
    };
    std::deque<HotSlice> hot_slices;
  };

  static std::unique_ptr<BatchSizeAccountant> Create(const schema::Relation& rel,
                                                     size_t compacted_size);
  /**
   * CalcBatchStats returns an internal data structure that should be used for a call to
   * NewHotBatch. It is separated out from `NewHotBatch` as a static function, so that it can be
   * called without a lock held, and then later once the lock is held NewHotBatch can be called on
   * the result.
   * @param non_mutable_state, BatchSizeAccountant internal state, should be used directly from
   * batch_size_accountant.NonMutableState(). Passing this as a static function allows the
   * optimization described above of not calculating batch statistics whilst the lock is held.
   * @param batch the RecordOrRowBatch to calculate the stats for.
   * @return the sizing stats of that RecordOrRowBatch, useful to get the number of bytes in that
   * RecordOrRowBatch or to pass to `NewHotBatch`
   */
  static BatchStats CalcBatchStats(const BatchSizeAccountantNonMutableState& non_mutable_state,
                                   const RecordOrRowBatch& batch);
  /**
   * Notify BatchSizeAccountant of a new batch being written to the hot store, with the given
   * BatchStats. For synchronization reasons, this function accepts a `BatchStats` instead of a
   * `RecordOrRowBatch`.
   * @param batch_stats output of BatchSizeAccountant::BatchStats(batch) called on the new hot
   * batch.
   */
  void NewHotBatch(const BatchStats& batch_stats);
  /**
   * ExpireHotBatch notifies the BatchSizeAccountant that a hot batch is being expired, and so it
   * should update its accounting accordingly.
   */
  void ExpireHotBatch();
  /**
   * ExpireColdBatch notifies the BatchSizeAccountant that a cold batch is being expired, and so it
   * should update its accounting accordingly.
   */
  void ExpireColdBatch();
  /**
   * CompactedBatchReady returns whether there is enough data in the hot store to create a full
   * compacted batch.
   */
  bool CompactedBatchReady() const;
  /**
   * GetNextCompactedBatchSpec gets the spec for the next batch that should be placed in the cold
   * store, specified in terms of slices of batches in the current hot store.
   * @return CompactedBatchSpec corresponding to the next batch to be compacted.
   */
  const CompactedBatchSpec& GetNextCompactedBatchSpec() const;
  /**
   * FinishCompactedBatch notifies the BatchSizeAccountant that the compacted batch from
   * `GetNextCompactedBatchSpec` has been moved to the cold store, and BatchSizeAccountant should
   * update hot_bytes_ and cold_bytes_ accordingly. It returns the number of rows that need to be
   * removed from start of the first hot batch in order to prevent duplicated data between the hot
   * and cold stores.
   * @return Number of rows to remove from the front of the hot store, since those rows were moved
   * into the cold store via CompactedBatchSpec.
   */
  uint64_t FinishCompactedBatch();
  /**
   * @return the number of bytes stored in the hot store.
   */
  uint64_t HotBytes() const;
  /**
   * @return the number of bytes stored in the cold store.
   */
  uint64_t ColdBytes() const;

  const BatchSizeAccountantNonMutableState& NonMutableState() const;

 private:
  const BatchSizeAccountantNonMutableState non_mutable_state_;

  std::deque<CompactedBatchSpec> compacted_batch_specs_;
  std::deque<uint64_t> cold_batch_bytes_;
  uint64_t hot_bytes_ = 0;
  uint64_t cold_bytes_ = 0;

  static BatchSizeAccountantNonMutableState CreateNonMutableState(const schema::Relation& rel,
                                                                  size_t compacted_size);
  explicit BatchSizeAccountant(BatchSizeAccountantNonMutableState state);
  CompactedBatchSpec* NewCompactedBatchSpec();
};

}  // namespace internal
}  // namespace table_store
}  // namespace px
