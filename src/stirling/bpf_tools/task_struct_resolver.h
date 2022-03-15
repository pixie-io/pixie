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

#include <string>

#include "src/common/base/base.h"

namespace px {
namespace stirling {
namespace utils {

struct TaskStructOffsets {
  uint64_t real_start_time_offset = 0;
  uint64_t group_leader_offset = 0;
  uint64_t exit_code_offset = 0;

  std::string ToString() {
    return absl::Substitute("{real_start_time=$0, group_leader=$1, exit_code=$2}",
                            real_start_time_offset, group_leader_offset, exit_code_offset);
  }

  bool operator==(const TaskStructOffsets& other) const {
    return real_start_time_offset == other.real_start_time_offset &&
           group_leader_offset == other.group_leader_offset &&
           exit_code_offset == other.exit_code_offset;
  }
};

/**
 * Uses BPF probe to try to resolve the task struct offsets of certain fields.
 * This method does not require Linux headers.
 *
 * It determines the values by searching task_struct's memory for predictable values
 * that identify the location of the desired members.
 *
 * In particular, we look for the offset of two values:
 * 1) real_start_time/start_boottime (name changed in Linux 5.5)
 * 2) group_leader
 *
 * The reason we need these offsets is so we can access task->group_leader->real_start_time.
 *
 * We find real_start_time by cross-checking the time value against the value under /proc
 *
 * We find the group_leader by probing the main thread and looking for a pointer to self.
 * This works because the leader of a task with a single thread is itself.
 */
StatusOr<TaskStructOffsets> ResolveTaskStructOffsets();

/**
 * The core logic for ResolveTaskStructOffsets.
 * This is exposed for testing purposes only.
 */
StatusOr<TaskStructOffsets> ResolveTaskStructOffsetsCore();

}  // namespace utils
}  // namespace stirling
}  // namespace px
