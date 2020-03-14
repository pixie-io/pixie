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
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/base/time.h"
#include "src/shared/scriptspb/scripts.pb.h"
#include "src/table_store/proto/schema.pb.h"
#include "src/table_store/schema/relation.h"

using pl::carnot::planner::distributedpb::LogicalPlannerResult;
using pl::carnot::planner::plannerpb::GetAvailableFlagsResult;
using pl::shared::scriptspb::VizFuncsInfoResult;

PlannerPtr PlannerNew(const char* udf_info_data, int udf_info_len) {
  std::string udf_info_str(udf_info_data, udf_info_data + udf_info_len);
  pl::carnot::udfspb::UDFInfo udf_info_pb;

  bool did_udf_info_pb_load =
      google::protobuf::TextFormat::MergeFromString(udf_info_str, &udf_info_pb);

  CHECK(did_udf_info_pb_load) << absl::Substitute("Couldn't process the udf_info: $0.",
                                                  udf_info_str);

  auto planner_or_s = pl::carnot::planner::LogicalPlanner::Create(udf_info_pb);
  if (!planner_or_s.ok()) {
    return nullptr;
  }
  auto planner = planner_or_s.ConsumeValueOrDie();
  // We release the pointer b/c we are moving out of unique_ptr managed memory to Go.
  return reinterpret_cast<PlannerPtr>(planner.release());
}

char* PlannerPlan(PlannerPtr planner_ptr, const char* planner_state_str_c,
                  int planner_state_str_len, const char* query_request_str_c,
                  int query_request_str_len, int* resultLen) {
  DCHECK(planner_state_str_c != nullptr);
  DCHECK(query_request_str_c != nullptr);
  std::string planner_state_pb_str(planner_state_str_c,
                                   planner_state_str_c + planner_state_str_len);
  std::string query_request_pb_str(query_request_str_c,
                                   query_request_str_c + query_request_str_len);

  // Load in the planner state protobuf.
  pl::carnot::planner::distributedpb::LogicalPlannerState planner_state_pb;
  // TODO(philkuz) convert this to read serialized calls instead of human readable.
  bool planner_state_merge_success =
      google::protobuf::TextFormat::MergeFromString(planner_state_pb_str, &planner_state_pb);
  if (!planner_state_merge_success) {
    std::string err =
        absl::Substitute("Failed to process the logical planner state: $0.", planner_state_pb_str);
    LOG(ERROR) << err;
    return ExitEarly<LogicalPlannerResult>(err, resultLen);
  }

  // Load in the query request protobuf.
  pl::carnot::planner::plannerpb::QueryRequest query_request_pb;
  // TODO(philkuz) convert this to read serialized calls instead of human readable.
  bool query_request_merge_success =
      google::protobuf::TextFormat::MergeFromString(query_request_pb_str, &query_request_pb);
  if (!query_request_merge_success) {
    std::string err =
        absl::Substitute("Failed to process the query request: $0.", query_request_pb_str);
    LOG(ERROR) << err;
    return ExitEarly<LogicalPlannerResult>(err, resultLen);
  }

  auto planner = reinterpret_cast<pl::carnot::planner::LogicalPlanner*>(planner_ptr);

  auto distributed_plan_status = planner->Plan(planner_state_pb, query_request_pb);
  if (!distributed_plan_status.ok()) {
    return ExitEarly<LogicalPlannerResult>(distributed_plan_status.status(), resultLen);
  }
  std::unique_ptr<pl::carnot::planner::distributed::DistributedPlan> distributed_plan =
      distributed_plan_status.ConsumeValueOrDie();

  // If the response is ok, then we can go ahead and set this up.
  LogicalPlannerResult planner_result_pb;
  WrapStatus(&planner_result_pb, distributed_plan_status.status());
  // In the future, if we actually have plan options that will actually determine how the plan is
  // constructed, we may want to pass the planOptions to planner.Plan. However, this
  // will need to go through many more layers (such as the coordinator), so this is fine for now.
  distributed_plan->SetPlanOptions(planner_state_pb.plan_options());

  auto plan_pb_status = distributed_plan->ToProto();
  if (!plan_pb_status.ok()) {
    return ExitEarly<LogicalPlannerResult>(plan_pb_status.status(), resultLen);
  }

  *(planner_result_pb.mutable_plan()) = plan_pb_status.ConsumeValueOrDie();

  // Serialize the logical plan into bytes.
  return PrepareResult(&planner_result_pb, resultLen);
}

char* PlannerGetAvailableFlags(PlannerPtr planner_ptr, const char* query_request_str_c,
                               int query_request_str_len, int* resultLen) {
  DCHECK(query_request_str_c != nullptr);
  std::string query_request_pb_str(query_request_str_c,
                                   query_request_str_c + query_request_str_len);
  pl::carnot::planner::plannerpb::QueryRequest query_request_pb;
  bool query_request_merge_success =
      google::protobuf::TextFormat::MergeFromString(query_request_pb_str, &query_request_pb);
  if (!query_request_merge_success) {
    std::string err =
        absl::Substitute("Failed to process the query request: $0.", query_request_pb_str);
    LOG(ERROR) << err;
    return ExitEarly<GetAvailableFlagsResult>(err, resultLen);
  }

  auto planner = reinterpret_cast<pl::carnot::planner::LogicalPlanner*>(planner_ptr);

  auto query_flags_spec_status = planner->GetAvailableFlags(query_request_pb);
  if (!query_flags_spec_status.ok()) {
    return ExitEarly<GetAvailableFlagsResult>(query_flags_spec_status.status(), resultLen);
  }

  GetAvailableFlagsResult flags_response_pb;
  WrapStatus(&flags_response_pb, query_flags_spec_status.status());
  *(flags_response_pb.mutable_query_flags()) = query_flags_spec_status.ConsumeValueOrDie();

  return PrepareResult(&flags_response_pb, resultLen);
}

char* PlannerVizFuncsInfo(PlannerPtr planner_ptr, const char* script_str_c, int script_str_len,
                          int* resultLen) {
  DCHECK(script_str_c != nullptr);
  std::string script_str(script_str_c, script_str_c + script_str_len);

  auto planner = reinterpret_cast<pl::carnot::planner::LogicalPlanner*>(planner_ptr);

  auto viz_funcs_info_or_s = planner->GetVizFuncsInfo(script_str);
  if (!viz_funcs_info_or_s.ok()) {
    return ExitEarly<VizFuncsInfoResult>(viz_funcs_info_or_s.status(), resultLen);
  }

  VizFuncsInfoResult viz_funcs_pb;
  WrapStatus(&viz_funcs_pb, viz_funcs_info_or_s.status());
  *(viz_funcs_pb.mutable_info()) = viz_funcs_info_or_s.ConsumeValueOrDie();

  return PrepareResult(&viz_funcs_pb, resultLen);
}

void PlannerFree(PlannerPtr planner_ptr) {
  delete reinterpret_cast<pl::carnot::planner::LogicalPlanner*>(planner_ptr);
}

void StrFree(char* str) { delete str; }
