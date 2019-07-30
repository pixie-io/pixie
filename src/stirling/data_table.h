#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/info_class_manager.h"
#include "src/stirling/types.h"

namespace pl {
namespace stirling {

struct TaggedRecordBatch {
  size_t tablet_id;
  std::unique_ptr<types::ColumnWrapperRecordBatch> records_uptr;
};

class DataTable {
 public:
  DataTable() = delete;
  virtual ~DataTable() = default;
  explicit DataTable(const InfoClassSchema& schema);
  explicit DataTable(const DataTableSchema& schema);

  /**
   * @brief Get the data collected so far and relinquish ownership.
   *
   * @return pointer to a vector of ColumnWrapperRecordBatch pointers.
   */
  std::vector<TaggedRecordBatch> ConsumeRecordBatches();

  /**
   * @brief Get a pointer to the active record batch, for appending.
   *
   * Note that while this function is const, because it doesn't change the DataTable members
   * directly, the pointer that is returned is meant for appending, and so logically the contents of
   * DataTable can (and likely will) change.
   *
   * @return Pointer to active record batch.
   */
  types::ColumnWrapperRecordBatch* ActiveRecordBatch(size_t tablet_id = 0);

  /**
   * @brief Return current occupancy of the Data Table.
   *
   * @return size_t occupancy
   */
  size_t Occupancy() const {
    size_t occupancy = 0;
    for (auto& [tablet_id, tablet] : tablets_) {
      PL_UNUSED(tablet_id);
      DCHECK(tablet != nullptr);
      occupancy += tablet->at(0)->Size();
    }
    return occupancy;
  }

  /**
   * @brief Occupancy of the Data Table as a percentage of size.
   *
   * @return double percent occupancy
   */
  double OccupancyPct() const { return 1.0 * Occupancy() / kTargetCapacity; }

 protected:
  // Initialize a new Active record batch.
  void InitBuffers(types::ColumnWrapperRecordBatch* record_batch_ptr);

  // Close the active record batch, and call InitBuffers to set up new active record batch.
  void SealActiveRecordBatch();

  // Table schema: a DataElement to describe each column.
  std::vector<DataElement> table_schema_;

  // Active record batch.
  // Key is tablet id, value is tablet active record batch.
  std::unordered_map<size_t, std::unique_ptr<types::ColumnWrapperRecordBatch>> tablets_;

  // Sealed record batches that have been collected, but need to be pushed upstream.
  std::vector<TaggedRecordBatch> sealed_batches_;

  // ColumnWrapper specific members
  static constexpr size_t kTargetCapacity = 1024;
};

}  // namespace stirling
}  // namespace pl
