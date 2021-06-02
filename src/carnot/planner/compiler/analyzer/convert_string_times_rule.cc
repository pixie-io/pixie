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

#include "src/carnot/planner/compiler/analyzer/convert_string_times_rule.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

// This feels like the wrong way to be handling String->Time Conversion,
// should consider rearchitecting it
StatusOr<bool> ConvertStringTimesRule::Apply(IRNode* node) {
  if (Match(node, MemorySource())) {
    MemorySourceIR* mem_src = static_cast<MemorySourceIR*>(node);
    return HandleMemSrc(mem_src);
  } else if (Match(node, Rolling())) {
    RollingIR* rolling = static_cast<RollingIR*>(node);
    return HandleRolling(rolling);
  } else {
    return false;
  }
}

StatusOr<bool> ConvertStringTimesRule::HandleMemSrc(MemorySourceIR* mem_src) {
  if (mem_src->IsTimeSet() || !mem_src->HasTimeExpressions()) {
    return false;
  }
  bool start_has_string_time = HasStringTime(mem_src->start_time_expr());
  bool end_has_string_time = HasStringTime(mem_src->end_time_expr());

  if (!start_has_string_time && !end_has_string_time) {
    return false;
  }

  ExpressionIR* start_time = mem_src->start_time_expr();
  ExpressionIR* end_time = mem_src->end_time_expr();

  if (start_has_string_time) {
    PL_ASSIGN_OR_RETURN(start_time, ConvertStringTimes(start_time, /* relative_time */ true));
  }
  if (end_has_string_time) {
    PL_ASSIGN_OR_RETURN(end_time, ConvertStringTimes(end_time, /* relative_time */ true));
  }

  PL_RETURN_IF_ERROR(mem_src->SetTimeExpressions(start_time, end_time));
  return true;
}

StatusOr<bool> ConvertStringTimesRule::HandleRolling(RollingIR* rolling) {
  if (!HasStringTime(rolling->window_size())) {
    return false;
  }
  PL_ASSIGN_OR_RETURN(ExpressionIR * new_window_size,
                      ConvertStringTimes(rolling->window_size(), /* relative_time */ false));
  PL_RETURN_IF_ERROR(rolling->ReplaceWindowSize(new_window_size));
  return true;
}

bool ConvertStringTimesRule::HasStringTime(const ExpressionIR* node) {
  if (Match(node, String())) {
    return true;
  }
  if (Match(node, Func())) {
    const FuncIR* func = static_cast<const FuncIR*>(node);
    for (const ExpressionIR* arg : func->args()) {
      bool has_string_time = HasStringTime(arg);
      if (has_string_time) {
        return true;
      }
    }
  }
  return false;
}

// Support taking strings like "-2m" into a memory source or rolling operator.
// relative_time determines whether to add in the current compiler time or just
// use the time given by the string
StatusOr<ExpressionIR*> ConvertStringTimesRule::ConvertStringTimes(ExpressionIR* node,
                                                                   bool relative_time) {
  // Mem sources treat expressions differently than other nodes, so if we run into one with
  // a shared parent, we should clone it to make sure that that operator doesn't get the same
  // special case treatment of the expression.
  if (node->graph()->dag().ParentsOf(node->id()).size() > 1) {
    PL_ASSIGN_OR_RETURN(IRNode * copy, node->graph()->CopyNode(node));
    CHECK(Match(copy, Expression()));
    node = static_cast<ExpressionIR*>(copy);
  }

  if (Match(node, String())) {
    auto str_node = static_cast<StringIR*>(node);
    PL_ASSIGN_OR_RETURN(
        int64_t time,
        ParseStringToTime(str_node, relative_time ? compiler_state_->time_now().val : 0));
    return node->graph()->CreateNode<IntIR>(node->ast(), time);
  } else if (Match(node, Func())) {
    auto func_node = static_cast<FuncIR*>(node);
    for (const auto& [idx, arg] : Enumerate(func_node->args())) {
      PL_ASSIGN_OR_RETURN(auto eval_result, ConvertStringTimes(arg, relative_time));
      if (eval_result != arg) {
        PL_RETURN_IF_ERROR(func_node->UpdateArg(idx, eval_result));
      }
    }
    return func_node;
  }
  return node;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
