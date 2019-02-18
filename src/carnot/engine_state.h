#pragma once

#include <arrow/memory_pool.h>
#include <glog/logging.h>
#include <memory>

#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/exec/table_store.h"
#include "src/carnot/plan/plan_state.h"
#include "src/carnot/plan/schema.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base.h"
#include "src/common/status.h"
#include "udf/registry.h"

namespace pl {
namespace carnot {

/**
 * EngineState manages the state required to compile and execute a query.
 *
 * The purpose of this class is to keep track of resources required for the query
 * and provide common resources (UDFs, UDA, etc) the operators within the query.
 */
class EngineState : public NotCopyable {
 public:
  EngineState(udf::ScalarUDFRegistry* udf_registry, udf::UDARegistry* uda_registry,
              std::shared_ptr<exec::TableStore> table_store, std::shared_ptr<plan::Schema> schema,
              compiler::RegistryInfo* registry_info)
      : uda_registry_(uda_registry),
        scalar_udf_registry_(udf_registry),
        table_store_(table_store),
        schema_(schema),
        registry_info_(registry_info) {}

  static StatusOr<std::unique_ptr<EngineState>> CreateDefault() {
    // Initialize state.
    auto scalar_udf_registry = udf::ScalarUDFRegistry("udf_registry");
    auto uda_registry = udf::UDARegistry("uda_registry");
    builtins::RegisterBuiltinsOrDie(&scalar_udf_registry);
    builtins::RegisterBuiltinsOrDie(&uda_registry);

    auto table_store = std::make_shared<exec::TableStore>();
    auto schema = std::make_shared<plan::Schema>();

    auto registry_info = compiler::RegistryInfo();
    auto udf_info =
        udf::RegistryInfoExporter().Registry(uda_registry).Registry(scalar_udf_registry).ToProto();
    PL_RETURN_IF_ERROR(registry_info.Init(udf_info));
    return std::make_unique<EngineState>(&scalar_udf_registry, &uda_registry, table_store, schema,
                                         &registry_info);
  }

  std::shared_ptr<plan::Schema> schema() { return schema_; }

  std::unique_ptr<exec::ExecState> CreateExecState() {
    return std::make_unique<exec::ExecState>(scalar_udf_registry_, uda_registry_, table_store_);
  }

  std::unique_ptr<plan::PlanState> CreatePlanState() {
    return std::make_unique<plan::PlanState>(scalar_udf_registry_, uda_registry_);
  }

  std::unique_ptr<compiler::CompilerState> CreateCompilerState() {
    return std::make_unique<compiler::CompilerState>(schema_, registry_info_);
  }

 private:
  udf::UDARegistry* uda_registry_;
  udf::ScalarUDFRegistry* scalar_udf_registry_;
  std::shared_ptr<exec::TableStore> table_store_;
  std::shared_ptr<plan::Schema> schema_;
  compiler::RegistryInfo* registry_info_;
};

}  // namespace carnot
}  // namespace pl
