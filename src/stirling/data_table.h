#pragma once

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

class DataTable : public NotCopyable {
 public:
  explicit DataTable(const DataTableSchema& schema);
  virtual ~DataTable() = default;

  /**
   * @brief Get the data collected so far and relinquish ownership.
   *
   * @return pointer to a vector of ColumnWrapperRecordBatch pointers.
   */
  std::vector<TaggedRecordBatch> ConsumeRecordBatches();

  /**
   * @brief Return current occupancy of the Data Table.
   *
   * @return size_t occupancy
   */
  size_t Occupancy() const {
    size_t occupancy = 0;
    for (auto& [tablet_id, tablet] : tablets_) {
      PL_UNUSED(tablet_id);
      occupancy += tablet[0]->Size();
    }
    return occupancy;
  }

  /**
   * @brief Occupancy of the Data Table as a percentage of size.
   *
   * @return double percent occupancy
   */
  double OccupancyPct() const { return 1.0 * Occupancy() / kTargetCapacity; }

  // Example usage:
  // DataTable::RecordBuilder<&kTable> r(data_table);
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
    // Any string larger than this size will be truncated before being placed in the record.
    static constexpr int kMaxStringBytes = 512;
    static constexpr char kTruncatedMsg[] = "... [TRUNCATED]";

    explicit RecordBuilder(DataTable* data_table, types::TabletIDView tablet_id)
        : RecordBuilder(data_table->ActiveRecordBatch(tablet_id)) {
      static_assert(schema->tabletized());
      tablet_id_ = tablet_id;
    }

    explicit RecordBuilder(DataTable* data_table) : RecordBuilder(data_table->ActiveRecordBatch()) {
      static_assert(!schema->tabletized());
    }

    // For convenience, a wrapper around ColIndex() in the DataTableSchema class.
    constexpr uint32_t ColIndex(std::string_view name) { return schema->ColIndex(name); }

    // The argument type is inferred by the table schema and the column index.
    template <const uint32_t index>
    inline void Append(
        typename types::DataTypeTraits<schema->elements()[index].type()>::value_type val) {
      if constexpr (index == schema->tabletization_key()) {
        // TODO(oazizi): This will probably break if val is ever StringValue.
        DCHECK(std::to_string(val.val) == tablet_id_);
      }

      if constexpr (std::is_same_v<typename types::DataTypeTraits<
                                       schema->elements()[index].type()>::value_type,
                                   types::StringValue>) {
        if (val.size() > kMaxStringBytes) {
          val.resize(kMaxStringBytes);
          val.append(kTruncatedMsg);
        }
      }

      record_batch_[index]->Append(std::move(val));
      DCHECK(!signature_[index]) << absl::Substitute(
          "Attempt to Append() to column $0 (name=$1) multiple times", index,
          schema->ColName(index));
      signature_.set(index);
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
    explicit RecordBuilder(types::ColumnWrapperRecordBatch* active_record_batch)
        : record_batch_(*active_record_batch) {
      DCHECK_EQ(schema->elements().size(), active_record_batch->size());
    }

    types::ColumnWrapperRecordBatch& record_batch_;
    std::bitset<schema->elements().size()> signature_;
    types::TabletIDView tablet_id_ = "";
  };

 protected:
  // ColumnWrapper specific members
  static constexpr size_t kTargetCapacity = 1024;

  // Initialize a new Active record batch.
  void InitBuffers(types::ColumnWrapperRecordBatch* record_batch_ptr);

  // Get a pointer to the active record batch, for appending.
  // Used by RecordBuilder.
  types::ColumnWrapperRecordBatch* ActiveRecordBatch(types::TabletIDView tablet_id = "");

  // Table schema: a DataElement to describe each column.
  const DataTableSchema& table_schema_;

  // Active record batch.
  // Key is tablet id, value is tablet active record batch.
  absl::flat_hash_map<types::TabletID, types::ColumnWrapperRecordBatch> tablets_;
};

}  // namespace stirling
}  // namespace pl
