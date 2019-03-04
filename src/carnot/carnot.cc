#include <memory>
#include <string>

#include "src/carnot/builtins/builtins.h"
#include "src/carnot/carnot.h"
#include "src/carnot/compiler/compiler.h"
#include "src/carnot/exec/table.h"
#include "src/carnot/plan/operators.h"
#include "src/common/elapsed_timer.h"

namespace pl {
namespace carnot {

Status Carnot::Init() {
  PL_ASSIGN_OR_RETURN(engine_state_, EngineState::CreateDefault());
  is_initialized_ = true;
  return Status::OK();
}

StatusOr<CarnotQueryResult> Carnot::ExecuteQuery(const std::string& query) {
  // Compile the query.
  auto timer = ElapsedTimer();
  timer.Start();
  auto compiler_state = engine_state_->CreateCompilerState();
  PL_ASSIGN_OR_RETURN(auto logical_plan, compiler_.Compile(query, compiler_state.get()));
  timer.Stop();
  int64_t compile_time_ns = timer.ElapsedTime_us() * 1000;

  plan::Plan plan;
  PL_RETURN_IF_ERROR(plan.Init(logical_plan));
  // For each of the plan fragments in the plan, execute the query.
  std::vector<std::string> output_table_strs;
  auto exec_state = engine_state_->CreateExecState();
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

  std::vector<exec::Table*> output_tables;
  output_tables.reserve(output_table_strs.size());
  for (const auto& table_str : output_table_strs) {
    output_tables.push_back(table_store()->GetTable(table_str));
  }

  // Get the output table names from the plan.
  return CarnotQueryResult(output_tables, rows_processed, bytes_processed, compile_time_ns,
                           exec_time_ns);
}

}  // namespace carnot
}  // namespace pl
