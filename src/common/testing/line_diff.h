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
#include <vector>

#include <absl/strings/substitute.h>

namespace px {
namespace testing {

// Returns a text representation of the diffs between 2 sequences of text lines.
std::string Diff(const std::vector<std::string>& lhs, const std::vector<std::string>& rhs);

enum class DiffPolicy {
  // Diff all literal lines.
  kDefault,

  // Ignores blank lines.
  kIgnoreBlankLines,
};

// Returns a text representation of the line-based diffs of 2 text sequences.
std::string DiffLines(const std::string& lhs, const std::string& rhs,
                      DiffPolicy = DiffPolicy::kDefault);

}  // namespace testing
}  // namespace px
