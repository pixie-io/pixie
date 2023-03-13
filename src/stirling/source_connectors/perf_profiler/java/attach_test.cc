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
#include "src/stirling/source_connectors/perf_profiler/testing/testing.h"
#include "src/stirling/testing/common.h"

DECLARE_string(stirling_profiler_px_jattach_path);
DEFINE_string(stirling_profiler_java_agent_libs, "none", "Agent libs test arg.");

namespace px {
namespace stirling {

using ::px::stirling::profiler::testing::GetAgentLibsFlagValueForTesting;
using ::px::stirling::profiler::testing::GetPxJattachFlagValueForTesting;
using ::px::testing::BazelRunfilePath;
using ::testing::HasSubstr;

namespace {
// Uses the value in FLAGS_stirling_profiler_java_agent_libs to construct a new arg. that
// is a comma separated list of abs paths to agent libs.
StatusOr<std::string> GetAbsPathLibsArg() {
  const std::string& comma_separated_libs = FLAGS_stirling_profiler_java_agent_libs;
  const std::vector<std::string_view> rel_path_libs = absl::StrSplit(comma_separated_libs, ",");

  std::vector<std::string> abs_path_libs;
  for (const auto& rel_p : rel_path_libs) {
    PX_ASSIGN_OR_RETURN(const auto abs_p, fs::Absolute(rel_p));
    if (!fs::Exists(abs_p)) {
      return error::NotFound("Could not find: $0.", abs_p.string());
    }
    abs_path_libs.push_back(abs_p.string());
  }
  return absl::StrJoin(abs_path_libs, ",");
}
}  // namespace

// This test does the following:
// 1. Starts a target Java process (the profiler test app).
// 2. Uses the AgentAttach class to inject a JVMTI agent (our symbolization agent).
// 3. Finds the resulting symbol file and verifies that it has some symbols.
// Before doing any of the above, we setup some file paths for the target app,
// and for AgentAttach to find the JVMTI .so libs.
TEST(JavaAttachTest, ExpectedSymbolsTest) {
  // Form the file name w/ user login to make it pedantically unique.
  // Also, this is the same as in agent_test, so we keep the test logic consistent.

  constexpr std::string_view kJavaAppName = "profiler_test";

  using fs_path = std::filesystem::path;
  const fs_path java_testing_path = "src/stirling/source_connectors/perf_profiler/testing/java";
  const fs_path toy_app_path = java_testing_path / kJavaAppName;
  const fs_path bazel_test_app_path = BazelRunfilePath(toy_app_path);
  const fs_path bazel_attach_app_path = BazelRunfilePath(GetPxJattachFlagValueForTesting());

  LOG(INFO) << "bazel_attach_app_path: " << bazel_attach_app_path;
  LOG(INFO) << "bazel_test_app_path: " << bazel_test_app_path;
  ASSERT_TRUE(fs::Exists(bazel_attach_app_path));
  ASSERT_TRUE(fs::Exists(bazel_test_app_path));

  PX_SET_FOR_SCOPE(FLAGS_stirling_profiler_px_jattach_path, bazel_attach_app_path.string());
  PX_SET_FOR_SCOPE(FLAGS_stirling_profiler_java_agent_libs, GetAgentLibsFlagValueForTesting());

  // Start the Java process (and wait for it to enter the "live" phase, because
  // you cannot inject a JVMTI agent during Java startup phase).
  SubProcess sub_process;
  DEFER(sub_process.Kill());
  const auto started = sub_process.Start({bazel_test_app_path});
  ASSERT_OK(started) << absl::StrFormat("Could not start Java app: %s.", kJavaAppName);
  const uint32_t child_pid = sub_process.child_pid();
  LOG(INFO) << absl::StrFormat("Started Java app: %s, pid: %d.", kJavaAppName, child_pid);

  // Construct struct upid_t for our child process,
  // use that to construct the symbol file path.
  using ::px::system::GetPIDStartTimeTicks;
  const std::string proc_pid_path = std::string("/proc/") + std::to_string(child_pid);
  ASSERT_OK_AND_ASSIGN(const uint64_t start_time, GetPIDStartTimeTicks(proc_pid_path));
  const struct upid_t child_upid = {{child_pid}, start_time};
  const fs_path symbol_file_path = java::StirlingSymbolFilePath(child_upid);

  // Populate libs arg. with a comma separated list of abs paths to agent libs.
  ASSERT_OK_AND_ASSIGN(const auto libs_arg, GetAbsPathLibsArg());

  // Invoke the attach process by creating an attach object.
  auto attacher = java::AgentAttacher(child_upid, libs_arg);

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
  auto SymbolFileOrStatus = [&]() {
    return ReadFileToString(symbol_file_path.string(), std::ios_base::binary);
  };
  ASSERT_OK_AND_ASSIGN(const auto file_contents, SymbolFileOrStatus());

  // Check to see if the symbol file has some symbols.
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
    EXPECT_THAT(file_contents, HasSubstr(expected_symbol));
  }

  // Cleanup.
  // TODO(jps): use TearDown method in test fixture. Also update agent_test.
  const std::filesystem::path artifacts_path = java::StirlingArtifactsPath(child_upid);
  if (fs::Exists(artifacts_path)) {
    LOG(INFO) << absl::Substitute("Removing symbol artifacts path: $0.", artifacts_path.string());
    ASSERT_OK(fs::RemoveAll(artifacts_path));
  }
}

}  // namespace stirling
}  // namespace px
