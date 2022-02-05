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

#pragma once

#include <string>
#include <vector>

namespace px {
namespace stirling {
namespace java {

// AgentAttacher injects a JVMTI agent into a target Java process. The agent itself is a shared
// library (.so) that will be mapped into the target process by dlopen, and it is responsible
// for interacting with Java through the JVMTI API. Because the Java attach process can take
// some time (possibly a few seconds) AgentAttacher forks and provides an API that will expose
// the status of the forked process (ultimately, we hope to see a successful agent attach).
// Because different Java processes in the wild may use different libc's, AgentAttacher will
// test a list of possible agent .so files in the namespace of the target process to find one
// that will dynamically link against the existing libc in that namespace.
class AgentAttacher {
 public:
  // On creation, AgentAttacher will fork. The child process will copy libs to the target
  // namespace, find an appropriate lib to inject (based on testing w/ dlopen), and
  // finally, invoke jattach to inject into the target Java process.
  // ... target_pid: the process we will attach to (inject the JVMTI agent into).
  // ... agent_args: passed to the agent (used here to specify a location for the symbol file).
  // ... agent_libs: a list of .so files, so we can find one that links in the target namespace.
  AgentAttacher(const int target_pid, const std::string& agent_args,
                const std::vector<std::filesystem::path>& agent_libs);

  // For use by the parent process, this checks if the child has finished & returns "true" if so.
  bool Finished();

  // Only meaningful once finished, this indicates either success or failure to attach.
  inline bool attached() const { return attached_; }

 private:
  void CopyAgentLibsOrDie();
  void SelectLibWithDLOpenOrDie();
  void AttachOrDie();

  const int target_pid_;
  const std::string& agent_args_;
  std::vector<std::filesystem::path> agent_libs_;
  int child_pid_;
  bool finished_ = false;
  bool attached_ = false;
  std::string lib_so_path_;
};

}  // namespace java
}  // namespace stirling
}  // namespace px
