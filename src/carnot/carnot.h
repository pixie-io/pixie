#pragma once

#include <arrow/memory_pool.h>
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/compiler/compiler.h"
#include "src/carnot/engine_state.h"
#include "src/carnot/exec/exec_graph.h"
#include "src/carnot/exec/table.h"
#include "src/carnot/exec/table_store.h"
#include "src/carnot/plan/plan.h"
#include "src/carnot/plan/schema.h"
#include "src/carnot/udf/registry.h"
#include "src/common/common.h"

namespace pl {
namespace carnot {
struct CarnotQueryResult {
  CarnotQueryResult() {}
  explicit CarnotQueryResult(std::vector<exec::Table*> output_tables, int64_t rows_processed,
                             int64_t bytes_processed, int64_t compile_time_ns, int64_t exec_time_ns)
      : output_tables_(output_tables),
        rows_processed(rows_processed),
        bytes_processed(bytes_processed),
        compile_time_ns(compile_time_ns),
        exec_time_ns(exec_time_ns) {}
  size_t NumTables() const { return output_tables_.size(); }
  exec::Table* GetTable(int64_t i) const { return output_tables_[i]; }
  StatusOr<std::vector<exec::RecordBatchSPtr>> GetTableAsRecordBatches(int64_t i) const {
    return GetTable(i)->GetTableAsRecordBatches();
  }
  std::vector<exec::Table*> output_tables_;
  int64_t rows_processed = 0;
  int64_t bytes_processed = 0;
  int64_t compile_time_ns = 0;
  int64_t exec_time_ns = 0;
};
class Carnot : public NotCopyable {
 public:
  /**
   * Initializes the engine with the state necessary to compile and execute a query.
   * This includes the tables and udf registries.
   * @return a status of whether initialization was successful.
   */
  Status Init();

  /**
   * Returns the Table Store.
   * This method is for testing purposes.
   */
  exec::TableStore* table_store() { return engine_state_->table_store(); }

  /**
   * Executes the given query.
   *
   * @param query the query in the form of a string.
   * @return a Carnot Return with output_tables if successful. Error status otherwise.
   */
  StatusOr<CarnotQueryResult> ExecuteQuery(const std::string& query);

  bool is_initialized() { return is_initialized_; }

 private:
  compiler::Compiler compiler_;
  std::unique_ptr<EngineState> engine_state_;
  bool is_initialized_ = false;
};

}  // namespace carnot
}  // namespace pl
