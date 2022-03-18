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

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "src/common/base/base.h"
#include "src/common/base/mixins.h"
#include "src/stirling/core/types.h"

namespace px {
namespace stirling {

/**
 * A tagged record batch is simply a record_batch that is tagged with a tablet_id.
 */
struct TaggedRecordBatch {
  types::TabletID tablet_id;
  types::ColumnWrapperRecordBatch records;
};

struct Tablet {
  types::TabletID tablet_id;
  // TODO(oazizi): Convert this vector into a heap of {time, index} objects.
  std::vector<uint64_t> times;
  types::ColumnWrapperRecordBatch records;
};

class DataTable : public NotCopyable {
 public:
  // Global unique ID identifies the table store to which this DataTable's data should be pushed.
  DataTable(uint64_t id, const DataTableSchema& schema);
  virtual ~DataTable() = default;

  /**
   * Consume the data buffered in the data table, up to the specified time.
   * Any records beyond the specified time will remain buffered in the table.
   *
   * Note that this function also internally tracks the largest timestamp pushed
   * across all previous calls to the function. Any records that have a timestamp
   * that would cause the appearance of records going backwards in time are dropped.
   * A warning message is printed in such cases.
   *
   * @return vector of Tablets (without tabletization, vector size is <=1).
   *         Empty record batches are not pushed into the vector, so all
   *         TaggedRecordBatch objects will have at least one record.
   */
  std::vector<TaggedRecordBatch> ConsumeRecords();

  /**
   * Sets a cutoff time for the table. Any records that appear after this time
   * will not be pushed out on a call to ConsumeRecords(). Instead, they will
   * remain buffered until the cutoff time is advanced.
   *
   * @param cutoff_time The time up to which records will be pushed out.
   */
  void SetConsumeRecordsCutoffTime(uint64_t cutoff_time) {
    if (cutoff_time_.has_value()) {
      DCHECK(cutoff_time >= cutoff_time_);
    }
    cutoff_time_ = cutoff_time;
  }

  /**
   * Return current occupancy of the Data Table.
   *
   * @return size_t occupancy
   */
  size_t Occupancy() const {
    size_t occupancy = 0;
    for (auto& [tablet_id, tablet] : tablets_) {
      if (!tablet.records.empty()) {
        occupancy += tablet.records[0]->Size();
      }
    }
    return occupancy;
  }

  /**
   * Occupancy of the Data Table as a percentage of size.
   *
   * @return double percent occupancy
   */
  double OccupancyPct() const { return 1.0 * Occupancy() / kTargetCapacity; }

  // Example usage:
  // DataTable::RecordBuilder<&kTable> r(data_table, time);
  // r.Append<r.ColIndex("field0")>(val0);
  // r.Append<r.ColIndex("field1")>(val1);
  // r.Append<r.ColIndex("field2")>(val2);
  //
  // NOTE: Today, the tabletization key must appear as replicated in a column.
  // This is technically redundant information, as the column will simply contain a constant value.
  // This is being done to keep Carnot simple. This way, Carnot does not to have to convert tablet
  // keys into dynamically generated columns. For example, if the tablet key was a PID,
  // the memory source from table store would have to add the PID when selecting multiple tablets,
  // if the tablet key was not an explicit column.
  // TODO(oazizi): Look into the optimization of avoiding replication.
  // See https://phab.pixielabs.ai/D1428 for an abandoned implementation.

  static constexpr char kTruncatedMsg[] = "... [TRUNCATED]";

  // RecordBuilder is used to build records into the DataTable.
  // It is to be preferred when the schema is known at compile-time, as it is more optimized.
  // If the schema is not known at compile-time, see DynamicRecordBuilder.
  template <const DataTableSchema* schema>
  class RecordBuilder {
   public:
    RecordBuilder(DataTable* data_table, types::TabletIDView tablet_id, uint64_t time = 0)
        : tablet_(*data_table->GetTablet(tablet_id)) {
      static_assert(schema->tabletized());
      tablet_id_ = tablet_id;
      Init(time);
    }

    explicit RecordBuilder(DataTable* data_table, uint64_t time = 0)
        : tablet_(*data_table->GetTablet("")) {
      static_assert(!schema->tabletized());
      Init(time);
    }

    // For convenience, a wrapper around ColIndex() in the DataTableSchema class.
    constexpr uint32_t ColIndex(std::string_view name) { return schema->ColIndex(name); }

    // The argument type is inferred by the table schema and the column index.
    // Strings larger than max_string_bytes size will be truncated before being appended.
    template <const size_t TIndex>
    void Append(typename types::DataTypeTraits<schema->elements()[TIndex].type()>::value_type val,
                const size_t max_string_bytes = 1024) {
      using TDataType =
          typename types::DataTypeTraits<schema->elements()[TIndex].type()>::value_type;

      if constexpr (TIndex == schema->tabletization_key()) {
        // This will break if val is ever StringValue (string tabletization keys are not supported).
        DCHECK(std::to_string(val.val) == tablet_id_);
      }

      if constexpr (std::is_same_v<TDataType, types::StringValue>) {
        if (val.size() > max_string_bytes) {
          val.resize(max_string_bytes);
          val.append(kTruncatedMsg);
        }
        val.shrink_to_fit();
      }

      tablet_.records[TIndex]->Append(std::move(val));
      DCHECK(!signature_[TIndex]) << absl::Substitute(
          "Attempt to Append() to column $0 (name=$1) multiple times", TIndex,
          schema->ColName(TIndex));
      signature_.set(TIndex);
    }

    ~RecordBuilder() {
      DCHECK(signature_.all()) << absl::Substitute(
          "Must call Append() on all columns. Table name = $0, Column unfilled = [$1]",
          schema->name(), absl::StrJoin(UnfilledColNames(), ","));
    }

    std::vector<std::string_view> UnfilledColNames() const {
      std::vector<std::string_view> res;
      for (size_t i = 0; i < signature_.size(); ++i) {
        if (!signature_.test(i)) {
          res.push_back(schema->ColName(i));
        }
      }
      return res;
    }

   private:
    void Init(uint64_t time) {
      DCHECK_EQ(schema->elements().size(), tablet_.records.size());
      tablet_.times.push_back(time);
    }

    Tablet& tablet_;
    std::bitset<schema->elements().size()> signature_;
    types::TabletIDView tablet_id_ = "";
  };

  // DynamicRecordBuilder is used to build records into the DataTable.
  // In contrast to RecordBuilder, it works even when the schema is not known at compile-time.
  // This, however, comes at a performance and style cost.
  // (e.g. easier to select columns by name with RecordBuilder).
  // If the schema is known at compile-time, please use RecordBuilder.
  // TODO(oazizi): Find a way to merge with RecordBuilder.
  class DynamicRecordBuilder {
   public:
    DynamicRecordBuilder(DataTable* data_table, types::TabletIDView tablet_id, uint64_t time = 0)
        : schema_(data_table->table_schema_), tablet_(*data_table->GetTablet(tablet_id)) {
      DCHECK(schema_.tabletized());
      tablet_id_ = tablet_id;
      Init(time);
    }

    explicit DynamicRecordBuilder(DataTable* data_table, uint64_t time = 0)
        : schema_(data_table->table_schema_), tablet_(*data_table->GetTablet("")) {
      DCHECK(!schema_.tabletized());
      Init(time);
    }

    // Any string larger than max_string_bytes size will be truncated.
    template <typename TValueType>
    void Append(size_t col_index, TValueType val, size_t max_string_bytes = 1024) {
      if constexpr (std::is_same_v<TValueType, types::StringValue>) {
        if (val.size() > max_string_bytes) {
          val.resize(max_string_bytes);
          val.append(kTruncatedMsg);
        }
      }

      tablet_.records[col_index]->Append(std::move(val));

      DCHECK(!signature_[col_index])
          << absl::Substitute("Attempt to Append() to column $0 (name=$1) multiple times",
                              col_index, schema_.ColName(col_index));
      signature_.set(col_index);
    }

    ~DynamicRecordBuilder() {
      // Check that every column was populated.
      DCHECK_EQ(signature_.count(), schema_.elements().size());
      DCHECK((signature_ >> schema_.elements().size()).none());
    }

   private:
    void Init(uint64_t time) {
      DCHECK_EQ(schema_.elements().size(), tablet_.records.size());
      tablet_.times.push_back(time);
      LOG_IF(DFATAL, schema_.elements().size() > kMaxSupportedColumns) << absl::Substitute(
          "Tables with more than $0 columns are not supported.", kMaxSupportedColumns);
    }

    static constexpr int kMaxSupportedColumns = 64;
    const DataTableSchema& schema_;
    std::bitset<kMaxSupportedColumns> signature_ = 0;
    Tablet& tablet_;
    types::TabletIDView tablet_id_ = "";
  };

  uint64_t id() const { return id_; }

 protected:
  // ColumnWrapper specific members
  static constexpr size_t kTargetCapacity = 1024;

  // Unique ID set by InfoClassManager.
  const uint64_t id_;

  // Initialize a new Active record batch.
  void InitBuffers(types::ColumnWrapperRecordBatch* record_batch_ptr);

  // Get a pointer to the Tablet, for appending. Used by RecordBuilder.
  Tablet* GetTablet(types::TabletIDView tablet_id);

  // Table schema: a DataElement to describe each column.
  const DataTableSchema& table_schema_;

  // Key is tablet id, value is tablet records.
  absl::flat_hash_map<types::TabletID, Tablet> tablets_;

  uint64_t start_time_ = 0;

  // The cutoff time is an optional field that sets up to which time
  // data source can guarantee that all events have been observed.
  // Used particularly by the socket tracer which receives asynchronous
  // events from BPF.
  std::optional<uint64_t> cutoff_time_;
};

}  // namespace stirling
}  // namespace px
