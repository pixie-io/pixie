#pragma once

#include <memory>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/info_class_manager.h"
#include "src/stirling/types.h"

namespace pl {
namespace stirling {

class DataTable {
 public:
  DataTable() = delete;
  virtual ~DataTable() = default;
  explicit DataTable(const InfoClassSchema& schema);

  /**
   * @brief Get the data collected so far and relinquish ownership.
   *
   * @return pointer to a vector of ColumnWrapperRecordBatch pointers.
   */
  std::unique_ptr<types::ColumnWrapperRecordBatchVec> ConsumeRecordBatches();

  /**
   * @brief Get a pointer to the active record batch, for appending.
   *
   * Note that while this function is const, because it doesn't change the DataTable members
   * directly, the pointer that is returned is meant for appending, and so logically the contents of
   * DataTable can (and likely will) change.
   *
   * @return Pointer to active record batch.
   */
  types::ColumnWrapperRecordBatch* ActiveRecordBatch() const { return record_batch_.get(); }

  /**
   * @brief Return current occupancy of the Data Table.
   *
   * @return size_t occupancy
   */
  size_t Occupancy() const { return record_batch_->at(0)->Size(); }

  /**
   * @brief Occupancy of the Data Table as a percentage of size.
   *
   * @return double percent occupancy
   */
  double OccupancyPct() const { return 1.0 * Occupancy() / kTargetCapacity; }

 protected:
  // Initialize a new Active record batch.
  void InitBuffers();

  // Close the active record batch, and call InitBuffers to set up new active record batch.
  void SealActiveRecordBatch();

  // Table schema
  std::vector<DataElement> table_schema_;

  // Active record batch.
  std::unique_ptr<types::ColumnWrapperRecordBatch> record_batch_;

  // Sealed record batches that have been collected, but need to be pushed upstream.
  std::unique_ptr<types::ColumnWrapperRecordBatchVec> sealed_batches_;

  // ColumnWrapper specific members
  static constexpr size_t kTargetCapacity = 1024;
};

}  // namespace stirling
}  // namespace pl
