#include <memory>
#include <string>

#include "src/carnot/builtins/builtins.h"
#include "src/carnot/carnot.h"
#include "src/carnot/compiler/compiler.h"
#include "src/carnot/exec/table.h"
#include "src/carnot/plan/operators.h"

namespace pl {
namespace carnot {

Status Carnot::Init() {
  PL_ASSIGN_OR_RETURN(engine_state_, EngineState::CreateDefault());
  is_initialized_ = true;
  return Status::OK();
}

StatusOr<CarnotQueryResult> Carnot::ExecuteQuery(const std::string& query) {
  // Compile the query.
  auto compiler_state = engine_state_->CreateCompilerState();
  PL_ASSIGN_OR_RETURN(auto logical_plan, compiler_.Compile(query, compiler_state.get()));

  plan::Plan plan;
  PL_RETURN_IF_ERROR(plan.Init(logical_plan));
  // For each of the plan fragments in the plan, execute the query.
  std::vector<std::string> output_table_strs;
  auto exec_state = engine_state_->CreateExecState();
  auto plan_state = engine_state_->CreatePlanState();
  auto s =
      plan::PlanWalker()
          .OnPlanFragment([&](auto* pf) {
            auto exec_graph = exec::ExecutionGraph();
            PL_RETURN_IF_ERROR(
                exec_graph.Init(engine_state_->schema(), plan_state.get(), exec_state.get(), pf));
            PL_RETURN_IF_ERROR(exec_graph.Execute());
            std::vector<std::string> frag_sinks = exec_graph.OutputTables();
            output_table_strs.insert(output_table_strs.end(), frag_sinks.begin(), frag_sinks.end());
            return Status::OK();
          })
          .Walk(&plan);
  PL_RETURN_IF_ERROR(s);
  std::vector<exec::Table*> output_tables;
  output_tables.reserve(output_table_strs.size());
  for (const auto& table_str : output_table_strs) {
    output_tables.push_back(table_store()->GetTable(table_str));
  }

  // Get the output table names from the plan.
  return CarnotQueryResult(output_tables);
}

}  // namespace carnot
}  // namespace pl
