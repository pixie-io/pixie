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

#include <filesystem>

#include "src/stirling/source_connectors/perf_profiler/testing/testing.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {
namespace profiler {
namespace testing {

using ::px::testing::BazelRunfilePath;
using fs_path = std::filesystem::path;

namespace {
const fs_path profiler_path_pfx = "src/stirling/source_connectors/perf_profiler";
}

// Returns a string as the flag value for the --stirling_profiler_java_agent_libs.
std::string GetAgentLibsFlagValueForTesting() {
  const fs_path path = profiler_path_pfx / "java" / "agent" / "libpx-java-agent.so";
  return BazelRunfilePath(path).string();
}

std::string GetPxJattachFlagValueForTesting() {
  const fs_path px_jattach_path = profiler_path_pfx / "java" / "px_jattach" / "px_jattach";
  return BazelRunfilePath(px_jattach_path);
}

}  // namespace testing
}  // namespace profiler
}  // namespace stirling
}  // namespace px
