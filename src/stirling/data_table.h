#pragma once

#include <memory>
#include <vector>

#include "src/common/statusor.h"
#include "src/stirling/data_table_schema.h"
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
  StatusOr<std::unique_ptr<ColumnWrapperRecordBatchVec>> GetRecordBatches();

  /**
   * @brief Get a pointer to the active record batch, for appending.
   *
   * @return Pointer to active record batch
   */
  ColumnWrapperRecordBatch* GetActiveRecordBatch() { return record_batch_.get(); }

  /**
   * @brief Return current occupancy of the Data Table.
   *
   * @return uint32_t occupancy
   */
  uint32_t Occupancy() { return current_row_; }

  /**
   * @brief Occupancy of the Data Table as a percentage of size.
   *
   * @return double percent occupancy
   */
  double OccupancyPct() { return 1.0 * current_row_ / target_capacity_; }

 protected:
  // Given an InfoClassSchema, generate the appropriate table. Helper for constructor.
  Status RegisterSchema(const InfoClassSchema& schema);

  // Initialize a new Active record batch.
  Status InitBuffers();

  // Close the active record batch, and call InitBuffers to set up new active record batch.
  Status SealActiveRecordBatch();

  // Table schema
  std::unique_ptr<DataTableSchema> table_schema_;

  // Pre-computed offsets into raw data buffers, according to schema.
  std::vector<uint32_t> offsets_;

  // Pre-computed row size, according to schema.
  uint32_t row_size_;

  // Current row in the table, where items will be appended.
  uint64_t current_row_;

  // Active record batch.
  std::unique_ptr<ColumnWrapperRecordBatch> record_batch_;

  // Sealed record batches that have been collected, but need to be pushed upstream.
  std::unique_ptr<ColumnWrapperRecordBatchVec> sealed_batches_;

  // ColumnWrapper specific members
  static constexpr uint64_t target_capacity_ = 1024;
};

}  // namespace stirling
}  // namespace pl
