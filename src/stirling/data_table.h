#pragma once

#include <algorithm>
#include <limits>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "src/common/base/base.h"
#include "src/common/base/mixins.h"
#include "src/stirling/types.h"

namespace pl {
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
  // TODO(oazizi): Convert his vector into a heap of {time, index} objects.
  std::vector<uint64_t> times;
  types::ColumnWrapperRecordBatch records;
};

class DataTable : public NotCopyable {
 public:
  explicit DataTable(const DataTableSchema& schema);
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
   * @param end_time Threshold time up until which records are pushed out.
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
      PL_UNUSED(tablet_id);
      occupancy += tablet.records[0]->Size();
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

  template <const DataTableSchema* schema>
  class RecordBuilder {
   public:
    RecordBuilder(DataTable* data_table, types::TabletIDView tablet_id, uint64_t time = 0)
        : tablet_(*data_table->GetTablet(tablet_id)), time_(time) {
      static_assert(schema->tabletized());
      DCHECK_EQ(schema->elements().size(), tablet_.records.size());
      tablet_id_ = tablet_id;
      tablet_.times.push_back(time);
    }

    explicit RecordBuilder(DataTable* data_table, uint64_t time = 0)
        : tablet_(*data_table->GetTablet("")), time_(time) {
      static_assert(!schema->tabletized());
      DCHECK_EQ(schema->elements().size(), tablet_.records.size());
      tablet_.times.push_back(time);
    }

    // For convenience, a wrapper around ColIndex() in the DataTableSchema class.
    constexpr uint32_t ColIndex(std::string_view name) { return schema->ColIndex(name); }

    // The argument type is inferred by the table schema and the column index.
    // Any string larger than TMaxStringBytes size will be truncated before being placed in the
    // record.
    template <const size_t TIndex, const size_t TMaxStringBytes = 1024>
    inline void Append(
        typename types::DataTypeTraits<schema->elements()[TIndex].type()>::value_type val) {
      if constexpr (TIndex == schema->tabletization_key()) {
        // TODO(oazizi): This will probably break if val is ever StringValue.
        DCHECK(std::to_string(val.val) == tablet_id_);
      }

      if constexpr (std::is_same_v<typename types::DataTypeTraits<
                                       schema->elements()[TIndex].type()>::value_type,
                                   types::StringValue>) {
        if (val.size() > TMaxStringBytes) {
          val.resize(TMaxStringBytes);
          val.append(kTruncatedMsg);
        }
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
    Tablet& tablet_;
    std::bitset<schema->elements().size()> signature_;
    types::TabletIDView tablet_id_ = "";
    uint64_t time_;

    static constexpr char kTruncatedMsg[] = "... [TRUNCATED]";
  };

 protected:
  // ColumnWrapper specific members
  static constexpr size_t kTargetCapacity = 1024;

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
}  // namespace pl
