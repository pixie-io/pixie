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
#include <string>

#include "src/common/base/base.h"

namespace px {
namespace stirling {

// Describes the application type of an executable.
enum class Application {
  kUnknown,
  kNode,
};

// Returns the application of the input executable.
Application DetectApplication(const std::filesystem::path& exe);

// Returns the output of the <executable> <version flag>.
StatusOr<std::string> GetVersion(const std::filesystem::path& exe,
                                 std::string_view version_flag = "--version");

}  // namespace stirling
}  // namespace px
