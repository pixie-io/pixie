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
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/carnot/planner/compilerpb/compiler_status.pb.h"
#include "src/common/base/status.h"
#include "src/common/base/statusor.h"

namespace px {
namespace carnot {
namespace planner {
compilerpb::CompilerErrorGroup LineColErrorPb(int64_t line, int64_t column,
                                              std::string_view message);

void AddLineColError(compilerpb::CompilerErrorGroup* error_group, int64_t line, int64_t column,
                     std::string_view message);

compilerpb::CompilerErrorGroup MergeGroups(
    const std::vector<compilerpb::CompilerErrorGroup>& groups);

Status MergeStatuses(const std::vector<Status>& statuses);

}  // namespace planner
}  // namespace carnot
}  // namespace px
