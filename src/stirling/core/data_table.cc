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

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/shared/types/type_utils.h"
#include "src/stirling/core/data_table.h"
#include "src/stirling/core/types.h"
#include "src/stirling/utils/index_sorted_vector.h"

namespace px {
namespace stirling {

using types::ColumnWrapper;
using types::DataType;

DataTable::DataTable(uint64_t id, const DataTableSchema& schema) : id_(id), table_schema_(schema) {}

void DataTable::InitBuffers(types::ColumnWrapperRecordBatch* record_batch_ptr) {
  DCHECK(record_batch_ptr != nullptr);
  DCHECK(record_batch_ptr->empty());

  for (const auto& element : table_schema_.elements()) {
    px::types::DataType type = element.type();

#define TYPE_CASE(_dt_)                           \
  auto col = types::ColumnWrapper::Make(_dt_, 0); \
  col->Reserve(kTargetCapacity);                  \
  record_batch_ptr->push_back(col);
    PX_SWITCH_FOREACH_DATATYPE(type, TYPE_CASE);
#undef TYPE_CASE
  }
}

Tablet* DataTable::GetTablet(types::TabletIDView tablet_id) {
  auto& tablet = tablets_[tablet_id];
  if (tablet.records.empty()) {
    InitBuffers(&tablet.records);
  }
  return &tablet;
}

std::vector<TaggedRecordBatch> DataTable::ConsumeRecords() {
  std::vector<TaggedRecordBatch> tablets_out;
  absl::flat_hash_map<types::TabletID, Tablet> carryover_tablets;
  uint64_t next_start_time = start_time_;

  for (auto& [tablet_id, tablet] : tablets_) {
    // Sort based on times.
    std::vector<size_t> sort_indexes = utils::SortedIndexes(tablet.times);

    // End time is cutoff time + 1, so call to SplitSortedVector() produces the following
    // classification: which classified according to:
    //   expired < start_time
    //   pushable <= end_time
    uint64_t end_time = cutoff_time_.has_value() ? (cutoff_time_.value() + 1)
                                                 : std::numeric_limits<uint64_t>::max();

    // Split the indexes into three groups:
    // 1) Expired indexes: these are too old to return.
    // 2) Pushable indexes: these are the ones that we return.
    // 3) Carryover indexes: these are too new to return, so hold on to them until the next round.
    auto positions =
        utils::SplitSortedVector<2>(tablet.times, sort_indexes, {start_time_, end_time});
    int num_expired = positions[0];
    int num_pushable = positions[1] - positions[0];
    int num_carryover = tablet.times.size() - positions[1];

    // Case 1: Expired records. Just print a message.
    VLOG_IF(1, num_expired > 0) << absl::Substitute(
        "$0 records for table $1 dropped due to late arrival [cutoff time=$2, oldest event "
        "time=$3].",
        num_expired, table_schema_.name(), end_time, tablet.times[sort_indexes[0]]);

    // Case 2: Pushable records. Copy to output.
    if (num_pushable > 0) {
      // TODO(oazizi): Consider VectorView to avoid copying.
      std::vector<size_t> push_indexes(sort_indexes.begin() + num_expired,
                                       sort_indexes.end() - num_carryover);
      types::ColumnWrapperRecordBatch pushable_records;
      for (auto& col : tablet.records) {
        pushable_records.push_back(col->MoveIndexes(push_indexes));
      }
      uint64_t last_time = tablet.times[push_indexes.back()];
      next_start_time = std::max(next_start_time, last_time);
      tablets_out.push_back(TaggedRecordBatch{tablet_id, std::move(pushable_records)});
    }

    // Case 3: Carryover records.
    if (num_carryover > 0) {
      // TODO(oazizi): Consider VectorView to avoid copying.
      std::vector<size_t> carryover_indexes(sort_indexes.begin() + num_pushable,
                                            sort_indexes.end());
      types::ColumnWrapperRecordBatch carryover_records;
      for (auto& col : tablet.records) {
        carryover_records.push_back(col->MoveIndexes(carryover_indexes));
      }

      std::vector<uint64_t> times(carryover_indexes.size());
      for (size_t i = 0; i < times.size(); ++i) {
        times[i] = tablet.times[carryover_indexes[i]];
      }
      carryover_tablets[tablet_id] =
          Tablet{tablet_id, std::move(times), std::move(carryover_records)};
    }
  }
  tablets_ = std::move(carryover_tablets);

  start_time_ = next_start_time;

  return tablets_out;
}

}  // namespace stirling
}  // namespace px
