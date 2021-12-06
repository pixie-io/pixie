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
  // TODO(jps): move this file to be a peer to agent.cc in the directory structure.
  const std::string kJavaAppName = "fib";
  const std::string kSymbolFilePath = "/tmp/px-java-symbolization-agent.bin";

  const std::filesystem::path kPathToJavaTesting =
      "src/stirling/source_connectors/perf_profiler/java/testing";
  const std::filesystem::path kToyAppPath = kPathToJavaTesting / kJavaAppName;
  const std::filesystem::path bazel_app_path = BazelBinTestFilePath(kToyAppPath);
  ASSERT_OK(fs::Exists(bazel_app_path));

  SubProcess sub_process;
  ASSERT_OK(sub_process.Start({bazel_app_path})) << "Could not start Java app: " << kJavaAppName;
  std::this_thread::sleep_for(std::chrono::seconds(5));

  const auto r = ReadFileToString(kSymbolFilePath, std::ios_base::binary);
  ASSERT_OK(r);
  const auto s = r.ValueOrDie();

  const absl::flat_hash_set<std::string> expected_symbols = {
      "([B[B)Z",          "main",        "([Ljava/lang/String;)V",
      "LJavaFib;",        "vtable stub", "(Ljava/lang/Object;)I",
      "Ljava/lang/Math;", "fib52",       "()J"};
  for (const auto& expected_symbol : expected_symbols) {
    EXPECT_THAT(s, HasSubstr(expected_symbol));
  }
}

}  // namespace stirling
}  // namespace px
