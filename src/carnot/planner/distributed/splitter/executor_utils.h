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
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/pattern_match.h"
#include "src/carnot/udfspb/udfs.pb.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

// This file contains utility functions for the distributed planner.

// Check if this node is a function that has a particular UDF source executor.
// Note, if you want to recursively check children, or check operators, use
// `HasFuncWithExecutor`.
inline StatusOr<bool> IsFuncWithExecutor(CompilerState* compiler_state, IRNode* node,
                                         udfspb::UDFSourceExecutor executor) {
  // If a node is not an operator nor a func, then there are no scalar funcs to check.
  if (!Match(node, Func())) {
    return false;
  }
  auto func = static_cast<FuncIR*>(node);

  PX_ASSIGN_OR_RETURN(auto udf_type,
                      compiler_state->registry_info()->GetUDFExecType(func->func_name()));
  if (udf_type != UDFExecType::kUDF) {
    return false;
  }

  if (!func->HasRegistryArgTypes()) {
    return error::Internal("func '$0' doesn't have RegistryArgTypes set.", func->func_name());
  }
  PX_ASSIGN_OR_RETURN(auto udf_executor, compiler_state->registry_info()->GetUDFSourceExecutor(
                                             func->func_name(), func->registry_arg_types()));
  return executor == udf_executor;
}

// Check if this node is a function that has a particular UDF source executor,
// or if its child UDFs have that executor.
// Note, if you only want to check the input function, rather than its children,
// use `IsFuncWithExecutor`.
inline StatusOr<bool> HasFuncWithExecutor(CompilerState* compiler_state, IRNode* node,
                                          udfspb::UDFSourceExecutor executor) {
  if (Match(node, Operator())) {
    // Get the operator's child funcs, and call this function on them to check if their UDFs can
    // successfully execute based on the target execution type.
    auto op_children = node->graph()->dag().DependenciesOf(node->id());
    for (const auto& op_child_id : op_children) {
      auto child_node = node->graph()->Get(op_child_id);
      if (Match(child_node, Func())) {
        PX_ASSIGN_OR_RETURN(auto res, HasFuncWithExecutor(compiler_state, child_node, executor));
        if (res) {
          return res;
        }
      }
    }
  }

  PX_ASSIGN_OR_RETURN(auto is_scalar_func_executor,
                      IsFuncWithExecutor(compiler_state, node, executor));
  if (is_scalar_func_executor) {
    return true;
  }

  // If a node is not an operator nor a func, then there are no scalar funcs to check.
  if (!Match(node, Func())) {
    return false;
  }
  auto func = static_cast<FuncIR*>(node);

  // Get the children match the condition.
  std::vector<types::DataType> children_data_types;
  for (const auto& arg : func->args()) {
    PX_ASSIGN_OR_RETURN(auto res, HasFuncWithExecutor(compiler_state, arg, executor));
    if (res) {
      return res;
    }
  }
  return false;
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
