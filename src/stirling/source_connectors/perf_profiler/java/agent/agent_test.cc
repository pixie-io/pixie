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

#include <string>

#include "src/common/exec/subprocess.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/stirling/source_connectors/perf_profiler/java/agent/agent_hash.h"
#include "src/stirling/source_connectors/perf_profiler/java/agent/raw_symbol_update.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

using ::px::testing::BazelRunfilePath;
using ::testing::HasSubstr;

TEST(JavaAgentTest, ExpectedSymbolsTest) {
  constexpr std::string_view kJavaAppName = "profiler_test_with_agent";

  using fs_path = std::filesystem::path;
  const fs_path kPathToJavaTesting = "src/stirling/source_connectors/perf_profiler/testing/java";
  const fs_path kToyAppPath = kPathToJavaTesting / kJavaAppName;
  const fs_path kBazelAppPath = BazelRunfilePath(kToyAppPath);
  ASSERT_TRUE(fs::Exists(kBazelAppPath));

  const fs_path artifacts_path = absl::Substitute("java-agent-test-$0", PX_JVMTI_AGENT_HASH);
  const fs_path symbol_file_path = artifacts_path / java::kBinSymbolFileName;

  if (fs::Exists(artifacts_path)) {
    // The symbol file is created by the Java process when the agent is attached.
    // A left over stale symbol file can cause this test to pass when it should fail.
    // Here, we prevent that from happening.
    char const* const stale_path_msg = "Removing stale symbolization artifacts path: $0.";
    LOG(WARNING) << absl::Substitute(stale_path_msg, artifacts_path.string());
    ASSERT_OK(fs::RemoveAll(artifacts_path));
  }
  ASSERT_FALSE(fs::Exists(artifacts_path));
  ASSERT_OK(fs::CreateDirectories(artifacts_path));

  SubProcess sub_process;
  ASSERT_OK(sub_process.Start({kBazelAppPath})) << "Could not start Java app: " << kJavaAppName;
  std::this_thread::sleep_for(std::chrono::seconds(5));

  const auto r = ReadFileToString(std::string(symbol_file_path), std::ios_base::binary);
  ASSERT_OK(r);
  const auto s = r.ValueOrDie();

  const absl::flat_hash_set<std::string> expected_symbols = {
      "()J",
      "leaf1x",
      "leaf2x",
      "([B[B)Z",
      "call_stub",
      "Interpreter",
      "vtable stub",
      "LProfilerTest;",
      "Ljava/lang/Math;",
      "()Ljava/lang/String;",
      "(Ljava/lang/Object;)I",
  };
  for (const auto& expected_symbol : expected_symbols) {
    EXPECT_THAT(s, HasSubstr(expected_symbol));
  }
  if (fs::Exists(artifacts_path)) {
    char const* const remove_msg = "Removing symbolization artifacts path: $0.";
    LOG(INFO) << absl::Substitute(remove_msg, artifacts_path.string());
    ASSERT_OK(fs::RemoveAll(artifacts_path));
  }
}

}  // namespace stirling
}  // namespace px
