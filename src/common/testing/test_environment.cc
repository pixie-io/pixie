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

#include "src/common/testing/test_environment.h"

#include "src/common/base/base.h"

namespace px {
namespace testing {

bool IsBazelEnvironment() {
  // Use TEST_SRCDIR environment variable to test whether we are running through bazel or not.
  const char* test_src_dir = std::getenv("TEST_SRCDIR");
  return (test_src_dir != nullptr);
}

std::filesystem::path TestFilePath(const std::filesystem::path& rel_path) {
  if (!IsBazelEnvironment()) {
    LOG_FIRST_N(WARNING, 1)
        << "This test uses static test files, but is not being run through bazel. "
           "It will only run correctly from repo ToT.";
  }

  return rel_path;
}

std::filesystem::path BazelBinTestFilePath(const std::filesystem::path& rel_path) {
  if (!IsBazelEnvironment()) {
    LOG_FIRST_N(WARNING, 1)
        << "This test uses bazel-generated test files, but is not being run through bazel. "
           "It will only run correctly from repo ToT.";
    return std::filesystem::path("bazel-bin") / rel_path;
  }
  return rel_path;
}

}  // namespace testing
}  // namespace px
