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

#include "src/carnot/planner/logical_planner.h"

#include <utility>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/ast_utils.h"
#include "src/carnot/planner/otel_generator/otel_generator.h"
#include "src/carnot/planner/parser/parser.h"
#include "src/shared/scriptspb/scripts.pb.h"

namespace px {
namespace carnot {
namespace planner {

using table_store::schemapb::Schema;

StatusOr<std::unique_ptr<RelationMap>> MakeRelationMapFromSchema(const Schema& schema_pb) {
  auto rel_map = std::make_unique<RelationMap>();
  for (auto& relation_pair : schema_pb.relation_map()) {
    px::table_store::schema::Relation rel;
    PX_RETURN_IF_ERROR(rel.FromProto(&relation_pair.second));
    rel_map->emplace(relation_pair.first, rel);
  }

  return rel_map;
}
StatusOr<std::unique_ptr<RelationMap>> MakeRelationMapFromDistributedState(
    const distributedpb::DistributedState& state_pb) {
  auto rel_map = std::make_unique<RelationMap>();
  for (const auto& schema_info : state_pb.schema_info()) {
    px::table_store::schema::Relation rel;
    PX_RETURN_IF_ERROR(rel.FromProto(&schema_info.relation()));
    rel_map->emplace(schema_info.name(), rel);
  }

  return rel_map;
}

static inline RedactionOptions RedactionOptionsFromPb(
    const distributedpb::RedactionOptions& redaction_options) {
  RedactionOptions options;
  options.use_full_redaction = redaction_options.use_full_redaction();
  options.use_px_redact_pii_best_effort = redaction_options.use_px_redact_pii_best_effort();
  return options;
}

StatusOr<std::unique_ptr<CompilerState>> CreateCompilerState(
    const distributedpb::LogicalPlannerState& logical_state, RegistryInfo* registry_info,
    int64_t max_output_rows_per_table) {
  PX_ASSIGN_OR_RETURN(std::unique_ptr<RelationMap> rel_map,
                      MakeRelationMapFromDistributedState(logical_state.distributed_state()));

  SensitiveColumnMap sensitive_columns = {
      {"cql_events", {"req_body", "resp_body"}},
      {"http_events", {"req_headers", "req_body", "resp_headers", "resp_body"}},
      {"kafka_events.beta", {"req_body", "resp"}},
      {"mysql_events", {"req_body", "resp_body"}},
      {"nats_events.beta", {"body", "resp"}},
      {"pgsql_events", {"req", "resp"}},
      {"redis_events", {"req_args", "resp"}}};

  std::unique_ptr<planpb::OTelEndpointConfig> otel_endpoint_config = nullptr;
  if (logical_state.has_otel_endpoint_config()) {
    otel_endpoint_config = std::make_unique<planpb::OTelEndpointConfig>();
    otel_endpoint_config->set_url(logical_state.otel_endpoint_config().url());
    for (const auto& [key, value] : logical_state.otel_endpoint_config().headers()) {
      (*otel_endpoint_config->mutable_headers())[key] = value;
    }
    otel_endpoint_config->set_insecure(logical_state.otel_endpoint_config().insecure());
    otel_endpoint_config->set_timeout(logical_state.otel_endpoint_config().timeout());
  }
  std::unique_ptr<planner::PluginConfig> plugin_config = nullptr;
  if (logical_state.has_plugin_config()) {
    plugin_config = std::unique_ptr<planner::PluginConfig>(
        new planner::PluginConfig{logical_state.plugin_config().start_time_ns(),
                                  logical_state.plugin_config().end_time_ns()});
  }
  planner::DebugInfo debug_info;
  for (const auto& debug_info_pb : logical_state.debug_info().otel_debug_attributes()) {
    debug_info.otel_debug_attrs.push_back({debug_info_pb.name(), debug_info_pb.value()});
  }
  // Create a CompilerState obj using the relation map and grabbing the current time.
  return std::make_unique<planner::CompilerState>(
      std::move(rel_map), sensitive_columns, registry_info, px::CurrentTimeNS(),
      max_output_rows_per_table, logical_state.result_address(),
      logical_state.result_ssl_targetname(),
      // TODO(philkuz) add an endpoint config to logical_state and pass that in here.
      RedactionOptionsFromPb(logical_state.redaction_options()), std::move(otel_endpoint_config),
      // TODO(philkuz) propagate the otel debug attributes here.
      std::move(plugin_config), debug_info);
}

StatusOr<std::unique_ptr<LogicalPlanner>> LogicalPlanner::Create(const udfspb::UDFInfo& udf_info) {
  auto planner = std::unique_ptr<LogicalPlanner>(new LogicalPlanner());
  PX_RETURN_IF_ERROR(planner->Init(udf_info));
  return planner;
}

Status LogicalPlanner::Init(const udfspb::UDFInfo& udf_info) {
  compiler_ = compiler::Compiler();
  registry_info_ = std::make_unique<planner::RegistryInfo>();
  PX_RETURN_IF_ERROR(registry_info_->Init(udf_info));

  PX_ASSIGN_OR_RETURN(distributed_planner_, distributed::DistributedPlanner::Create());
  return Status::OK();
}

StatusOr<std::unique_ptr<distributed::DistributedPlan>> LogicalPlanner::Plan(
    const plannerpb::QueryRequest& query_request) {
  // Compile into the IR.

  auto ms = query_request.logical_planner_state().plan_options().max_output_rows_per_table();
  VLOG(1) << "Max output rows: " << ms;
  PX_ASSIGN_OR_RETURN(
      std::unique_ptr<CompilerState> compiler_state,
      CreateCompilerState(query_request.logical_planner_state(), registry_info_.get(), ms));

  std::vector<plannerpb::FuncToExecute> exec_funcs(query_request.exec_funcs().begin(),
                                                   query_request.exec_funcs().end());
  PX_ASSIGN_OR_RETURN(
      std::shared_ptr<IR> single_node_plan,
      compiler_.CompileToIR(query_request.query_str(), compiler_state.get(), exec_funcs));
  // Create the distributed plan.
  PX_ASSIGN_OR_RETURN(
      auto distributed_plan,
      distributed_planner_->Plan(query_request.logical_planner_state().distributed_state(),
                                 compiler_state.get(), single_node_plan.get()));
  distributed_plan->SetExecutionCompleteAddress(
      query_request.logical_planner_state().result_address(),
      query_request.logical_planner_state().result_ssl_targetname());
  return distributed_plan;
}

StatusOr<std::unique_ptr<compiler::MutationsIR>> LogicalPlanner::CompileTrace(
    const plannerpb::CompileMutationsRequest& mutations_req) {
  // Compile into the IR.
  auto ms = mutations_req.logical_planner_state().plan_options().max_output_rows_per_table();
  VLOG(1) << "Max output rows: " << ms;
  PX_ASSIGN_OR_RETURN(
      std::unique_ptr<CompilerState> compiler_state,
      CreateCompilerState(mutations_req.logical_planner_state(), registry_info_.get(), ms));

  std::vector<plannerpb::FuncToExecute> exec_funcs(mutations_req.exec_funcs().begin(),
                                                   mutations_req.exec_funcs().end());

  return compiler_.CompileTrace(mutations_req.query_str(), compiler_state.get(), exec_funcs);
}

StatusOr<std::unique_ptr<plannerpb::GenerateOTelScriptResponse>> LogicalPlanner::GenerateOTelScript(
    const plannerpb::GenerateOTelScriptRequest& generate_req) {
  PX_ASSIGN_OR_RETURN(std::unique_ptr<CompilerState> compiler_state,
                      CreateCompilerState(generate_req.logical_planner_state(),
                                          registry_info_.get(), /* max_output_rows */ 0));

  PX_ASSIGN_OR_RETURN(auto out_script,
                      OTelGenerator::GenerateOTelScript(&compiler_, compiler_state.get(),
                                                        generate_req.pxl_script()));
  auto generate_resp = std::make_unique<plannerpb::GenerateOTelScriptResponse>();
  generate_resp->set_otel_script(out_script);
  return generate_resp;
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
