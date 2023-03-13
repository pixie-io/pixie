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

// A macro that sets a variable, but then restores it to its original value after the scope exits.
// Useful for setting a flag for the duration of a test.
#define PX_SET_FOR_SCOPE(var, val) \
  auto var##__orig = var;          \
  DEFER(var = var##__orig);        \
  var = val;

namespace px {
namespace testing {

/**
 * Returns the path to a runfile, specified by a path relative to ToT.
 * Path is valid when run through bazel.
 */
std::filesystem::path BazelRunfilePath(const std::filesystem::path& rel_path);

}  // namespace testing
}  // namespace px
