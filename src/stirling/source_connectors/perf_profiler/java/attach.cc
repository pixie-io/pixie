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

#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <utility>

#include <absl/strings/str_format.h>

#include "src/common/base/logging.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/proc_parser.h"
#include "src/common/system/proc_pid_path.h"
#include "src/common/system/scoped_namespace.h"
#include "src/stirling/source_connectors/perf_profiler/java/agent/agent_hash.h"
#include "src/stirling/source_connectors/perf_profiler/java/agent/raw_symbol_update.h"
#include "src/stirling/source_connectors/perf_profiler/java/attach.h"
#include "src/stirling/utils/proc_path_tools.h"

DEFINE_string(stirling_profiler_px_jattach_path, "/px/px_jattach", "Path to px_jattach app.");

namespace px {

using system::ProcPidRootPath;

namespace stirling {
namespace java {

namespace {
char const* const kSuffix = "px-java-symbolization-artifacts-$0-$1-$2";
}

std::filesystem::path AgentArtifactsPath(const struct upid_t& upid) {
  // This is the full agent artifacts path. Stirling needs to know this to:
  // 1. Resolve the agent artifacts path for cleanup.
  // 2. Test libs using dlopen inside of the target process mount namespace.
  const auto p = absl::Substitute(kSuffix, upid.pid, upid.start_time_ticks, PX_JVMTI_AGENT_HASH);
  return std::filesystem::path("/tmp") / p;
}

std::filesystem::path StirlingTmpPathForUPID(const struct upid_t& upid) {
  return ProcPidRootPath(upid.pid, "tmp");
}

std::filesystem::path StirlingArtifactsPath(const struct upid_t& upid) {
  const auto p = absl::Substitute(kSuffix, upid.pid, upid.start_time_ticks, PX_JVMTI_AGENT_HASH);
  return ProcPidRootPath(upid.pid, "tmp", p);
}

std::filesystem::path StirlingSymbolFilePath(const struct upid_t& upid) {
  return StirlingArtifactsPath(upid) / kBinSymbolFileName;
}

bool AgentAttacher::Finished() {
  // Only the parent process should call this method.
  DCHECK_NE(child_pid_, 0);

  if (!finished_) {
    // Check on the status of the child process, but don't block.
    // When the child process is finished, capture its return value
    // to see if the attach was successful.
    int exit_status;
    const int terminated_pid = ::waitpid(child_pid_, &exit_status, WNOHANG);

    if (terminated_pid == child_pid_) {
      const int rc = WEXITSTATUS(exit_status);
      finished_ = true;
      attached_ = rc == 0;
    }
  }
  return finished_;
}

AgentAttacher::AgentAttacher(const struct upid_t& upid, const std::string& agent_libs)
    : start_time_(std::chrono::steady_clock::now()) {
  std::string pid_arg = absl::Substitute("$0", upid.pid);
  std::string start_time_ticks_arg = absl::Substitute("$0", upid.start_time_ticks);
  std::string agent_libs_arg = agent_libs;

  char* const argv[5] = {FLAGS_stirling_profiler_px_jattach_path.data(), pid_arg.data(),
                         start_time_ticks_arg.data(), agent_libs_arg.data(), nullptr};

  child_pid_ = fork();
  if (child_pid_ != 0) {
    // Parent process: return immediately.
    return;
  }
  execv(FLAGS_stirling_profiler_px_jattach_path.data(), argv);
  LOG(FATAL) << "Should never get here.";
}

}  // namespace java
}  // namespace stirling
}  // namespace px
