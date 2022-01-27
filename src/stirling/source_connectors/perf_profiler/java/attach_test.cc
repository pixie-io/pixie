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
#include <string>
#include <vector>

#include "src/common/exec/subprocess.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/stirling/source_connectors/perf_profiler/java/attach.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

using ::px::testing::BazelBinTestFilePath;
using ::testing::HasSubstr;

std::string GetLogin() {
  constexpr size_t kBufSize = 4096;
  char login[kBufSize];

  const int err = getlogin_r(login, kBufSize);
  const uid_t uid = getuid();

  if (err == 0) {
    return login;
  }
  if (uid == 0) {
    return "root";
  }
  return "none";
}

// This test does the following:
// 1. Starts a target Java process (the fib app).
// 2. Uses the AgentAttach class to inject a JVMTI agent (our symbolization agent).
// 3. Finds the resulting symbol file and verifies that it has some symbols.
// Before doing any of the above, we setup some file paths for the target app,
// and for AgentAttach to find the JVMTI .so libs.
TEST(JavaAgentTest, ExpectedSymbolsTest) {
  // Form the file name w/ user login to make it pedantically unique.
  // Also, this is the same as in agent_test, so we keep the test logic consistent.
  const std::string symbol_file_path_pfx = absl::StrCat("px-java-symbols-", GetLogin());
  const std::string symbol_file_path = absl::StrCat(symbol_file_path_pfx, ".bin");
  const std::string java_app_name = "fib_no_agent";

  using fs_path = std::filesystem::path;
  const fs_path java_testing_path = "src/stirling/source_connectors/perf_profiler/java/testing";
  const fs_path toy_app_path = java_testing_path / java_app_name;
  const fs_path bazel_app_path = BazelBinTestFilePath(toy_app_path);

  LOG(INFO) << "bazel_app_path: " << bazel_app_path;
  ASSERT_OK(fs::Exists(bazel_app_path));

  // Construct the a vector of strings, "libs." It is used to show the attacher where it
  // can find candidate agent.so files. The attacher will test each agent.so vs. the link
  // environment inside of the target process namespace by (2) entering that namespace
  // and (2) attempting to use a function from the lib by mapping it w/ dlopen.
  // Currently, we have a symbolization agent library for glibc and for musl.
  const fs_path lib_path_pfx = "src/stirling/source_connectors/perf_profiler/java/agent";
  const fs_path lib_path_sfx = "build/agent.so";

  const std::vector<std::string> libs = {
      std::filesystem::absolute(BazelBinTestFilePath(lib_path_pfx / "build-musl" / lib_path_sfx)),
      std::filesystem::absolute(BazelBinTestFilePath(lib_path_pfx / "build-glibc" / lib_path_sfx)),
  };
  for (const auto& lib : libs) {
    ASSERT_OK(fs::Exists(lib)) << lib;
  }

  if (fs::Exists(symbol_file_path).ok()) {
    // The symbol file is created by the Java process when the agent is attached.
    // A left over stale symbol file can cause this test to pass when it should fail.
    // Here, we prevent that from happening.
    LOG(INFO) << absl::StrFormat("Removing stale file: %s.", symbol_file_path);
    ASSERT_OK(fs::Remove(symbol_file_path));
  }

  // Start the Java process (and wait for it to enter the "live" phase, because
  // you cannot inject a JVMTI agent during Java startup phase).
  SubProcess sub_process;
  const auto started = sub_process.Start({bazel_app_path});
  ASSERT_OK(started) << absl::StrFormat("Could not start Java app: %s.", java_app_name);
  std::this_thread::sleep_for(std::chrono::milliseconds(250));
  const int child_pid = sub_process.child_pid();
  LOG(INFO) << absl::StrFormat("Started Java app: %s, pid: %d.", java_app_name, child_pid);

  // Invoke the attach process by creating an attach object.
  auto attacher = java::AgentAttacher(child_pid, symbol_file_path_pfx, libs);

  // The attacher object forks. The parent process, this test, can ask the attacher object about
  // its state. Is the attacher finished (child process terminated)? attached (child process
  // returned 0 i.e. successfully attached)? The attacher uses a separate process so that it
  // does not block the parent and also to sandbox itself.
  const std::chrono::milliseconds wait_interval(1);
  std::chrono::milliseconds time_spent_waiting(0);
  while (!attacher.Finished()) {
    std::this_thread::sleep_for(wait_interval);
    time_spent_waiting += wait_interval;
  }
  EXPECT_TRUE(attacher.attached());

  // For the curious, here is how long the attacher would have blocked for. YMMV.
  LOG(INFO) << absl::StrFormat("Java attach required waiting for %d milliseconds.",
                               time_spent_waiting.count());

  // After attach is complete, wait a little more for the symbol file to materialize fully.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  const auto file_contents_or_status = ReadFileToString(symbol_file_path, std::ios_base::binary);
  ASSERT_OK_AND_ASSIGN(const auto file_contents, file_contents_or_status);

  // Check to see if the symbol file has some symbols.
  const absl::flat_hash_set<std::string> expected_symbols = {
      "([B[B)Z",          "main",        "([Ljava/lang/String;)V",
      "LJavaFib;",        "vtable stub", "(Ljava/lang/Object;)I",
      "Ljava/lang/Math;", "fib52",       "()J"};
  for (const auto& expected_symbol : expected_symbols) {
    EXPECT_THAT(file_contents, HasSubstr(expected_symbol));
  }

  // Cleanup.
  // TODO(jps): use TearDown method in test fixture. Also update agent_test.
  if (fs::Exists(symbol_file_path).ok()) {
    LOG(INFO) << "Removing symbol file: " << symbol_file_path;
    ASSERT_OK(fs::Remove(symbol_file_path));
  }
}

}  // namespace stirling
}  // namespace px
