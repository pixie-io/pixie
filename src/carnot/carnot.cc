#include <memory>
#include <string>

#include "src/carnot/carnot.h"
#include "src/carnot/compiler/compiler.h"
#include "src/carnot/engine_state.h"
#include "src/carnot/exec/exec_graph.h"
#include "src/carnot/funcs/builtins/builtins.h"
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

  for (size_t i = 0; i < output_tables_.size(); ++i) {
    auto table = query_result->add_tables();
    PL_RETURN_IF_ERROR(output_tables_[i]->ToProto(table));
    table->set_name(table_names_[i]);
  }
  return Status::OK();
}

class CarnotImpl final : public Carnot {
 public:
  ~CarnotImpl() override;
  /**
   * Initializes the engine with the state necessary to compile and execute a query.
   * This includes the tables, udf registries, and the stub generator for generating stubs
   * to the Kelvin GRPC service.
   * grpc_server_port of 0 disables the GRPC server.
   * @return a status of whether initialization was successful.
   */
  Status Init(std::shared_ptr<table_store::TableStore> table_store,
              const exec::KelvinStubGenerator& stub_generator, int grpc_server_port = 0,
              std::shared_ptr<grpc::ServerCredentials> grpc_server_creds = nullptr);

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
  table_store::TableStore* table_store() { return engine_state_->table_store(); }

  std::shared_ptr<const md::AgentMetadataState> GetMetadataState() {
    if (!agent_md_callback_) {
      return nullptr;
    }
    return agent_md_callback_();
  }

  void GRPCServerFunc();

  /**
   * @brief This rewrites the logical plan's sinks to be related to the query id, removing a bug
   * that we encountered when you write a table with the same output name but different relation.
   *
   * @param logical_plan: the original logical_plan
   * @param query_id: the query id to use as some form of the sink name.
   * @return planpb::Plan: the modified plan.
   */
  planpb::Plan InterceptSinksInPlan(const planpb::Plan& logical_plan, const sole::uuid& query_id,
                                    absl::flat_hash_map<std::string, std::string>*);

  AgentMetadataCallbackFunc agent_md_callback_;
  compiler::Compiler compiler_;
  std::unique_ptr<EngineState> engine_state_;

  std::shared_ptr<grpc::ServerCredentials> grpc_server_creds_;
  // GRPC server setup.
  // TODO(zasgar/nserrino): Pull these out to another class that we can inject into Carnot.
  // TODO(zasgar/nserrino): GRPC server should not need threads and we should be able to use
  // pl::event instead.
  std::unique_ptr<std::thread> grpc_server_thread_;
  std::unique_ptr<grpc::Server> grpc_server_;
  std::unique_ptr<exec::GRPCRouter> grpc_router_;
  int grpc_server_port_;
};

Status CarnotImpl::Init(std::shared_ptr<table_store::TableStore> table_store,
                        const exec::KelvinStubGenerator& stub_generator, int grpc_server_port,
                        std::shared_ptr<grpc::ServerCredentials> grpc_server_creds) {
  grpc_server_creds_ = grpc_server_creds;
  grpc_server_port_ = grpc_server_port;
  grpc_router_ = std::make_unique<exec::GRPCRouter>();
  if (grpc_server_port_ > 0) {
    grpc_server_thread_ = std::make_unique<std::thread>(&CarnotImpl::GRPCServerFunc, this);
  }

  PL_ASSIGN_OR_RETURN(engine_state_,
                      EngineState::CreateDefault(table_store, stub_generator, grpc_router_.get()));
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
      .OnJoin([&](const auto&) {})
      .OnGRPCSource([&](const auto&) {})
      .OnGRPCSink([&](const auto&) {})
      .OnUDTFSource([&](const auto&) {})
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

planpb::Plan CarnotImpl::InterceptSinksInPlan(
    const planpb::Plan& logical_plan, const sole::uuid& query_id,
    absl::flat_hash_map<std::string, std::string>* table_name_mapping) {
  planpb::Plan modified_logical_plan = logical_plan;
  int64_t sink_counter = 0;
  for (int64_t i = 0; i < modified_logical_plan.nodes_size(); ++i) {
    auto plan_fragment = modified_logical_plan.mutable_nodes(i);
    for (int64_t frag_i = 0; frag_i < plan_fragment->nodes_size(); ++frag_i) {
      auto plan_op = plan_fragment->mutable_nodes(frag_i);
      if (plan_op->op().op_type() == planpb::MEMORY_SINK_OPERATOR) {
        auto mem_sink = plan_op->mutable_op()->mutable_mem_sink_op();
        auto new_name = absl::Substitute("$0_$1", query_id.str(), sink_counter);
        (*table_name_mapping)[new_name] = mem_sink->name();
        *(mem_sink->mutable_name()) = new_name;
        sink_counter += 1;
      }
    }
  }

  return modified_logical_plan;
}

void CarnotImpl::GRPCServerFunc() {
  CHECK(grpc_router_ != nullptr);
  CHECK(grpc_server_creds_ != nullptr);

  std::string server_address(absl::Substitute("0.0.0.0:$0", grpc_server_port_));
  grpc::ServerBuilder builder;

  builder.AddListeningPort(server_address, grpc_server_creds_);
  builder.RegisterService(grpc_router_.get());
  grpc_server_ = builder.BuildAndStart();
  std::cout << "Server listening on " << server_address << std::endl;
  CHECK(grpc_server_ != nullptr);
  grpc_server_->Wait();
}

StatusOr<CarnotQueryResult> CarnotImpl::ExecutePlan(const planpb::Plan& logical_plan,
                                                    const sole::uuid& query_id) {
  auto timer = ElapsedTimer();
  plan::Plan plan;

  // Here we intercept the logical plan and remove all references to user specified table names.
  // TODO(philkuz) in the future when we remove the name argumetn from Results, rework this name.
  absl::flat_hash_map<std::string, std::string> table_name_mapping;

  PL_RETURN_IF_ERROR(plan.Init(InterceptSinksInPlan(logical_plan, query_id, &table_name_mapping)));

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
  std::vector<std::string> output_table_names;
  output_table_names.reserve(output_table_strs.size());
  for (const auto& table_str : output_table_strs) {
    auto table_name_mapping_iter = table_name_mapping.find(table_str);
    if (table_name_mapping_iter == table_name_mapping.end()) {
      output_table_names.push_back(table_str);
    } else {
      output_table_names.push_back(table_name_mapping_iter->second);
    }

    output_tables.push_back(table_store()->GetTable(table_str));
  }

  // Compile time is not set for ExecutePlan.
  int64_t compile_time_ns = 0;
  // Get the output table names from the plan.
  return CarnotQueryResult{output_tables,   output_table_names, rows_processed,
                           bytes_processed, compile_time_ns,    exec_time_ns};
}

CarnotImpl::~CarnotImpl() {
  if (grpc_server_ && grpc_server_thread_) {
    grpc_server_->Shutdown();
    if (grpc_server_thread_->joinable()) {
      grpc_server_thread_->join();
    }
  }
}

StatusOr<std::unique_ptr<Carnot>> Carnot::Create(
    std::shared_ptr<table_store::TableStore> table_store,
    const exec::KelvinStubGenerator& stub_generator, int grpc_server_port,
    std::shared_ptr<grpc::ServerCredentials> grpc_server_creds) {
  std::unique_ptr<Carnot> carnot_impl(new CarnotImpl());
  PL_RETURN_IF_ERROR(static_cast<CarnotImpl*>(carnot_impl.get())
                         ->Init(table_store, stub_generator, grpc_server_port, grpc_server_creds));
  return carnot_impl;
}

}  // namespace carnot
}  // namespace pl
