#include <memory>
#include <string>

#include "src/carnot/builtins/builtins.h"
#include "src/carnot/carnot.h"
#include "src/carnot/compiler/compiler.h"
#include "src/carnot/exec/table.h"

namespace pl {
namespace carnot {

Status Carnot::Init() {
  PL_ASSIGN_OR_RETURN(engine_state_, EngineState::CreateDefault());
  is_initialized_ = true;
  return Status::OK();
}

Status Carnot::ExecuteQuery(const std::string& query) {
  // Compile the query.
  auto compiler_state = engine_state_->CreateCompilerState();
  PL_ASSIGN_OR_RETURN(auto logical_plan, compiler_.Compile(query, compiler_state.get()));

  plan::Plan plan;
  PL_RETURN_IF_ERROR(plan.Init(logical_plan));
  // For each of the plan fragments in the plan, execute the query.
  auto exec_state = engine_state_->CreateExecState();
  auto plan_state = engine_state_->CreatePlanState();
  auto s = plan::PlanWalker()
               .OnPlanFragment([&](auto* pf) {
                 auto exec_graph = exec::ExecutionGraph();
                 PL_RETURN_IF_ERROR(exec_graph.Init(engine_state_->schema(), plan_state.get(),
                                                    exec_state.get(), pf));
                 PL_RETURN_IF_ERROR(exec_graph.Execute());
                 return Status::OK();
               })
               .Walk(&plan);
  PL_RETURN_IF_ERROR(s);
  return Status::OK();
}

void Carnot::AddTable(const std::string& table_name, std::shared_ptr<exec::Table> table) {
  auto exec_state = engine_state_->CreateExecState();
  auto table_store = exec_state->table_store();
  table_store->AddTable(table_name, table);
}

exec::Table* Carnot::GetTable(const std::string& table_name) {
  auto exec_state = engine_state_->CreateExecState();
  auto table_store = exec_state->table_store();
  return table_store->GetTable(table_name);
}

}  // namespace carnot
}  // namespace pl
