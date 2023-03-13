/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <memory>
#include <string>

#include "src/carnot/carnot.h"
#include "src/carnot/carnotpb/carnot.grpc.pb.h"
#include "src/carnot/carnotpb/carnot.pb.h"
#include "src/carnot/engine_state.h"
#include "src/carnot/exec/exec_graph.h"
#include "src/carnot/exec/grpc_router.h"
#include "src/carnot/funcs/builtins/builtins.h"
#include "src/carnot/plan/operators.h"
#include "src/carnot/plan/plan.h"
#include "src/carnot/planner/compiler/compiler.h"
#include "src/carnot/planner/distributed/annotate_abortable_sources_for_limits_rule.h"
#include "src/carnot/udf/registry.h"
#include "src/common/perf/perf.h"
#include "src/shared/types/type_utils.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {

using types::DataType;

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
  Status Init(const sole::uuid& agent_id, std::unique_ptr<udf::Registry> func_registry,
              std::shared_ptr<table_store::TableStore> table_store,
              std::unique_ptr<ClientsConfig> clients_config,
              std::unique_ptr<ServerConfig> server_config);

  Status ExecuteQuery(const std::string& query, const sole::uuid& query_id,
                      types::Time64NSValue time_now, bool analyze) override;

  Status ExecutePlan(const planpb::Plan& plan, const sole::uuid& query_id, bool analyze) override;

  void RegisterAgentMetadataCallback(AgentMetadataCallbackFunc func) override {
    agent_md_callback_ = func;
  };

  const udf::Registry* FuncRegistry() const override { return engine_state_->func_registry(); }

  EngineState* GetEngineState() override { return engine_state_.get(); }

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

  bool HasGRPCServer() { return grpc_server_ != nullptr; }

  void GRPCServerFunc();

  AgentMetadataCallbackFunc agent_md_callback_;
  planner::compiler::Compiler compiler_;
  std::unique_ptr<EngineState> engine_state_;

  std::unique_ptr<std::thread> grpc_server_thread_;
  std::unique_ptr<grpc::Server> grpc_server_;
  std::unique_ptr<ClientsConfig> clients_config_;
  std::unique_ptr<ServerConfig> server_config_;

  // The id of the agent that owns this Carnot instance.
  sole::uuid agent_id_;
};

Status CarnotImpl::Init(const sole::uuid& agent_id, std::unique_ptr<udf::Registry> func_registry,
                        std::shared_ptr<table_store::TableStore> table_store,
                        std::unique_ptr<ClientsConfig> clients_config,
                        std::unique_ptr<ServerConfig> server_config) {
  agent_id_ = agent_id;
  clients_config_ = std::move(clients_config);
  server_config_ = std::move(server_config);
  if (server_config_->grpc_server_port > 0) {
    grpc_server_thread_ = std::make_unique<std::thread>(&CarnotImpl::GRPCServerFunc, this);
  }

  PX_ASSIGN_OR_RETURN(engine_state_,
                      EngineState::CreateDefault(std::move(func_registry), table_store,
                                                 clients_config_->stub_generator,
                                                 clients_config_->add_auth_to_grpc_context_func,
                                                 &server_config_->grpc_router));
  return Status::OK();
}

Status CarnotImpl::ExecuteQuery(const std::string& query, const sole::uuid& query_id,
                                types::Time64NSValue time_now, bool analyze) {
  // Compile the query.
  auto compiler_state = engine_state_->CreateLocalExecutionCompilerState(time_now);
  PX_ASSIGN_OR_RETURN(auto logical_plan, compiler_.CompileToIR(query, compiler_state.get()));
  // TOOD(james/nserrino/philkuz): This is a hack to make sure that the distributed rule for limits
  // gets run even in carnot_test. We should think about how we want to run distributed analyzer
  // rules in these test envs.
  planner::distributed::AnnotateAbortableSourcesForLimitsRule rule;
  PX_RETURN_IF_ERROR(rule.Execute(logical_plan.get()));
  PX_ASSIGN_OR_RETURN(auto plan_proto, logical_plan->ToProto());
  auto dest = plan_proto.add_execution_status_destinations();
  dest->set_grpc_address(compiler_state->result_address());
  dest->set_ssl_targetname(compiler_state->result_ssl_targetname());
  return ExecutePlan(plan_proto, query_id, analyze);
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
  auto no_op = [&](const auto&) { return Status::OK(); };
  return plan::PlanFragmentWalker()
      .OnMap([&](const plan::MapOperator& map) {
        for (const auto& expr : map.expressions()) {
          PX_RETURN_IF_ERROR(WalkExpression(exec_state, *expr));
        }
        return Status::OK();
      })
      .OnAggregate([&](const plan::AggregateOperator& agg) {
        for (const auto& expr : agg.values()) {
          PX_RETURN_IF_ERROR(WalkExpression(exec_state, *expr));
        }
        return Status::OK();
      })
      .OnFilter([&](const plan::FilterOperator& filter) {
        return WalkExpression(exec_state, *filter.expression());
      })
      .OnLimit(no_op)
      .OnMemorySink(no_op)
      .OnMemorySource(no_op)
      .OnUnion(no_op)
      .OnJoin(no_op)
      .OnGRPCSource(no_op)
      .OnGRPCSink(no_op)
      .OnUDTFSource(no_op)
      .OnEmptySource(no_op)
      .OnOTelSink(no_op)
      .Walk(pf);
}

absl::flat_hash_map<std::string, carnotpb::ResultSinkService::StubInterface*> GetOutgoingConns(
    exec::ExecState* exec_state, const planpb::Plan& plan) {
  absl::flat_hash_map<std::string, carnotpb::ResultSinkService::StubInterface*> outgoing_conns;
  for (const auto& dest : plan.execution_status_destinations()) {
    outgoing_conns[dest.grpc_address()] =
        exec_state->ResultSinkServiceStub(dest.grpc_address(), dest.ssl_targetname());
  }
  return outgoing_conns;
}

Status CarnotImpl::WalkExpression(exec::ExecState* exec_state, const plan::ScalarExpression& expr) {
  StatusOr<bool> walk_result =
      plan::ExpressionWalker<bool>()
          .OnAggregateExpression([&](const plan::AggregateExpression& func, std::vector<bool>) {
            CHECK_EQ(func.registry_arg_types().size(),
                     func.init_arguments().size() + func.arg_deps().size());
            Status s = exec_state->AddUDA(func.uda_id(), func.name(), func.registry_arg_types());
            return s.ok();
          })
          .OnColumn([&](const auto&, std::vector<bool>) { return true; })
          .OnScalarFunc([&](const plan::ScalarFunc& func, std::vector<bool>) {
            CHECK_EQ(func.registry_arg_types().size(),
                     func.init_arguments().size() + func.arg_deps().size());
            Status s =
                exec_state->AddScalarUDF(func.udf_id(), func.name(), func.registry_arg_types());
            return s.ok();
          })
          .OnScalarValue([&](const auto&, std::vector<bool>) { return true; })
          .Walk(expr);
  if (!walk_result.ok() || !walk_result.ConsumeValueOrDie()) {
    return error::Internal("Error walking expression.");
  }
  return Status::OK();
}

void CarnotImpl::GRPCServerFunc() {
  CHECK(server_config_ != nullptr);

  std::string server_address(absl::Substitute("0.0.0.0:$0", server_config_->grpc_server_port));
  grpc::ServerBuilder builder;

  builder.AddListeningPort(server_address, server_config_->grpc_server_creds);
  builder.RegisterService(&server_config_->grpc_router);
  grpc_server_ = builder.BuildAndStart();
  std::cout << "Server listening on " << server_address << std::endl;
  CHECK(grpc_server_ != nullptr);
  grpc_server_->Wait();
}

Status SendTransferResultChunkToOutgoingConns(
    const absl::flat_hash_map<std::string, carnotpb::ResultSinkService::StubInterface*>&
        outgoing_servers,
    const std::function<void(grpc::ClientContext*)>& add_auth_to_grpc_context_func,
    ::px::carnotpb::TransferResultChunkRequest req) {
  if (outgoing_servers.empty()) {
    return Status::OK();
  }
  for (const auto& [addr, server] : outgoing_servers) {
    ::px::carnotpb::TransferResultChunkResponse resp;
    req.set_address(addr);
    grpc::ClientContext context;
    add_auth_to_grpc_context_func(&context);
    context.set_deadline(std::chrono::system_clock::now() + kRPCResultTimeout);
    auto writer = server->TransferResultChunk(&context, &resp);
    writer->Write(req);
    writer->WritesDone();
    auto status = writer->Finish();
    if (!status.ok()) {
      return error::Internal(
          "Failed to call Finish on TransferResultChunk. "
          "Status: $0",
          status.error_message());
    }
  }
  return Status::OK();
}

Status SendFinalExecutionStatsToOutgoingConns(
    const sole::uuid& query_id,
    const absl::flat_hash_map<std::string, carnotpb::ResultSinkService::StubInterface*>&
        outgoing_servers,
    std::function<void(grpc::ClientContext*)> add_auth_to_grpc_context_func,
    const queryresultspb::AgentExecutionStats& agent_stats,
    const std::vector<queryresultspb::AgentExecutionStats>& all_agent_stats) {
  ::px::carnotpb::TransferResultChunkRequest req;
  ToProto(query_id, req.mutable_query_id());

  int64_t total_bytes_processed = 0;
  int64_t total_records_processed = 0;

  // Add all of the agent stats.
  for (const auto& agent_stats : all_agent_stats) {
    *(req.mutable_execution_and_timing_info()->add_agent_execution_stats()) = agent_stats;
    total_records_processed += agent_stats.records_processed();
    total_bytes_processed += agent_stats.bytes_processed();
  }

  // Add the stats for this particular agent.
  auto stats = req.mutable_execution_and_timing_info()->mutable_execution_stats();
  stats->mutable_timing()->set_execution_time_ns(agent_stats.execution_time_ns());
  stats->set_bytes_processed(total_bytes_processed);
  stats->set_records_processed(total_records_processed);
  return SendTransferResultChunkToOutgoingConns(outgoing_servers, add_auth_to_grpc_context_func,
                                                std::move(req));
}

Status SendErrorToOutgoingConns(
    const sole::uuid& query_id,
    const absl::flat_hash_map<std::string, carnotpb::ResultSinkService::StubInterface*>&
        outgoing_servers,
    const std::function<void(grpc::ClientContext*)>& add_auth_to_grpc_context_func, Status s) {
  ::px::carnotpb::TransferResultChunkRequest req;
  ToProto(query_id, req.mutable_query_id());
  s.ToProto(req.mutable_execution_error());
  return SendTransferResultChunkToOutgoingConns(outgoing_servers, add_auth_to_grpc_context_func,
                                                std::move(req));
}

Status InitiateOutgoingConns(
    const sole::uuid& query_id,
    const absl::flat_hash_map<std::string, carnotpb::ResultSinkService::StubInterface*>&
        outgoing_servers,
    const std::function<void(grpc::ClientContext*)>& add_auth_to_grpc_context_func) {
  // Only run this if there are outgoing_servers.
  if (outgoing_servers.empty()) {
    return Status::OK();
  }
  ::px::carnotpb::TransferResultChunkRequest req;
  ToProto(query_id, req.mutable_query_id());
  *req.mutable_initiate_conn() = ::px::carnotpb::TransferResultChunkRequest::InitiateConnection();
  return SendTransferResultChunkToOutgoingConns(outgoing_servers, add_auth_to_grpc_context_func,
                                                std::move(req));
}

Status CarnotImpl::ExecutePlan(const planpb::Plan& logical_plan, const sole::uuid& query_id,
                               bool analyze) {
  auto timer = ElapsedTimer();
  plan::Plan plan;

  PX_RETURN_IF_ERROR(plan.Init(logical_plan));

  // For each of the plan fragments in the plan, execute the query.
  std::vector<std::string> output_table_strs;
  auto exec_state = engine_state_->CreateExecState(query_id);
  auto outgoing_conns = GetOutgoingConns(exec_state.get(), logical_plan);
  PX_RETURN_IF_ERROR(InitiateOutgoingConns(query_id, outgoing_conns,
                                           engine_state_->add_auth_to_grpc_context_func()));

  // TODO(michellenguyen/zasgar, PP-2579): We should periodically update the metadata state for
  // long-running queries after a certain time duration or number of row batches processed. For now,
  // we use a single metadata state throughout the entire length of the query.
  auto metadata_state = GetMetadataState();
  if (metadata_state) {
    exec_state->set_metadata_state(metadata_state);
  }

  PX_RETURN_IF_ERROR(RegisterUDFs(exec_state.get(), &plan));

  auto plan_state = engine_state_->CreatePlanState();
  int64_t bytes_processed = 0;
  int64_t rows_processed = 0;
  queryresultspb::AgentExecutionStats agent_operator_exec_stats;
  ToProto(agent_id_, agent_operator_exec_stats.mutable_agent_id());
  timer.Start();
  // Unclear how we'll use plan fragments in the future (they're currently unused). For now, we will
  // share the schema between plan fragments.
  auto schema = std::make_unique<table_store::schema::Schema>();
  std::vector<statuspb::Status> incoming_errors;
  auto s =
      plan::PlanWalker()
          .OnPlanFragment([&](auto* pf) {
            auto exec_graph = exec::ExecutionGraph();
            PX_RETURN_IF_ERROR(exec_graph.Init(schema.get(), plan_state.get(), exec_state.get(), pf,
                                               /* collect_exec_node_stats */ analyze));
            PX_RETURN_IF_ERROR(exec_graph.Execute());

            // We must get this while exec_graph is alive. ExecutionGraph destructor calls
            // GRPCRouter::DeleteQuery() which would delete this data.
            auto errors = exec_state->grpc_router()->GetIncomingWorkerErrors(query_id);
            incoming_errors.insert(incoming_errors.end(), errors.begin(), errors.end());
            auto exec_stats = exec_graph.GetStats();
            bytes_processed += exec_stats.bytes_processed;
            rows_processed += exec_stats.rows_processed;

            if (analyze) {
              for (int64_t node_id : pf->dag().TopologicalSort()) {
                PX_ASSIGN_OR_RETURN(auto exec_node, exec_graph.node(node_id));
                std::string node_name =
                    absl::Substitute("$0 (id=$1)", pf->nodes()[node_id]->DebugString(), node_id);
                exec::ExecNodeStats* stats = exec_node->stats();
                stats->AddExtraMetric("batches_output", stats->batches_output);
                int64_t total_time_ns = stats->TotalExecTime();
                int64_t self_time_ns = stats->SelfExecTime();
                LOG(INFO) << absl::Substitute(
                    "self_time:$1\ttotal_time: $2\tbytes_output: $3\trows_output: $4\tnode_id:$0",
                    node_name, PrettyDuration(self_time_ns), PrettyDuration(total_time_ns),
                    stats->bytes_output, stats->rows_output);

                queryresultspb::OperatorExecutionStats* stats_pb =
                    agent_operator_exec_stats.add_operator_execution_stats();
                stats_pb->set_plan_fragment_id(pf->id());
                stats_pb->set_node_id(node_id);
                stats_pb->set_bytes_output(stats->bytes_output);
                stats_pb->set_records_output(stats->rows_output);
                stats_pb->set_total_execution_time_ns(total_time_ns);
                stats_pb->set_self_execution_time_ns(self_time_ns);

                for (const auto& [k, v] : stats->extra_metrics) {
                  (*stats_pb->mutable_extra_metrics())[k] = v;
                }

                for (const auto& [k, v] : stats->extra_info) {
                  (*stats_pb->mutable_extra_info())[k] = v;
                }
                (*stats_pb->mutable_extra_info())["DebugString"] =
                    pf->nodes()[node_id]->DebugString();
              }
            }
            return Status::OK();
          })
          .Walk(&plan);
  if (!s.ok()) {
    PX_RETURN_IF_ERROR(SendErrorToOutgoingConns(query_id, outgoing_conns,
                                                engine_state_->add_auth_to_grpc_context_func(), s));
    return s;
  }

  if (!incoming_errors.empty()) {
    std::vector<std::string> messages;
    for (const auto& e : incoming_errors) {
      messages.push_back(e.msg());
    }

    auto combined_status = Status(incoming_errors[0].err_code(), absl::StrJoin(messages, "\n"));

    PX_RETURN_IF_ERROR(SendErrorToOutgoingConns(
        query_id, outgoing_conns, engine_state_->add_auth_to_grpc_context_func(), combined_status));
    return combined_status;
  }

  std::vector<uuidpb::UUID> incoming_agents;
  for (const auto& id : logical_plan.incoming_agent_ids()) {
    incoming_agents.push_back(id);
  }
  timer.Stop();

  std::vector<queryresultspb::AgentExecutionStats> input_agent_stats;
  if (HasGRPCServer() && !incoming_agents.empty()) {
    PX_ASSIGN_OR_RETURN(input_agent_stats,
                        server_config_->grpc_router.GetIncomingWorkerExecStats(query_id));
    if (input_agent_stats.size() != incoming_agents.size()) {
      LOG(WARNING) << absl::Substitute("Agent ids are not the same size. Got $0 and expected $1",
                                       input_agent_stats.size(), incoming_agents.size());
    }
  }

  // Compute bytes processed and records processed across all agents for all queries,
  // regardless of flags.
  for (const auto& agent_stats : input_agent_stats) {
    bytes_processed += agent_stats.bytes_processed();
    rows_processed += agent_stats.records_processed();
  }

  agent_operator_exec_stats.set_execution_time_ns(timer.ElapsedTime_us() * 1000);
  agent_operator_exec_stats.set_bytes_processed(bytes_processed);
  agent_operator_exec_stats.set_records_processed(rows_processed);

  std::vector<queryresultspb::AgentExecutionStats> all_agent_stats;
  if (analyze) {
    all_agent_stats = input_agent_stats;
  }

  // Even if analyze is set to false, send the most basic exec stats (rows, etc) per agent.
  // analyze=true will send per operator stats.
  all_agent_stats.push_back(agent_operator_exec_stats);

  return SendFinalExecutionStatsToOutgoingConns(query_id, outgoing_conns,
                                                engine_state_->add_auth_to_grpc_context_func(),
                                                agent_operator_exec_stats, all_agent_stats);
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
    const sole::uuid& agent_id, std::unique_ptr<udf::Registry> func_registry,
    std::shared_ptr<table_store::TableStore> table_store,
    std::unique_ptr<ClientsConfig> clients_config, std::unique_ptr<ServerConfig> server_config) {
  std::unique_ptr<Carnot> carnot_impl(new CarnotImpl());
  PX_RETURN_IF_ERROR(static_cast<CarnotImpl*>(carnot_impl.get())
                         ->Init(agent_id, std::move(func_registry), table_store,
                                std::move(clients_config), std::move(server_config)));
  return carnot_impl;
}

}  // namespace carnot
}  // namespace px
