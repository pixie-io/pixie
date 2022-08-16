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
  kJava,
};

// Returns the application of the input executable.
Application DetectApplication(const std::filesystem::path& exe);

// Describes a semantic versioning number.
struct SemVer {
  int major = 0;
  int minor = 0;
  int patch = 0;

  std::string ToString() const {
    return absl::Substitute("major=$0 minor=$1 patch=$2", major, minor, patch);
  }
};

// Returns true if lhs is before rhs in lexical order.
bool operator<(const SemVer& lhs, const SemVer& rhs);
bool operator==(const SemVer& lhs, const SemVer& rhs);
bool operator<=(const SemVer& lhs, const SemVer& rhs);

// Returns semantic version from a version string that include a substring that represent the
// semantic version numbers. For example, v1.1.0 returns SemVer{1, 1, 0}.
// Note: the argument is chosen because the implementation uses std::regex_search which does not
// accept std::string_view.
// If not strict, then patch is not mandatory and patch parsing failure is tolerated.
// e.g. go1.18rc1 -> SemVer{1, 18, 0}.
StatusOr<SemVer> GetSemVer(const std::string& version, const bool strict = true);

}  // namespace stirling
}  // namespace px
