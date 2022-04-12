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

#include "src/common/exec/subprocess.h"

#include <thread>

#include "src/common/testing/testing.h"

namespace px {

// Tests that child can be stopped before executing the command, and can be resumed afterwards.
TEST(SubProcessTest, StopBeforeExecAndResume) {
  SubProcess subprocess;
  ASSERT_OK(subprocess.Start(
      []() -> int { return 123; },
      // This instructs the child to raise SIGSTOP to itself, which pauses the child process.
      {.stop_before_exec = true}));
  ASSERT_TRUE(subprocess.IsRunning());
  std::this_thread::sleep_for(std::chrono::seconds(3));
  ASSERT_TRUE(subprocess.IsRunning());
  // The child process is paused by SIGSTOP before executing, so we need to resume it with SIGCONT
  // signal.
  subprocess.Signal(SIGCONT);
  int status = subprocess.Wait();
  EXPECT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 123);
}

// Tests that kill multiple times the child process wont cause abnormal crash in the parent.
TEST(SubProcessTest, KillMultipleTimes) {
  SubProcess subprocess;
  ASSERT_OK(subprocess.Start(
      []() -> int {
        std::this_thread::sleep_for(std::chrono::seconds(1000));
        return 0;
      },
      /*options*/ {}));
  ASSERT_TRUE(subprocess.IsRunning());
  std::this_thread::sleep_for(std::chrono::seconds(3));
  ASSERT_TRUE(subprocess.IsRunning()) << "Should be still running after 3 seconds";

  subprocess.Kill();
  subprocess.Kill();
  int status = subprocess.Wait();
  EXPECT_TRUE(WIFSIGNALED(status));
  EXPECT_EQ(WTERMSIG(status), 9);

  subprocess.Kill();
  status = subprocess.Wait();
  EXPECT_EQ(status, -1) << "Wait returns an invalid status code, as the process already exited";
}

// Tests that kill a unstarted subprocess causes crash.
TEST(SubProcessTest, KillUnstartedProcess) {
  SubProcess subprocess;
  subprocess.Kill();
#ifndef NDEBUG
  EXPECT_DEATH(subprocess.Wait(), "Child process has not been started");
#endif
}

}  // namespace px
