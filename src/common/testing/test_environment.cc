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

#include <memory>

#include "src/common/base/base.h"
#include "src/common/testing/test_environment.h"

#include "tools/cpp/runfiles/runfiles.h"

namespace px {
namespace testing {

using bazel::tools::cpp::runfiles::Runfiles;

std::filesystem::path BazelRunfilePath(const std::filesystem::path& rel_path) {
  std::string error;
  std::unique_ptr<Runfiles> runfiles(Runfiles::CreateForTest(&error));
  if (!error.empty()) {
    LOG_FIRST_N(WARNING, 1) << "Failed to initialize runfiles";
    return rel_path;
  }

  std::string path = runfiles->Rlocation(std::filesystem::path("px") / rel_path.string());
  return path;
}

}  // namespace testing
}  // namespace px
