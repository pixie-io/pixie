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
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

using ::px::testing::BazelBinTestFilePath;
using ::testing::HasSubstr;

TEST(JavaAgentTest, ExpectedSymbolsTest) {
  // Reconstruct the symbolization log file name that is specified in the bazel BUILD file
  // for the java test app. In the BUILD file, we use whoami to uniquify the file name.
  // Our motivation here is that previously, tests from different users collided because
  // they referenced the same symbolization log file.
  // TODO(jps): consider building the Java test app "fib" inside of a container (i.e. to explicitly
  // *not* use java_binary() in Bazel). This would allow the agent test (this code) to completely
  // own the symbol file name, i.e. to coordinate between logic in the BUILD file and this code.
  constexpr std::string_view kSymbolFilePath = "px-java-symbols.bin";
  constexpr std::string_view kJavaAppName = "fib_with_agent";

  using fs_path = std::filesystem::path;
  const fs_path kPathToJavaTesting = "src/stirling/source_connectors/perf_profiler/testing/java";
  const fs_path kToyAppPath = kPathToJavaTesting / kJavaAppName;
  const fs_path kBazelAppPath = BazelBinTestFilePath(kToyAppPath);
  ASSERT_OK(fs::Exists(kBazelAppPath));

  if (fs::Exists(kSymbolFilePath).ok()) {
    // The symbol file is created by the Java process when the agent is attached.
    // A left over stale symbol file can cause this test to pass when it should fail.
    // Here, we prevent that from happening.
    LOG(INFO) << "Removing stale file: " << kSymbolFilePath << ".";
    ASSERT_OK(fs::Remove(kSymbolFilePath));
  }

  SubProcess sub_process;
  ASSERT_OK(sub_process.Start({kBazelAppPath})) << "Could not start Java app: " << kJavaAppName;
  std::this_thread::sleep_for(std::chrono::seconds(5));

  const auto r = ReadFileToString(std::string(kSymbolFilePath), std::ios_base::binary);
  ASSERT_OK(r);
  const auto s = r.ValueOrDie();

  const absl::flat_hash_set<std::string> expected_symbols = {
      "()J",
      "fib52",
      "([B[B)Z",
      "LJavaFib;",
      "call_stub",
      "Interpreter",
      "vtable stub",
      "Ljava/lang/Math;",
      "()Ljava/lang/String;",
      "(Ljava/lang/Object;)I",
  };
  for (const auto& expected_symbol : expected_symbols) {
    EXPECT_THAT(s, HasSubstr(expected_symbol));
  }
  if (fs::Exists(kSymbolFilePath).ok()) {
    LOG(INFO) << "Removing symbol file: " << kSymbolFilePath;
    ASSERT_OK(fs::Remove(kSymbolFilePath));
  }
}

}  // namespace stirling
}  // namespace px
