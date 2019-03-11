#pragma once

#include <arrow/memory_pool.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/exec/table_store.h"
#include "src/carnot/udf/registry.h"
#include "src/common/common.h"

namespace pl {
namespace carnot {
namespace exec {

/**
 * ExecState manages the execution state for a single query. A new one will
 * be constructed for every query executed in Carnot and it will not be reused.
 *
 * The purpose of this class is to keep track of resources required for the query
 * and provide common resources (UDFs, UDA, etc) the operators within the query.
 */
class ExecState {
 public:
  explicit ExecState(udf::ScalarUDFRegistry* scalar_udf_registry, udf::UDARegistry* uda_registry,
                     std::shared_ptr<TableStore> table_store)
      : scalar_udf_registry_(scalar_udf_registry),
        uda_registry_(uda_registry),
        table_store_(std::move(table_store)) {}
  arrow::MemoryPool* exec_mem_pool() {
    // TOOD(zasgar): Make this the correct pool.
    return arrow::default_memory_pool();
  }

  udf::ScalarUDFRegistry* scalar_udf_registry() { return scalar_udf_registry_; }
  udf::UDARegistry* uda_registry() { return uda_registry_; }

  TableStore* table_store() { return table_store_.get(); }

 private:
  udf::ScalarUDFRegistry* scalar_udf_registry_;
  udf::UDARegistry* uda_registry_;
  std::shared_ptr<TableStore> table_store_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
