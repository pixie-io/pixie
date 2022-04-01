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

#include <arrow/array.h>

#include <deque>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/table_store/schema/row_batch.h"

namespace px {
namespace table_store {
namespace internal {

using RecordBatchPtr = std::unique_ptr<px::types::ColumnWrapperRecordBatch>;
using ArrowArrayPtr = std::shared_ptr<arrow::Array>;
using Time = int64_t;
using TimeInterval = std::pair<Time, Time>;
using RowID = int64_t;
using RowIDInterval = std::pair<RowID, RowID>;
using BatchID = int64_t;

struct RecordBatchWithCache {
  RecordBatchPtr record_batch;
  // Whenever we have to convert a hot batch to an arrow array, we store the arrow array in
  // this cache. Compaction will eventually take these arrow arrays and move them into cold.
  mutable std::vector<ArrowArrayPtr> arrow_cache;
  mutable std::vector<bool> cache_validity;
};

enum StoreType {
  Hot,
  Cold,
};

struct BatchHints {
  StoreType hint_type;
  BatchID batch_id;
};

class RecordOrRowBatch;

using ColdBatch = std::vector<ArrowArrayPtr>;

template <StoreType type>
struct StoreTypeTraits {};
template <>
struct StoreTypeTraits<StoreType::Hot> {
  using batch_type = RecordOrRowBatch;
};
template <>
struct StoreTypeTraits<StoreType::Cold> {
  using batch_type = ColdBatch;
};

}  // namespace internal
}  // namespace table_store
}  // namespace px
