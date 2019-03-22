#pragma once

#include <arrow/memory_pool.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/compiler/compiler.h"
#include "src/carnot/schema/schema.h"
#include "src/carnot/schema/table.h"
#include "src/common/common.h"

namespace pl {
namespace carnot {

struct CarnotQueryResult {
  CarnotQueryResult() = default;
  explicit CarnotQueryResult(std::vector<schema::Table*> output_tables, int64_t rows_processed,
                             int64_t bytes_processed, int64_t compile_time_ns, int64_t exec_time_ns)
      : output_tables_(std::move(output_tables)),
        rows_processed(rows_processed),
        bytes_processed(bytes_processed),
        compile_time_ns(compile_time_ns),
        exec_time_ns(exec_time_ns) {}
  size_t NumTables() const { return output_tables_.size(); }
  schema::Table* GetTable(int64_t i) const { return output_tables_[i]; }
  StatusOr<std::vector<schema::RecordBatchSPtr>> GetTableAsRecordBatches(int64_t i) const {
    return GetTable(i)->GetTableAsRecordBatches();
  }
  std::vector<schema::Table*> output_tables_;
  int64_t rows_processed = 0;
  int64_t bytes_processed = 0;
  int64_t compile_time_ns = 0;
  int64_t exec_time_ns = 0;
};

class Carnot : public NotCopyable {
 public:
  static StatusOr<std::unique_ptr<Carnot>> Create();
  virtual ~Carnot() = default;

  /**
   * Adds a table by name (id generated automatically).
   * @param table_name The name of the table.
   * @param table The status.
   */
  virtual void AddTable(const std::string& table_name, std::shared_ptr<schema::Table> table) = 0;

  /**
   * Adds a table by id and name.
   * @param table_name The name of the table.
   * @param table_id The ID of the table.
   * @param table The table.
   * @return Status of adding the table.
   */
  virtual Status AddTable(const std::string& table_name, uint64_t table_id,
                          std::shared_ptr<schema::Table> table) = 0;

  /**
   * Gets a table by name.
   * @param table_name
   * @return a pointer to a table (ownership is not transferred).
   */
  virtual schema::Table* GetTable(const std::string& table_name) = 0;

  /**
   * Executes the given query.
   *
   * @param query the query in the form of a string.
   * @return a Carnot Return with output_tables if successful. Error status otherwise.
   */
  virtual StatusOr<CarnotQueryResult> ExecuteQuery(const std::string& query,
                                                   types::Time64NSValue time_now) = 0;
};

}  // namespace carnot
}  // namespace pl
