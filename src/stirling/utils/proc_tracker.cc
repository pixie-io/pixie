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

#include "src/stirling/utils/proc_tracker.h"

#include <utility>

#include "src/common/system/proc_parser.h"

namespace px {
namespace stirling {

void ProcTracker::Update(absl::flat_hash_set<md::UPID> upids) {
  new_upids_.clear();
  for (const auto& upid : upids) {
    auto iter = upids_.find(upid);
    if (iter != upids_.end()) {
      upids_.erase(iter);
      continue;
    }
    new_upids_.emplace(upid);
  }
  deleted_upids_ = std::move(upids_);
  upids_ = std::move(upids);
}

}  // namespace stirling
}  // namespace px
