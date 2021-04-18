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

#include "src/carnot/planner/compiler_error_context/compiler_error_context.h"

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <absl/strings/str_join.h>

namespace px {
namespace carnot {
namespace planner {
compilerpb::CompilerErrorGroup LineColErrorPb(int64_t line, int64_t column,
                                              std::string_view message) {
  compilerpb::CompilerErrorGroup error_group;
  AddLineColError(&error_group, line, column, message);
  return error_group;
}

void AddLineColError(compilerpb::CompilerErrorGroup* error_group, int64_t line, int64_t column,
                     std::string_view message) {
  compilerpb::CompilerError* err = error_group->add_errors();
  compilerpb::LineColError* lc_err_pb = err->mutable_line_col_error();
  lc_err_pb->set_line(line);
  lc_err_pb->set_column(column);
  lc_err_pb->set_message(std::string(message));
}

compilerpb::CompilerErrorGroup MergeGroups(
    const std::vector<compilerpb::CompilerErrorGroup>& groups) {
  compilerpb::CompilerErrorGroup out_error_group;
  for (const compilerpb::CompilerErrorGroup& group : groups) {
    for (const auto& error : group.errors()) {
      *(out_error_group.add_errors()) = error;
    }
  }
  return out_error_group;
}

Status MergeStatuses(const std::vector<Status>& statuses) {
  // If statuses is empty, then we return OK.
  if (statuses.empty()) {
    return Status::OK();
  }
  std::vector<compilerpb::CompilerErrorGroup> error_group_pbs;
  std::vector<std::string> messages;
  for (const auto& s : statuses) {
    messages.push_back(s.msg());
    if (!s.has_context() || !s.context()->Is<compilerpb::CompilerErrorGroup>()) {
      continue;
    }
    compilerpb::CompilerErrorGroup cur_error;
    s.context()->UnpackTo(&cur_error);
    error_group_pbs.push_back(cur_error);
  }
  if (error_group_pbs.empty()) {
    return Status(statuses[0].code(), absl::StrJoin(messages, "\n"));
  }
  return Status(statuses[0].code(), absl::StrJoin(messages, "\n"),
                std::make_unique<compilerpb::CompilerErrorGroup>(MergeGroups(error_group_pbs)));
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
