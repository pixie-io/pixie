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

#include "src/carnot/planner/cgo_export.h"

#include <google/protobuf/text_format.h>
#include <memory>
#include <string>
#include <utility>

#include "src/carnot/planner/cgo_export_utils.h"
#include "src/carnot/planner/compiler/compiler.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/compiler_state/registry_info.h"
#include "src/carnot/planner/compilerpb/compiler_status.pb.h"
#include "src/carnot/planner/distributedpb/distributed_plan.pb.h"
#include "src/carnot/planner/logical_planner.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/time.h"
#include "src/shared/scriptspb/scripts.pb.h"
#include "src/table_store/schema/relation.h"
#include "src/table_store/schemapb/schema.pb.h"

using px::carnot::planner::distributedpb::LogicalPlannerResult;
using px::carnot::planner::plannerpb::CompileMutationsResponse;
using px::carnot::planner::plannerpb::GenerateOTelScriptResponse;

PlannerPtr PlannerNew(const char* udf_info_data, int udf_info_len) {
  std::string udf_info_pb_str(udf_info_data, udf_info_data + udf_info_len);
  px::carnot::udfspb::UDFInfo udf_info_pb;

  bool did_udf_info_pb_load = udf_info_pb.ParseFromString(udf_info_pb_str);
  CHECK(did_udf_info_pb_load) << "Couldn't process the udf_info";

  auto planner_or_s = px::carnot::planner::LogicalPlanner::Create(udf_info_pb);
  if (!planner_or_s.ok()) {
    return nullptr;
  }
  auto planner = planner_or_s.ConsumeValueOrDie();
  // We release the pointer b/c we are moving out of unique_ptr managed memory to Go.
  return reinterpret_cast<PlannerPtr>(planner.release());
}

#define PLANNER_RETURN_IF_ERROR(__type, __output_len, __status) \
  do {                                                          \
    const auto& s = (__status);                                 \
    if (!s.ok()) {                                              \
      return ExitEarly<__type>(s, __output_len);                \
    }                                                           \
  } while (false)

px::Status LoadProto(const std::string& serialized_proto, google::protobuf::Message* output,
                     const std::string& error_msg) {
  bool success = output->ParseFromString(serialized_proto);
  if (!success) {
    LOG(ERROR) << error_msg;
    return px::Status(px::statuspb::INVALID_ARGUMENT, error_msg);
  }
  return px::Status::OK();
}

char* PlannerPlan(PlannerPtr planner_ptr, const char* query_request_str_c,
                  int query_request_str_len, int* resultLen) {
  std::string query_request_pb_str(query_request_str_c,
                                   query_request_str_c + query_request_str_len);

  // Load in the query request protobuf.
  px::carnot::planner::plannerpb::QueryRequest query_request_pb;
  PLANNER_RETURN_IF_ERROR(
      LogicalPlannerResult, resultLen,
      LoadProto(query_request_pb_str, &query_request_pb, "Failed to process the query request"));

  auto planner = reinterpret_cast<px::carnot::planner::LogicalPlanner*>(planner_ptr);

  auto distributed_plan_status = planner->Plan(query_request_pb);
  if (!distributed_plan_status.ok()) {
    return ExitEarly<LogicalPlannerResult>(distributed_plan_status.status(), resultLen);
  }
  std::unique_ptr<px::carnot::planner::distributed::DistributedPlan> distributed_plan =
      distributed_plan_status.ConsumeValueOrDie();

  // If the response is ok, then we can go ahead and set this up.
  LogicalPlannerResult planner_result_pb;
  WrapStatus(&planner_result_pb, distributed_plan_status.status());
  // In the future, if we actually have plan options that will actually determine how the plan is
  // constructed, we may want to pass the planOptions to planner.Plan. However, this
  // will need to go through many more layers (such as the coordinator), so this is fine for now.
  distributed_plan->SetPlanOptions(query_request_pb.logical_planner_state().plan_options());

  auto plan_pb_status = distributed_plan->ToProto();
  if (!plan_pb_status.ok()) {
    return ExitEarly<LogicalPlannerResult>(plan_pb_status.status(), resultLen);
  }

  *(planner_result_pb.mutable_plan()) = plan_pb_status.ConsumeValueOrDie();

  // Serialize the logical plan into bytes.
  return PrepareResult(&planner_result_pb, resultLen);
}

char* PlannerCompileMutations(PlannerPtr planner_ptr, const char* mutation_request_str_c,
                              int mutation_request_str_len, int* resultLen) {
  std::string mutation_request_pb_str(mutation_request_str_c,
                                      mutation_request_str_c + mutation_request_str_len);

  // Load in the mutation request protobuf.
  px::carnot::planner::plannerpb::CompileMutationsRequest mutation_request_pb;
  PLANNER_RETURN_IF_ERROR(CompileMutationsResponse, resultLen,
                          LoadProto(mutation_request_pb_str, &mutation_request_pb,
                                    "Failed to parse the mutation request"));

  auto planner = reinterpret_cast<px::carnot::planner::LogicalPlanner*>(planner_ptr);

  auto dynamic_trace_or_s = planner->CompileTrace(mutation_request_pb);
  if (!dynamic_trace_or_s.ok()) {
    return ExitEarly<CompileMutationsResponse>(dynamic_trace_or_s.status(), resultLen);
  }
  std::unique_ptr<px::carnot::planner::compiler::MutationsIR> trace =
      dynamic_trace_or_s.ConsumeValueOrDie();

  // If the response is ok, then we can go ahead and set this up.
  CompileMutationsResponse mutations_response_pb;
  WrapStatus(&mutations_response_pb, dynamic_trace_or_s.status());

  PLANNER_RETURN_IF_ERROR(CompileMutationsResponse, resultLen,
                          trace->ToProto(&mutations_response_pb));

  // Serialize the tracing program into bytes.
  return PrepareResult(&mutations_response_pb, resultLen);
}

char* PlannerGenerateOTelScript(PlannerPtr planner_ptr,
                                const char* generate_otel_script_request_str_c,
                                int generate_otel_script_request_str_len, int* resultLen) {
  std::string generate_otel_script_request_pb_str(
      generate_otel_script_request_str_c,
      generate_otel_script_request_str_c + generate_otel_script_request_str_len);

  // Load in the mutation request protobuf.
  px::carnot::planner::plannerpb::GenerateOTelScriptRequest generate_otel_script_request_pb;
  PLANNER_RETURN_IF_ERROR(
      GenerateOTelScriptResponse, resultLen,
      LoadProto(generate_otel_script_request_pb_str, &generate_otel_script_request_pb,
                "Failed to parse the mutation request"));

  auto planner = reinterpret_cast<px::carnot::planner::LogicalPlanner*>(planner_ptr);

  auto response_or_s = planner->GenerateOTelScript(generate_otel_script_request_pb);
  if (!response_or_s.ok()) {
    return ExitEarly<GenerateOTelScriptResponse>(response_or_s.status(), resultLen);
  }

  // If the response is ok, then we can go ahead and set this up.
  std::unique_ptr<GenerateOTelScriptResponse> generate_otel_script_response_pb =
      response_or_s.ConsumeValueOrDie();
  WrapStatus(generate_otel_script_response_pb.get(), response_or_s.status());

  // Serialize the tracing program into bytes.
  return PrepareResult(generate_otel_script_response_pb.get(), resultLen);
}

void PlannerFree(PlannerPtr planner_ptr) {
  delete reinterpret_cast<px::carnot::planner::LogicalPlanner*>(planner_ptr);
}

void StrFree(char* str) { delete str; }
