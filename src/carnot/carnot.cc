#include <memory>
#include <string>

#include "src/carnot/builtins/builtins.h"
#include "src/carnot/carnot.h"
#include "src/carnot/compiler/compiler.h"
#include "src/carnot/engine_state.h"
#include "src/carnot/exec/exec_graph.h"
#include "src/carnot/plan/operators.h"
#include "src/carnot/plan/plan.h"
#include "src/carnot/udf/registry.h"
#include "src/common/perf/perf.h"
#include "src/shared/types/type_utils.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {

using types::DataType;

Status CarnotQueryResult::ToProto(queryresultspb::QueryResult* query_result) const {
  CHECK(query_result != nullptr);
  auto* exec_stats = query_result->mutable_execution_stats();
  exec_stats->set_records_processed(rows_processed);
  exec_stats->set_bytes_processed(bytes_processed);

  auto* timing_info = query_result->mutable_timing_info();
  timing_info->set_execution_time_ns(exec_time_ns);
  timing_info->set_compilation_time_ns(compile_time_ns);

  for (const auto table : output_tables_) {
    PL_RETURN_IF_ERROR(table->ToProto(query_result->add_tables()));
  }
  return Status::OK();
}

class CarnotImpl final : public Carnot {
 public:
  /**
   * Initializes the engine with the state necessary to compile and execute a query.
   * This includes the tables and udf registries.
   * @return a status of whether initialization was successful.
   */
  Status Init(std::shared_ptr<table_store::TableStore> table_store);

  StatusOr<CarnotQueryResult> ExecuteQuery(const std::string& query,
                                           types::Time64NSValue time_now) override;

 private:
  /**
   * Returns the Table Store.
   */
  exec::TableStore* table_store() { return engine_state_->table_store(); }

  compiler::Compiler compiler_;
  std::unique_ptr<EngineState> engine_state_;
};

Status CarnotImpl::Init(std::shared_ptr<table_store::TableStore> table_store) {
  PL_ASSIGN_OR_RETURN(engine_state_, EngineState::CreateDefault(table_store));
  return Status::OK();
}

StatusOr<CarnotQueryResult> CarnotImpl::ExecuteQuery(const std::string& query,
                                                     types::Time64NSValue time_now) {
  // Compile the query.
  auto timer = ElapsedTimer();
  timer.Start();
  auto compiler_state = engine_state_->CreateCompilerState(time_now);
  PL_ASSIGN_OR_RETURN(auto logical_plan, compiler_.Compile(query, compiler_state.get()));
  timer.Stop();
  int64_t compile_time_ns = timer.ElapsedTime_us() * 1000;
  plan::Plan plan;
  PL_RETURN_IF_ERROR(plan.Init(logical_plan));
  // For each of the plan fragments in the plan, execute the query.
  std::vector<std::string> output_table_strs;
  auto exec_state = engine_state_->CreateExecState();

  // Initialize ScalarUDFs and UDAs.
  for (const auto& kv : compiler_state->udf_to_id_map()) {
    auto key = kv.first;
    auto name = key.name();
    auto arg_types = key.registry_arg_types();
    PL_RETURN_IF_ERROR(exec_state->AddScalarUDF(kv.second, name, arg_types));
  }
  for (const auto& kv : compiler_state->uda_to_id_map()) {
    auto key = kv.first;
    auto name = key.name();
    auto arg_types = key.registry_arg_types();
    PL_RETURN_IF_ERROR(exec_state->AddUDA(kv.second, name, arg_types));
  }

  auto plan_state = engine_state_->CreatePlanState();
  int64_t bytes_processed = 0;
  int64_t rows_processed = 0;
  timer.Start();
  auto s =
      plan::PlanWalker()
          .OnPlanFragment([&](auto* pf) {
            auto exec_graph = exec::ExecutionGraph();
            PL_RETURN_IF_ERROR(
                exec_graph.Init(engine_state_->schema(), plan_state.get(), exec_state.get(), pf));
            PL_RETURN_IF_ERROR(exec_graph.Execute());
            std::vector<std::string> frag_sinks = exec_graph.OutputTables();
            output_table_strs.insert(output_table_strs.end(), frag_sinks.begin(), frag_sinks.end());
            auto exec_stats = exec_graph.GetStats();
            bytes_processed += exec_stats.bytes_processed;
            rows_processed += exec_stats.rows_processed;
            return Status::OK();
          })
          .Walk(&plan);
  PL_RETURN_IF_ERROR(s);
  timer.Stop();
  int64_t exec_time_ns = timer.ElapsedTime_us() * 1000;

  std::vector<table_store::Table*> output_tables;
  output_tables.reserve(output_table_strs.size());
  for (const auto& table_str : output_table_strs) {
    output_tables.push_back(table_store()->GetTable(table_str));
  }

  // Get the output table names from the plan.
  return CarnotQueryResult(output_tables, rows_processed, bytes_processed, compile_time_ns,
                           exec_time_ns);
}

StatusOr<std::unique_ptr<Carnot>> Carnot::Create(
    std::shared_ptr<table_store::TableStore> table_store) {
  std::unique_ptr<Carnot> carnot_impl(new CarnotImpl());
  PL_RETURN_IF_ERROR(static_cast<CarnotImpl*>(carnot_impl.get())->Init(table_store));
  return carnot_impl;
}

}  // namespace carnot
}  // namespace pl
