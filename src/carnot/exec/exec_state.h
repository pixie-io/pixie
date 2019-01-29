#pragma once

#include <arrow/memory_pool.h>
#include <glog/logging.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/udf/registry.h"
#include "src/utils/status.h"

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
  explicit ExecState(std::shared_ptr<udf::ScalarUDFRegistry> scalar_udf_registry)
      : scalar_udf_registry_(std::move(scalar_udf_registry)) {}
  arrow::MemoryPool* exec_mem_pool() {
    // TOOD(zasgar): Make this the correct pool.
    return arrow::default_memory_pool();
  }

  udf::ScalarUDFRegistry* scalar_udf_registry() { return scalar_udf_registry_.get(); }

 private:
  std::shared_ptr<udf::ScalarUDFRegistry> scalar_udf_registry_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
