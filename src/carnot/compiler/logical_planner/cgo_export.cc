#include "src/carnot/compiler/logical_planner/cgo_export.h"

#include <google/protobuf/text_format.h>
#include <memory>
#include <string>
#include <utility>

#include "src/carnot/compiler/compiler.h"
#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/compiler/compilerpb/compiler_status.pb.h"
#include "src/carnot/compiler/distributedpb/distributed_plan.pb.h"
#include "src/carnot/compiler/logical_planner/logical_planner.h"
#include "src/carnot/compiler/registry_info.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/base/time.h"
#include "src/table_store/proto/schema.pb.h"
#include "src/table_store/schema/relation.h"

namespace {
char* CloneStringToCharArray(std::string str, int* ret_len) {
  *ret_len = str.size();
  char* retval = new char[str.size()];
  memcpy(retval, str.data(), str.size());
  return retval;
}

void WrapStatus(pl::carnot::compiler::distributedpb::LogicalPlannerResult* planner_result_pb,
                const pl::Status& status) {
  DCHECK(planner_result_pb);
  status.ToProto(planner_result_pb->mutable_status());
}

char* PrepareResult(pl::carnot::compiler::distributedpb::LogicalPlannerResult* planner_result_pb,
                    int* result_len) {
  DCHECK(planner_result_pb);
  std::string serialized;
  bool success = planner_result_pb->SerializeToString(&serialized);

  if (!success) {
    *result_len = 0;
    return nullptr;
  }
  return CloneStringToCharArray(serialized, result_len);
}

char* ExitEarly(const pl::Status& status, int* result_len) {
  DCHECK(result_len != nullptr);
  pl::carnot::compiler::distributedpb::LogicalPlannerResult planner_result_pb;
  WrapStatus(&planner_result_pb, status);
  return PrepareResult(&planner_result_pb, result_len);
}

char* ExitEarly(const std::string& err, int* result_len) {
  return ExitEarly(pl::error::InvalidArgument(err), result_len);
}

}  // namespace

PlannerPtr PlannerNew() {
  auto planner_or_s = pl::carnot::compiler::logical_planner::LogicalPlanner::Create();
  if (!planner_or_s.ok()) {
    return nullptr;
  }
  auto planner = planner_or_s.ConsumeValueOrDie();
  // We release the pointer b/c we are moving out of unique_ptr managed memory to Go.
  return reinterpret_cast<PlannerPtr>(planner.release());
}

char* PlannerPlan(PlannerPtr planner_ptr, const char* planner_state_str_c,
                  int planner_state_str_len, const char* query_str_c, int query_str_len,
                  int* resultLen) {
  DCHECK(planner_state_str_c != nullptr);
  DCHECK(query_str_c != nullptr);
  std::string planner_state_pb_str(planner_state_str_c,
                                   planner_state_str_c + planner_state_str_len);
  std::string query_str(query_str_c, query_str_c + query_str_len);

  pl::carnot::compiler::distributedpb::LogicalPlannerState planner_state_pb;
  // TODO(philkuz) convert this to read serialized calls instead of human readable.
  bool str_merge_success =
      google::protobuf::TextFormat::MergeFromString(planner_state_pb_str, &planner_state_pb);
  if (!str_merge_success) {
    LOG(ERROR) << absl::Substitute("Couldn't process the logical planner state: $0.",
                                   planner_state_pb_str);
    return ExitEarly("The state of the execution system couldn't be parsed", resultLen);
  }

  auto planner =
      reinterpret_cast<pl::carnot::compiler::logical_planner::LogicalPlanner*>(planner_ptr);

  auto distributed_plan_status = planner->Plan(planner_state_pb, query_str);
  if (!distributed_plan_status.ok()) {
    return ExitEarly(distributed_plan_status.status(), resultLen);
  }
  std::unique_ptr<pl::carnot::compiler::distributed::DistributedPlan> distributed_plan =
      distributed_plan_status.ConsumeValueOrDie();

  // If the response is ok, then we can go ahead and set this up.
  pl::carnot::compiler::distributedpb::LogicalPlannerResult planner_result_pb;
  WrapStatus(&planner_result_pb, distributed_plan_status.status());
  auto plan_pb_status = distributed_plan->ToProto();
  if (!plan_pb_status.ok()) {
    return ExitEarly(plan_pb_status.status(), resultLen);
  }

  *(planner_result_pb.mutable_plan()) = plan_pb_status.ConsumeValueOrDie();

  // Serialize the logical plan into bytes.
  return PrepareResult(&planner_result_pb, resultLen);
}

void PlannerFree(PlannerPtr planner_ptr) {
  delete reinterpret_cast<pl::carnot::compiler::logical_planner::LogicalPlanner*>(planner_ptr);
}

void StrFree(char* str) { delete str; }
