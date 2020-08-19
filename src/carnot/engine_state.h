#pragma once

#include <arrow/memory_pool.h>
#include <memory>
#include <utility>

#include "src/carnot/exec/exec_state.h"
#include "src/carnot/funcs/funcs.h"
#include "src/carnot/plan/plan_state.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"
#include "src/table_store/table_store.h"

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
  EngineState() = delete;
  EngineState(std::unique_ptr<udf::Registry> func_registry,
              std::shared_ptr<table_store::TableStore> table_store,
              std::shared_ptr<table_store::schema::Schema> schema,
              std::unique_ptr<planner::RegistryInfo> registry_info,
              const exec::ResultSinkStubGenerator& stub_generator, exec::GRPCRouter* grpc_router)
      : func_registry_(std::move(func_registry)),
        table_store_(std::move(table_store)),
        schema_(std::move(schema)),
        registry_info_(std::move(registry_info)),
        stub_generator_(stub_generator),
        grpc_router_(grpc_router) {}

  static StatusOr<std::unique_ptr<EngineState>> CreateDefault(
      std::unique_ptr<udf::Registry> func_registry,
      std::shared_ptr<table_store::TableStore> table_store,
      const exec::ResultSinkStubGenerator& stub_generator, exec::GRPCRouter* grpc_router) {
    auto schema = std::make_shared<table_store::schema::Schema>();
    auto registry_info = std::make_unique<planner::RegistryInfo>();
    auto udf_info = func_registry->ToProto();
    PL_RETURN_IF_ERROR(registry_info->Init(udf_info));

    return std::make_unique<EngineState>(std::move(func_registry), table_store, schema,
                                         std::move(registry_info), stub_generator, grpc_router);
  }

  std::shared_ptr<table_store::schema::Schema> schema() { return schema_; }

  table_store::TableStore* table_store() { return table_store_.get(); }
  std::unique_ptr<exec::ExecState> CreateExecState(const sole::uuid& query_id) {
    return std::make_unique<exec::ExecState>(func_registry_.get(), table_store_, stub_generator_,
                                             query_id, grpc_router_);
  }

  std::unique_ptr<plan::PlanState> CreatePlanState() {
    return std::make_unique<plan::PlanState>(func_registry_.get());
  }

  std::unique_ptr<planner::CompilerState> CreateLocalExecutionCompilerState(
      types::Time64NSValue time_now) {
    auto rel_map = table_store_->GetRelationMap();
    // Use an empty string for query result address, because the local execution mode should use
    // the Local GRPC result server to send results to.
    return std::make_unique<planner::CompilerState>(std::move(rel_map), registry_info_.get(),
                                                    time_now, /* result address */ "");
  }

  const udf::Registry* func_registry() const { return func_registry_.get(); }

 private:
  std::unique_ptr<udf::Registry> func_registry_;
  std::shared_ptr<table_store::TableStore> table_store_;
  std::shared_ptr<table_store::schema::Schema> schema_;
  std::unique_ptr<planner::RegistryInfo> registry_info_;
  const exec::ResultSinkStubGenerator stub_generator_;
  exec::GRPCRouter* grpc_router_ = nullptr;
};

}  // namespace carnot
}  // namespace pl
