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

#include "src/stirling/utils/proc_path_tools.h"

#include "src/common/testing/test_environment.h"
#include "src/common/testing/test_utils/test_container.h"
#include "src/common/testing/testing.h"

namespace px {
namespace stirling {

using ::testing::EndsWith;
using ::testing::MatchesRegex;

TEST(ProcExeTest, Basic) {
  std::filesystem::path proc_pid = "/proc/self";

  // We expect that ProcExe resolves proc_pid to this test.
  // The regex accounts for the fact that this source code is referenced by two tests:
  //   1) proc_path_tools_test
  //   2) proc_path_tools_container_test
  const std::string kExpectedPathRegex = ".*/src/stirling/utils/proc_path_tools.*_test";

  {
    ASSERT_OK_AND_ASSIGN(std::filesystem::path proc_exe, ProcExe(proc_pid));
    EXPECT_THAT(proc_exe.string(), MatchesRegex(kExpectedPathRegex));
  }

  {
    ASSERT_OK_AND_ASSIGN(std::filesystem::path proc_exe, ProcExe(getpid()));
    EXPECT_THAT(proc_exe.string(), MatchesRegex(kExpectedPathRegex));
  }
}

// Don't run this test if bazel is in a container environment with PL_HOST_PATH,
// because it will fail. This test is meant for non-container environments,
// to ensure FilePathResolver is robust.
#ifndef CONTAINER_ENV
TEST(FilePathResolver, ResolveNonContainerPaths) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<FilePathResolver> fp_resolver,
                       FilePathResolver::Create(getpid()));

  // ResolveMountPoint
  ASSERT_OK_AND_EQ(fp_resolver->ResolveMountPoint("/"), "/");
  ASSERT_NOT_OK(fp_resolver->ResolveMountPoint("/bogus"));

  // ResolvePath
  ASSERT_OK_AND_EQ(fp_resolver->ResolvePath("/app/foo"), "/app/foo");
}
#endif

// This test works on local machines.
// If bazel is itself in a container, that container must have the following options
//    `--pid=host -v /:/host -v /sys:/sys --env PL_HOST_PATH=/host`
#ifdef CONTAINER_ENV
TEST(FilePathResolver, ResolveContainerPaths) {
  SleepContainer container;
  constexpr auto kTimeoutSeconds = std::chrono::seconds{30};
  ASSERT_OK(container.Run(kTimeoutSeconds));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<FilePathResolver> fp_resolver,
                       FilePathResolver::Create(container.process_pid()));

  // ResolveMountPoint
  ASSERT_OK_AND_THAT(fp_resolver->ResolveMountPoint("/"),
                     MatchesRegex("/var/lib/docker/overlay2/.*/merged"));
  ASSERT_NOT_OK(fp_resolver->ResolveMountPoint("/bogus"));

  // ResolvePath
  ASSERT_OK_AND_THAT(fp_resolver->ResolvePath("/app/foo"),
                     MatchesRegex("/var/lib/docker/overlay2/.*/merged/app/foo"));
}
#endif

}  // namespace stirling
}  // namespace px
