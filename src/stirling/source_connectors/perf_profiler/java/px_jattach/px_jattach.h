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

#include "src/common/system/clock.h"
#include "src/stirling/upid/upid.h"

namespace px {
namespace stirling {
namespace java {

// AgentAttachApp injects a JVMTI agent into a target Java process. The agent itself is a shared
// library (.so) that will be mapped into the target process by dlopen, and it is responsible
// for interacting with Java through the JVMTI API. Because the Java attach process can take
// some time (possibly a few seconds) AgentAttachApp is run as a sub-process from the main Stirling
// process. Because different Java processes in the wild may use different libcs, it will
// test a list of possible agent .so files in the namespace of the target process to find one
// that will dynamically link against the existing libc in that namespace.
class AgentAttachApp {
 public:
  AgentAttachApp(const struct upid_t& upid, const std::vector<std::filesystem::path>& agent_libs);
  void Attach();

 private:
  void SetTargetUIDAndGIDOrDie();
  void CreateArtifactsPathOrDie();
  void CopyAgentLibsOrDie();
  void SelectLibWithDLOpenOrDie();
  void AttachOrDie();

  const struct upid_t target_upid_;
  std::vector<std::filesystem::path> agent_libs_;
  std::string lib_so_path_;
  uid_t target_uid_;
  gid_t target_gid_;
};

}  // namespace java
}  // namespace stirling
}  // namespace px
