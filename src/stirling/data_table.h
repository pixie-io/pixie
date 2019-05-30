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
  StatusOr<std::unique_ptr<types::ColumnWrapperRecordBatchVec>> GetRecordBatches();

  /**
   * @brief Get a pointer to the active record batch, for appending.
   *
   * @return Pointer to active record batch
   */
  types::ColumnWrapperRecordBatch* GetActiveRecordBatch() { return record_batch_.get(); }

  /**
   * @brief Return current occupancy of the Data Table.
   *
   * @return size_t occupancy
   */
  size_t Occupancy() { return record_batch_->at(0)->Size(); }

  /**
   * @brief Occupancy of the Data Table as a percentage of size.
   *
   * @return double percent occupancy
   */
  double OccupancyPct() { return 1.0 * Occupancy() / target_capacity_; }

 protected:
  // Initialize a new Active record batch.
  Status InitBuffers();

  // Close the active record batch, and call InitBuffers to set up new active record batch.
  Status SealActiveRecordBatch();

  // Table schema
  std::unique_ptr<std::vector<DataElement>> table_schema_;

  // Pre-computed row size, according to schema.
  uint32_t row_size_;

  // Active record batch.
  std::unique_ptr<types::ColumnWrapperRecordBatch> record_batch_;

  // Sealed record batches that have been collected, but need to be pushed upstream.
  std::unique_ptr<types::ColumnWrapperRecordBatchVec> sealed_batches_;

  // ColumnWrapper specific members
  static constexpr uint64_t target_capacity_ = 1024;
};

}  // namespace stirling
}  // namespace pl
