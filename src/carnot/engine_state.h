#pragma once

#include <arrow/memory_pool.h>
#include <memory>
#include <utility>

#include "src/carnot/builtins/builtins.h"
#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/exec/table_store.h"
#include "src/carnot/plan/plan_state.h"
#include "src/carnot/schema/schema.h"
#include "src/carnot/udf/registry.h"
#include "src/common/common.h"

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
  EngineState(std::unique_ptr<udf::ScalarUDFRegistry> udf_registry,
              std::unique_ptr<udf::UDARegistry> uda_registry,
              std::shared_ptr<exec::TableStore> table_store, std::shared_ptr<schema::Schema> schema,
              std::unique_ptr<compiler::RegistryInfo> registry_info)
      : uda_registry_(std::move(uda_registry)),
        scalar_udf_registry_(std::move(udf_registry)),
        table_store_(std::move(table_store)),
        schema_(std::move(schema)),
        registry_info_(std::move(registry_info)) {}

  static StatusOr<std::unique_ptr<EngineState>> CreateDefault() {
    // Initialize state.
    auto scalar_udf_registry = std::make_unique<udf::ScalarUDFRegistry>("udf_registry");
    auto uda_registry = std::make_unique<udf::UDARegistry>("uda_registry");
    builtins::RegisterBuiltinsOrDie(scalar_udf_registry.get());
    builtins::RegisterBuiltinsOrDie(uda_registry.get());

    auto table_store = std::make_shared<exec::TableStore>();
    auto schema = std::make_shared<schema::Schema>();

    auto registry_info = std::make_unique<compiler::RegistryInfo>();
    auto udf_info = udf::RegistryInfoExporter()
                        .Registry(*uda_registry)
                        .Registry(*scalar_udf_registry)
                        .ToProto();
    PL_RETURN_IF_ERROR(registry_info->Init(udf_info));

    return std::make_unique<EngineState>(std::move(scalar_udf_registry), std::move(uda_registry),
                                         table_store, schema, std::move(registry_info));
  }

  std::shared_ptr<schema::Schema> schema() { return schema_; }

  exec::TableStore* table_store() { return table_store_.get(); }

  std::unique_ptr<exec::ExecState> CreateExecState() {
    return std::make_unique<exec::ExecState>(scalar_udf_registry_.get(), uda_registry_.get(),
                                             table_store_);
  }

  std::unique_ptr<plan::PlanState> CreatePlanState() {
    return std::make_unique<plan::PlanState>(scalar_udf_registry_.get(), uda_registry_.get());
  }

  std::unique_ptr<compiler::CompilerState> CreateCompilerState(types::Time64NSValue time_now) {
    auto rel_map = table_store_->GetRelationMap();
    return std::make_unique<compiler::CompilerState>(std::move(rel_map), registry_info_.get(),
                                                     time_now);
  }

 private:
  std::unique_ptr<udf::UDARegistry> uda_registry_;
  std::unique_ptr<udf::ScalarUDFRegistry> scalar_udf_registry_;
  std::shared_ptr<exec::TableStore> table_store_;
  std::shared_ptr<schema::Schema> schema_;
  std::unique_ptr<compiler::RegistryInfo> registry_info_;
};

}  // namespace carnot
}  // namespace pl
