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

#pragma once
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/planner/compiler/compiler.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/distributed/distributed_plan/distributed_plan.h"
#include "src/carnot/planner/distributed/distributed_planner.h"
#include "src/carnot/planner/plannerpb/func_args.pb.h"
#include "src/carnot/planner/probes/probes.h"
#include "src/shared/scriptspb/scripts.pb.h"

namespace px {
namespace carnot {
namespace planner {

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
   * @param logical_state: the distributed layout of the vizier instance.
   * @param query: QueryRequest
   * @return std::unique_ptr<DistributedPlan> or error if one occurs during compilation.
   */
  StatusOr<std::unique_ptr<distributed::DistributedPlan>> Plan(
      const distributedpb::LogicalPlannerState& logical_state,
      const plannerpb::QueryRequest& query);

  StatusOr<std::unique_ptr<compiler::MutationsIR>> CompileTrace(
      const distributedpb::LogicalPlannerState& logical_state,
      const plannerpb::CompileMutationsRequest& mutations_req);

  /**
   * @brief Get the Main Func Args Spec for a query. Must have a main function in the query or the
   * method will return an error.
   *
   * @param query_request
   * @return StatusOr<shared::scriptspb::FuncArgsSpec>
   */
  StatusOr<shared::scriptspb::FuncArgsSpec> GetMainFuncArgsSpec(
      const plannerpb::QueryRequest& query_request);

  /**
   * @brief Takes in a script string and outputs information about vis funcs for that script.
   *
   * @param script: the string of the script.
   * @return VisFuncsInfo or error if one occurs during compilation.
   */
  StatusOr<px::shared::scriptspb::VisFuncsInfo> GetVisFuncsInfo(const std::string& script_str);

  Status Init(std::unique_ptr<planner::RegistryInfo> registry_info);
  Status Init(const udfspb::UDFInfo& udf_info);

 protected:
  LogicalPlanner() {}

 private:
  compiler::Compiler compiler_;
  std::unique_ptr<distributed::Planner> distributed_planner_;
  std::unique_ptr<planner::RegistryInfo> registry_info_;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
