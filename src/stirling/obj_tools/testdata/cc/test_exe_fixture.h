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

#include <filesystem>

#include "src/common/exec/exec.h"
#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace obj_tools {

// Holds a reference to the :test_exe, so that it's easier for tests to invoke the binary.
class TestExeFixture {
 public:
  static constexpr char kTestExePath[] = "src/stirling/obj_tools/testdata/cc/test_exe_/test_exe";

  const std::filesystem::path& Path() const { return test_exe_path_; }

  Status Run() const {
    auto stdout_or = Exec(test_exe_path_);
    return stdout_or.status();
  }

 private:
  const std::filesystem::path test_exe_path_ = testing::BazelRunfilePath(kTestExePath);
};

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
