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
   * This includes the tables, udf registries, and row batch queue for passing row batches
   * outside of the current execution.
   * @return a status of whether initialization was successful.
   */
  Status Init(std::shared_ptr<table_store::TableStore> table_store,
              std::shared_ptr<exec::RowBatchQueue> row_batch_queue);

  StatusOr<CarnotQueryResult> ExecuteQuery(const std::string& query, const sole::uuid& query_id,
                                           types::Time64NSValue time_now) override;

  StatusOr<CarnotQueryResult> ExecutePlan(const planpb::Plan& plan,
                                          const sole::uuid& query_id) override;

  void RegisterAgentMetadataCallback(AgentMetadataCallbackFunc func) override {
    agent_md_callback_ = func;
  };

 private:
  Status RegisterUDFs(exec::ExecState* exec_state, plan::Plan* plan);

  Status RegisterUDFsInPlanFragment(exec::ExecState* exec_state, plan::PlanFragment* pf);
  Status WalkExpression(exec::ExecState* exec_state, const plan::ScalarExpression& expr);
  /**
   * Returns the Table Store.
   */
  exec::TableStore* table_store() { return engine_state_->table_store(); }

  std::shared_ptr<const md::AgentMetadataState> GetMetadataState() {
    if (!agent_md_callback_) {
      return nullptr;
    }
    return agent_md_callback_();
  }

  AgentMetadataCallbackFunc agent_md_callback_;
  compiler::Compiler compiler_;
  std::unique_ptr<EngineState> engine_state_;
};

Status CarnotImpl::Init(std::shared_ptr<table_store::TableStore> table_store,
                        std::shared_ptr<exec::RowBatchQueue> row_batch_queue) {
  PL_ASSIGN_OR_RETURN(engine_state_, EngineState::CreateDefault(table_store, row_batch_queue));
  return Status::OK();
}

StatusOr<CarnotQueryResult> CarnotImpl::ExecuteQuery(const std::string& query,
                                                     const sole::uuid& query_id,
                                                     types::Time64NSValue time_now) {
  // Compile the query.
  auto timer = ElapsedTimer();
  timer.Start();
  auto compiler_state = engine_state_->CreateCompilerState(time_now);
  PL_ASSIGN_OR_RETURN(auto logical_plan, compiler_.Compile(query, compiler_state.get()));
  timer.Stop();
  int64_t compile_time_ns = timer.ElapsedTime_us() * 1000;
  // Get the output table names from the plan.
  PL_ASSIGN_OR_RETURN(CarnotQueryResult plan_result, ExecutePlan(logical_plan, query_id));
  plan_result.compile_time_ns = compile_time_ns;
  return plan_result;
}

/**
 * @brief LogicalPlanUDFRegister takes in an exec_state and a plan, finds all of the
 * UDF/UDA definitions and then tracks them with the ids provided by the Logical Plan.
 *
 * This assumes that the logical plan has valid ids for each udf/uda and that ids
 * aren't shared by non-matching RegistryKeys.
 */

Status CarnotImpl::RegisterUDFs(exec::ExecState* exec_state, plan::Plan* plan) {
  return plan::PlanWalker()
      .OnPlanFragment(std::bind(&CarnotImpl::RegisterUDFsInPlanFragment, this, exec_state,
                                std::placeholders::_1))
      .Walk(plan);
}

Status CarnotImpl::RegisterUDFsInPlanFragment(exec::ExecState* exec_state, plan::PlanFragment* pf) {
  plan::PlanFragmentWalker()
      .OnMap([&](const plan::MapOperator& map) {
        for (const auto& expr : map.expressions()) {
          PL_RETURN_IF_ERROR(WalkExpression(exec_state, *expr));
        }
        return Status::OK();
      })
      .OnAggregate([&](const plan::AggregateOperator& agg) {
        for (const auto& expr : agg.values()) {
          PL_RETURN_IF_ERROR(WalkExpression(exec_state, *expr));
        }
        return Status::OK();
      })
      .OnFilter([&](const plan::FilterOperator& filter) {
        return WalkExpression(exec_state, *filter.expression());
      })
      .OnLimit([&](const auto&) {})
      .OnMemorySink([&](const auto&) {})
      .OnMemorySource([&](const auto&) {})
      .OnUnion([&](const auto&) {})
      .Walk(pf);
  return Status::OK();
}

Status CarnotImpl::WalkExpression(exec::ExecState* exec_state, const plan::ScalarExpression& expr) {
  StatusOr<bool> walk_result =
      plan::ExpressionWalker<bool>()
          .OnAggregateExpression([&](const plan::AggregateExpression& func, std::vector<bool>) {
            CHECK_EQ(func.args_types().size(), func.arg_deps().size());
            Status s = exec_state->AddUDA(func.uda_id(), func.name(), func.args_types());
            return s.ok();
          })
          .OnColumn([&](const auto&, std::vector<bool>) { return true; })
          .OnScalarFunc([&](const plan::ScalarFunc& func, std::vector<bool>) {
            CHECK_EQ(func.args_types().size(), func.arg_deps().size());
            Status s = exec_state->AddScalarUDF(func.udf_id(), func.name(), func.args_types());
            return s.ok();
          })
          .OnScalarValue([&](const auto&, std::vector<bool>) { return true; })
          .Walk(expr);
  if (!walk_result.ok() || !walk_result.ConsumeValueOrDie()) {
    return error::Internal("Error walking expression.");
  }
  return Status::OK();
}

StatusOr<CarnotQueryResult> CarnotImpl::ExecutePlan(const planpb::Plan& logical_plan,
                                                    const sole::uuid& query_id) {
  auto timer = ElapsedTimer();
  plan::Plan plan;
  PL_RETURN_IF_ERROR(plan.Init(logical_plan));
  // For each of the plan fragments in the plan, execute the query.
  std::vector<std::string> output_table_strs;
  auto exec_state = engine_state_->CreateExecState(query_id);

  // TODO(michelle/zasgar): We should periodically update the metadata state for long-running
  // queries after a certain time duration or number of row batches processed. For now, we use a
  // single metadata state throughout the entire length of the query.
  auto metadata_state = GetMetadataState();
  if (metadata_state) {
    exec_state->set_metadata_state(metadata_state);
  }

  PL_RETURN_IF_ERROR(RegisterUDFs(exec_state.get(), &plan));

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

  // Compile time is not set for ExecutePlan.
  int64_t compile_time_ns = 0;
  // Get the output table names from the plan.
  return CarnotQueryResult(output_tables, rows_processed, bytes_processed, compile_time_ns,
                           exec_time_ns);
}

StatusOr<std::unique_ptr<Carnot>> Carnot::Create(
    std::shared_ptr<table_store::TableStore> table_store,
    std::shared_ptr<exec::RowBatchQueue> row_batch_queue) {
  std::unique_ptr<Carnot> carnot_impl(new CarnotImpl());
  PL_RETURN_IF_ERROR(
      static_cast<CarnotImpl*>(carnot_impl.get())->Init(table_store, row_batch_queue));
  return carnot_impl;
}

}  // namespace carnot
}  // namespace pl
