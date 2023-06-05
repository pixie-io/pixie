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

#include "src/stirling/utils/detect_application.h"

#include <string>
#include <vector>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/node_14_18_1_alpine_container.h"
#include "src/stirling/utils/proc_path_tools.h"

namespace px {
namespace stirling {

using ::px::system::ProcParser;
using ::testing::StrEq;

// Tests that the mntexec cli can execute into the alpine container.
TEST(AlpineNodeExecTest, MountNSSubprocessWorks) {
  ::px::stirling::testing::Node14_18_1AlpineContainer node_server;
  node_server.Run(std::chrono::seconds{60});
  pid_t node_server_pid = node_server.process_pid();

  ProcParser proc_parser;
  ASSERT_OK_AND_ASSIGN(std::filesystem::path exe, proc_parser.GetExePath(node_server_pid));

  SubProcess proc(node_server_pid);
  ASSERT_OK(proc.Start({exe.string(), "--version"}));
  ASSERT_EQ(proc.Wait(/*close_pipe*/ false), 0) << "Subprocess' exit code should be 0";

  std::string node_proc_stdout;
  ASSERT_OK(proc.Stdout(&node_proc_stdout));
  EXPECT_THAT(node_proc_stdout, StrEq("v14.18.1\n"));
}

}  // namespace stirling
}  // namespace px
