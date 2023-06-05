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

#include "src/common/exec/exec.h"
#include "src/common/testing/test_environment.h"

namespace px {
namespace stirling {
namespace testing {
namespace internal {

// A helper function useful for the Nginx images below.
int32_t GetNginxWorkerPID(int32_t pid) {
  // Nginx has a master process and a worker process. We need the PID of the worker process.
  int worker_pid;
  std::string pid_str = px::Exec(absl::Substitute("pgrep -P $0", pid)).ValueOrDie();
  CHECK(absl::SimpleAtoi(pid_str, &worker_pid));
  LOG(INFO) << absl::Substitute("Worker thread PID: $0", worker_pid);
  return worker_pid;
}

}  // namespace internal
}  // namespace testing
}  // namespace stirling
}  // namespace px
