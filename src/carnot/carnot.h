#pragma once

#include <arrow/memory_pool.h>
#include <glog/logging.h>
#include <memory>
#include <string>

#include "src/carnot/compiler/compiler.h"
#include "src/carnot/engine_state.h"
#include "src/carnot/exec/exec_graph.h"
#include "src/carnot/exec/table.h"
#include "src/carnot/exec/table_store.h"
#include "src/carnot/plan/plan.h"
#include "src/carnot/plan/schema.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base.h"
#include "src/common/status.h"

namespace pl {
namespace carnot {

class Carnot : public NotCopyable {
 public:
  /**
   * Initializes the engine with the state necessary to compile and execute a query.
   * This includes the tables and udf registries.
   * @return a status of whether initialization was successful.
   */
  Status Init();

  /**
   * Adds a table to the table store in the engine state.
   * This method is for testing purposes.
   *
   * @param table_name the name of the table to add.
   * @param table the table to add.
   */
  void AddTable(const std::string& table_name, std::shared_ptr<exec::Table> table);

  exec::Table* GetTable(const std::string& table_name);

  /**
   * Executes the given query.
   *
   * @param query the query in the form of a string.
   * @return a status of whether or not the query was successful.
   */
  Status ExecuteQuery(const std::string& query);

  bool is_initialized() { return is_initialized_; }

 private:
  compiler::Compiler compiler_;
  std::unique_ptr<EngineState> engine_state_;
  bool is_initialized_ = false;
};

}  // namespace carnot
}  // namespace pl
