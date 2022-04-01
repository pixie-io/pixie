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

using ::px::testing::BazelBinTestFilePath;

// Returns a string as the flag value for the --stirling_profiler_java_agent_libs.
std::string GetAgentLibsFlagValueForTesting() {
  using fs_path = std::filesystem::path;
  const fs_path agent_path_pfx = "src/stirling/source_connectors/perf_profiler/java/agent";
  const fs_path glibc_lib_sfx = "build-glibc/lib-px-java-agent-glibc.so";
  const fs_path musl_lib_sfx = "build-musl/lib-px-java-agent-musl.so";
  const std::string glibc_agent = BazelBinTestFilePath(agent_path_pfx / glibc_lib_sfx).string();
  const std::string musl_agent = BazelBinTestFilePath(agent_path_pfx / musl_lib_sfx).string();
  return absl::StrJoin({glibc_agent, musl_agent}, ",");
}

}  // namespace testing
}  // namespace profiler
}  // namespace stirling
}  // namespace px
