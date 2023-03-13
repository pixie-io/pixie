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

#include <vector>

#include "src/carnot/planner/distributed/splitter/scalar_udfs_run_on_executor_rule.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

// Helper struct for the scalar UDF checker rules, ScalarUDFsRunOnPEMRule/etc.
struct UDFCheckerResult {
  bool success;
  std::string bad_udf_name;
  static UDFCheckerResult Success() {
    UDFCheckerResult res;
    res.success = true;
    return res;
  }
  static UDFCheckerResult Fail(const std::string& udf_name) {
    UDFCheckerResult res;
    res.success = false;
    res.bad_udf_name = udf_name;
    return res;
  }
};

// Helper func for the scalar UDF checker rules, ScalarUDFsRunOnPEMRule/etc.
StatusOr<UDFCheckerResult> CheckScalarFuncExecutor(
    CompilerState* compiler_state, IRNode* node,
    absl::flat_hash_set<udfspb::UDFSourceExecutor> valid) {
  if (Match(node, Operator())) {
    // Get the operator's child funcs, and call this function on them to check if their UDFs can
    // successfully execute based on the target execution type.
    auto op_children = node->graph()->dag().DependenciesOf(node->id());
    for (const auto& op_child_id : op_children) {
      auto child_node = node->graph()->Get(op_child_id);
      if (Match(child_node, Func())) {
        PX_ASSIGN_OR_RETURN(auto res, CheckScalarFuncExecutor(compiler_state, child_node, valid));
        if (!res.success) {
          return res;
        }
      }
    }
  }

  // If a node is not an operator nor a func, then there are no scalar funcs to check.
  if (!Match(node, Func())) {
    return UDFCheckerResult::Success();
  }
  auto func = static_cast<FuncIR*>(node);

  for (const auto& arg : func->args()) {
    PX_ASSIGN_OR_RETURN(auto res, CheckScalarFuncExecutor(compiler_state, arg, valid));
    if (!res.success) {
      return res;
    }
  }

  PX_ASSIGN_OR_RETURN(auto udf_type,
                      compiler_state->registry_info()->GetUDFExecType(func->func_name()));
  if (udf_type != UDFExecType::kUDF) {
    return UDFCheckerResult::Success();
  }

  if (!func->HasRegistryArgTypes()) {
    return error::Internal("func '$0' doesn't have RegistryArgTypes set.", func->func_name());
  }
  PX_ASSIGN_OR_RETURN(auto executor, compiler_state->registry_info()->GetUDFSourceExecutor(
                                         func->func_name(), func->registry_arg_types()));
  if (!valid.contains(executor)) {
    return UDFCheckerResult::Fail(func->func_name());
  }
  return UDFCheckerResult::Success();
}

const absl::flat_hash_set<udfspb::UDFSourceExecutor> kValidPEMUDFExecutors{
    udfspb::UDFSourceExecutor::UDF_ALL, udfspb::UDFSourceExecutor::UDF_PEM};
const absl::flat_hash_set<udfspb::UDFSourceExecutor> kValidKelvinUDFExecutors{
    udfspb::UDFSourceExecutor::UDF_ALL, udfspb::UDFSourceExecutor::UDF_KELVIN};

StatusOr<bool> ScalarUDFsRunOnPEMRule::OperatorUDFsRunOnPEM(CompilerState* compiler_state,
                                                            OperatorIR* op) {
  PX_ASSIGN_OR_RETURN(auto res, CheckScalarFuncExecutor(compiler_state, op, kValidPEMUDFExecutors));
  return res.success;
}

StatusOr<bool> ScalarUDFsRunOnPEMRule::Apply(IRNode* node) {
  if (!Match(node, Operator())) {
    return false;
  }
  PX_ASSIGN_OR_RETURN(auto res,
                      CheckScalarFuncExecutor(compiler_state_, node, kValidPEMUDFExecutors));
  if (!res.success) {
    return node->CreateIRNodeError(
        "UDF '$0' must execute after blocking nodes such as limit, agg, and join.",
        res.bad_udf_name);
  }
  return false;
}

StatusOr<bool> ScalarUDFsRunOnKelvinRule::OperatorUDFsRunOnKelvin(CompilerState* compiler_state,
                                                                  OperatorIR* op) {
  PX_ASSIGN_OR_RETURN(auto res,
                      CheckScalarFuncExecutor(compiler_state, op, kValidKelvinUDFExecutors));
  return res.success;
}

StatusOr<bool> ScalarUDFsRunOnKelvinRule::Apply(IRNode* node) {
  if (!Match(node, Operator())) {
    return false;
  }
  PX_ASSIGN_OR_RETURN(auto res,
                      CheckScalarFuncExecutor(compiler_state_, node, kValidKelvinUDFExecutors));
  if (!res.success) {
    return node->CreateIRNodeError(
        "UDF '$0' must execute before blocking nodes such as limit, agg, and join.",
        res.bad_udf_name);
  }
  return false;
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
