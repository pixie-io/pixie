#pragma once
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/compiler/compiler.h"
#include "src/carnot/compiler/compiler_state/compiler_state.h"
#include "src/carnot/compiler/distributed_plan.h"
#include "src/carnot/compiler/distributed_planner.h"
#include "src/carnot/compiler/plannerpb/query_flags.pb.h"
#include "src/carnot/compiler/tablet_rules.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace logical_planner {
/**
 * @brief The logical planner takes in queries and a Logical Planner State and
 *
 */
class LogicalPlanner : public NotCopyable {
 public:
  /**
   * @brief The Creation function for the planner.
   *
   * @return StatusOr<std::unique_ptr<DistributedPlanner>>: the distributed planner object or an
   * error.
   */
  static StatusOr<std::unique_ptr<LogicalPlanner>> Create(const udfspb::UDFInfo& udf_info);

  /**
   * @brief Takes in a logical plan and outputs the distributed plan.
   *
   * @param distributed_state: the distributed layout of the vizier instance.
   * @param compiler_state: informastion passed to the compiler.
   * @param logical_plan
   * @return std::unique_ptr<DistributedPlan> or error if one occurs during compilation.
   */
  StatusOr<std::unique_ptr<distributed::DistributedPlan>> Plan(
      const distributedpb::LogicalPlannerState& logical_state,
      const plannerpb::QueryRequest& query);

  /**
   * @brief Takes in a query request and outputs the flag spec for that request.
   *
   * @param query_request: the query request from the UI
   * @return QueryFlagsSpec or an error if one occurs during compilation.
   *
   * Currently, not implemented and just returns an empty QueryFlagsSpec
   */
  StatusOr<plannerpb::QueryFlagsSpec> GetAvailableFlags(
      const plannerpb::QueryRequest& /* query_request */) {
    plannerpb::QueryFlagsSpec query_flags;
    return query_flags;
  }

  Status Init(std::unique_ptr<compiler::RegistryInfo> registry_info);
  Status Init(const udfspb::UDFInfo& udf_info);

 protected:
  LogicalPlanner() {}

 private:
  StatusOr<std::unique_ptr<RelationMap>> MakeRelationMap(
      const table_store::schemapb::Schema& schema_pb);

  StatusOr<std::unique_ptr<CompilerState>> CreateCompilerState(
      const distributedpb::LogicalPlannerState& logical_state, RegistryInfo* registry_info);
  Status ApplyTabletizer(distributed::DistributedPlan* distributed_plan);

  Compiler compiler_;
  std::unique_ptr<distributed::Planner> distributed_planner_;
  std::unique_ptr<compiler::RegistryInfo> registry_info_;
};

}  // namespace logical_planner
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
