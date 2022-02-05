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
#include "src/common/system/scoped_namespace.h"
#include "src/stirling/source_connectors/perf_profiler/java/attach.h"

extern "C" {
// NOLINTNEXTLINE: build/include_subdir
#include "jattach.h"
}

namespace px {
namespace stirling {
namespace java {

namespace {
StatusOr<int> GetNSPid(const int pid) {
  const auto& proc_parser = system::ProcParser(system::Config::GetInstance());
  std::vector<std::string> ns_pids;
  PL_RETURN_IF_ERROR(proc_parser.ReadNSPid(pid, &ns_pids));
  const std::string& ns_pid_string = ns_pids.back();
  const int32_t ns_pid = std::stol(ns_pid_string);
  return ns_pid;
}

bool TestDLOpen(const std::string& so_lib_file_path) {
  PL_EXIT_IF_ERROR(fs::Exists(so_lib_file_path));

  // Reset the error message in the dynamic linking library. We do this prior to any call to
  // dlxyz() as general good practice.
  dlerror();
  void* h = dlopen(so_lib_file_path.c_str(), RTLD_LAZY);

  if (h == nullptr) {
    VLOG(1) << absl::Substitute("TestDLOpen(), dlopen() failure: $0.", dlerror());
    return false;
  }

  dlerror();
  auto test_fn = (uint64_t(*)(void))dlsym(h, "PixieJavaAgentTestFn");

  if (test_fn == nullptr) {
    // Keeping this in the logs as a warning because we expect to not link (above) vs. not see the
    // test function succeed. If we see this message, it is a strange outcome.
    LOG(WARNING) << absl::Substitute("TestDLOpen(), dlsym() failure: $0.", dlerror());
    return false;
  }

  // TODO(jps): Move expected_test_fn_result to "shared."
  constexpr uint64_t expected_test_fn_result = 42;
  const uint64_t observed_test_fn_result = test_fn();

  if (observed_test_fn_result != expected_test_fn_result) {
    // Keeping this in the logs, same reasoning as for test_fn == nullptr.
    char const* const msg = "TestDLOpen(), test function returned: $0, expected $1.";
    LOG(WARNING) << absl::Substitute(msg, observed_test_fn_result, expected_test_fn_result);
    return false;
  }
  VLOG(1) << absl::Substitute("TestDLOpen(): Success for $0.", so_lib_file_path);
  return true;
}
}  // namespace

void AgentAttacher::CopyAgentLibsOrDie() {
  // This is only invoked if the target pid is in a subordinate namespace.
  // Here, we copy the .so libs into the tmp path in that mount namespace.
  // We also change the file ownership such that the target process sees itself as the owner
  // of the file (necessary because some Java versions may refuse to inject an agent otherwise).

  // First we get the uid & gid of the target process. Will need these to downgrade the
  // uid & gid of the file that we create (we are root).
  const std::string proc_path = absl::Substitute("/proc/$0", target_pid_);
  const struct stat sb = fs::Stat(proc_path).ConsumeValueOrDie();
  const uid_t uid = sb.st_uid;
  const gid_t gid = sb.st_gid;

  // It will be ok to overwrite existing.
  const auto copy_options = std::filesystem::copy_options::overwrite_existing;

  // Copy each file and downgrade ownership.
  for (const std::filesystem::path& src_path : agent_libs_) {
    const std::string basename = std::filesystem::path(src_path).filename();
    const std::string dst_path = absl::Substitute("/proc/$0/root/tmp/$1", target_pid_, basename);

    PL_EXIT_IF_ERROR(fs::Copy(src_path, dst_path, copy_options));
    PL_EXIT_IF_ERROR(fs::Chown(dst_path, uid, gid));
  }

  for (std::filesystem::path& agent_lib : agent_libs_) {
    // Mutate the values in agent_libs_ so that later, when we enter the namespace
    // of the target process, we use the correctly scoped file path.
    const std::string basename = std::filesystem::path(agent_lib).filename();
    agent_lib = absl::Substitute("/tmp/$0", basename);
  }
}

void AgentAttacher::SelectLibWithDLOpenOrDie() {
  // Enter pid & mount namespace for target pid so that use of dlopen correctly links
  // vs. the available libs in that namespace.
  PL_ASSIGN_OR(std::unique_ptr<system::ScopedNamespace> pid_scoped_namespace,
               system::ScopedNamespace::Create(target_pid_, "pid"),
               { LOG(FATAL) << "Could not enter pid namespace."; });
  PL_ASSIGN_OR(std::unique_ptr<system::ScopedNamespace> mnt_scoped_namespace,
               system::ScopedNamespace::Create(target_pid_, "mnt"),
               { LOG(FATAL) << "Could not enter mnt namespace."; });

  for (const std::filesystem::path& lib : agent_libs_) {
    // Set the member lib_so_path_, then test it with dlopen().
    lib_so_path_ = lib;

    if (TestDLOpen(lib_so_path_)) {
      // Success.
      char const* const msg = "SelectLibWithDLOpenOrDie(). Tested: $0, success.";
      VLOG(1) << absl::Substitute(msg, lib_so_path_);
      return;
    }
    // Failure; move on to the next candidate lib.
    VLOG(1) << absl::Substitute("SelectLibWithDLOpenOrDie(). Tested: $0, failure.", lib_so_path_);
  }

  // All the libs failed: die now.
  LOG(FATAL) << "SelectLibWithDLOpenOrDie(). Could not find a valid px java agent lib.";
}

void AgentAttacher::AttachOrDie() {
  constexpr int argc = 4;
  const char* argv[argc] = {"load", lib_so_path_.c_str(), "true", agent_args_.c_str()};
  const int r = jattach(target_pid_, argc, argv);
  char const* const msg = "AgentAttacher finished. pid: $0, lib: $1, exit code: $2";
  LOG(INFO) << absl::Substitute(msg, target_pid_, lib_so_path_, r);
  std::exit(r);
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

AgentAttacher::AgentAttacher(const int target_pid, const std::string& agent_args,
                             const std::vector<std::filesystem::path>& agent_libs)
    : target_pid_(target_pid), agent_args_(agent_args), agent_libs_(agent_libs) {
  child_pid_ = fork();

  if (child_pid_ != 0) {
    // Parent process: return immediately.
    return;
  }

  // NB: From here on, we are in the child process.
  // It is "ok to die" because that just tells the parent that attach has failed.

  const int target_ns_pid = GetNSPid(target_pid_).ConsumeValueOrDie();
  char const* const msg = "AgentAttacher(). target_pid: $0, target_ns_pid: $1.";
  VLOG(1) << absl::Substitute(msg, target_pid_, target_ns_pid);

  if (target_ns_pid != target_pid_) {
    // If the target Java process is in a subordinate namespace, we copy the agent libs into the
    // tmp path inside of that namespace (so that they are visible to the target process).
    // We skip the copy otherwise, to not pollute the tmp space.
    CopyAgentLibsOrDie();
  }

  // We compile one agent lib for each libc implementation we are aware of and support (currently,
  // those are glibc & musl). SelectLibWithDLOpenOrDie() enters the namespace of the target process
  // and tests each lib until it finds one that it can successfully dlopen and use.
  SelectLibWithDLOpenOrDie();

  // Having determined a lib that is likely to link in the target environment, we invoke the
  // well crafted code in jattach (https://github.com/apangin/jattach) to inject the library
  // as a JVMTI agent.
  AttachOrDie();
}

}  // namespace java
}  // namespace stirling
}  // namespace px
