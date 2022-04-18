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

#include <gtest/gtest.h>

#include <future>
#include <thread>

#include "src/common/testing/testing.h"
#include "src/stirling/bpf_tools/task_struct_resolver.h"

namespace px {
namespace stirling {
namespace utils {

// Wrap ResolveTaskStructOffsets in a thread, to test how it works in a threaded environment.
void ResolveTaskStructOffsetsCoreThread(std::promise<StatusOr<TaskStructOffsets>> result) {
  result.set_value(ResolveTaskStructOffsetsCore());
}

// Tests the core logic. Must be run as main thread (which is the case for gtest).
TEST(ResolveTaskStructOffsets, CoreAsMainThread) {
  ASSERT_OK_AND_ASSIGN(TaskStructOffsets offsets, ResolveTaskStructOffsetsCore());

  EXPECT_NE(offsets.real_start_time_offset, 0);
  EXPECT_NE(offsets.group_leader_offset, 0);
}

// Demonstrates that core logic breaks down when run inside a thread.
TEST(ResolveTaskStructOffsets, CoreAsWorkerThread) {
  std::promise<StatusOr<TaskStructOffsets>> result_promise;
  std::future<StatusOr<TaskStructOffsets>> result_future = result_promise.get_future();
  std::thread thread(&ResolveTaskStructOffsetsCoreThread, std::move(result_promise));

  // The way we find the group_leader means the resolver won't work unless it is the main thread.
  EXPECT_NOT_OK(result_future.get());

  ASSERT_TRUE(thread.joinable());
  thread.join();
}

// Test ResolveTaskStructOffsets when run as a subprocess.
TEST(ResolveTaskStructOffsets, AsSubProcess) {
  ASSERT_OK_AND_ASSIGN(TaskStructOffsets offsets, ResolveTaskStructOffsets());

  EXPECT_NE(offsets.real_start_time_offset, 0);
  EXPECT_NE(offsets.group_leader_offset, 0);
  EXPECT_NE(offsets.exit_code_offset, 0);
}

}  // namespace utils
}  // namespace stirling
}  // namespace px
