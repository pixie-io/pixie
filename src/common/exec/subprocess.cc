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

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>

#include "src/common/base/error.h"
#include "src/common/system/config.h"
#include "src/common/system/proc_parser.h"

namespace px {

SubProcess::SubProcess(int mnt_ns_pid) : mnt_ns_pid_(mnt_ns_pid) {}

SubProcess::~SubProcess() {
  // close() might fail, but won't affect anything, so just ignore the results.
  close(pipefd_[kRead]);
  close(pipefd_[kWrite]);

  // One thought is to call Kill() here to avoid forgetting call Kill() explicitly.
  //
  // That creates confusions on its effect when dtor is invoked by the child process.
  // Kill() called inside child process is sending a signal to pid 0, which according to:
  // https://man7.org/linux/man-pages/man2/kill.2.html
  // ```
  // If pid equals 0, then sig is sent to every process in the process group of the calling process.
  // ```
  // That is equivalent to parent calling Kill().
}

namespace {

std::string MountNamespacePath(int pid) {
  return system::Config::GetInstance()
      .ToHostPath(absl::Substitute("/proc/$0/ns/mnt", pid))
      .string();
}

Status SetMountNS(int pid) {
  DCHECK_GE(pid, 0);

  const std::string mnt_ns_path = MountNamespacePath(pid);
  int fd = open(mnt_ns_path.c_str(), O_RDONLY);
  if (fd == -1) {
    return error::Internal("Could not open mount namespace path '$0'", mnt_ns_path);
  }
  if (setns(fd, 0) != 0) {
    return error::Internal("setns() failed");
  }
  return Status::OK();
}

}  // namespace

void SubProcess::SetupChild(StartOptions options) {
  DCHECK_EQ(child_pid_, 0);

  // Redirect STDOUT to pipe
  if (dup2(pipefd_[kWrite], STDOUT_FILENO) == -1) {
    LOG(ERROR) << "Could not redirect STDOUT to pipe";
    exit(1);
  }

  if (options.stderr_to_stdout) {
    if (dup2(pipefd_[kWrite], STDERR_FILENO) == -1) {
      LOG(ERROR) << "Could not redirect STDERR to pipe";
      exit(1);
    }
  }

  close(pipefd_[kRead]);   // Close read end, as read is done by parent.
  close(pipefd_[kWrite]);  // Close after being duplicated.

  if (mnt_ns_pid_ != -1) {
    auto status = SetMountNS(mnt_ns_pid_);
    if (!status.ok()) {
      LOG(ERROR) << absl::Substitute("Could not set mount namespace to pid='$0' error: $1",
                                     mnt_ns_pid_, status.ToString());
      exit(1);
    }
  }

  if (options.stop_before_exec) {
    raise(SIGSTOP);
  }
}

Status SubProcess::Start(const std::vector<std::string>& args, bool stderr_to_stdout) {
  DCHECK(!started_);
  if (started_) {
    return error::Internal("Start called twice.");
  }
  started_ = true;

  std::vector<char*> exec_args;
  exec_args.reserve(args.size() + 1);
  for (const std::string& arg : args) {
    exec_args.push_back(const_cast<char*>(arg.c_str()));
  }
  exec_args.push_back(nullptr);

  // Create the pipe, see `man pipe2` for how these 2 file descriptors are used.
  // Also set the pipe to be non-blocking, so when reading from pipe won't block.
  if (pipe2(pipefd_, O_NONBLOCK) == -1) {
    return error::Internal("Could not create pipe.");
  }

  child_pid_ = fork();
  if (child_pid_ < 0) {
    return error::Internal("Could not fork!");
  }
  // Child process.
  if (child_pid_ == 0) {
    SetupChild({.stderr_to_stdout = stderr_to_stdout, .stop_before_exec = false});

    // This will run "ls -la" as if it were a command:
    // char* cmd = "ls";
    // char* argv[3];
    // argv[0] = "ls";
    // argv[1] = "-la";
    // argv[2] = NULL;
    // execvp(cmd, argv);
    int retval = execvp(exec_args.front(), exec_args.data());

    // If all goes well with exec, we never reach here.
    DCHECK_EQ(retval, -1);
    LOG(ERROR) << absl::Substitute("exec failed! error = $0", std::strerror(errno));
    exit(1);
  } else {
    // TODO(yzhao): Move this else branch outside of this if block.

    // Wait until the execution has started.
    // Test code might still want to wait for the child process actually initiated and one can
    // interact with it.
    system::ProcParser proc_parser(system::Config::GetInstance());

    // Wait until the exe path changes. The contract of Start() is such that after the call,
    // the child process already started. We use the change of child process' exe path as the signal
    // that the child process actually already started.
    PL_ASSIGN_OR_RETURN(std::filesystem::path parent_exe_path, proc_parser.GetExePath(getpid()));
    while (proc_parser.GetExePath(child_pid_).ValueOr({}) == parent_exe_path) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    close(pipefd_[kWrite]);  // Close write end, as write is done by child.
    return Status::OK();
  }
  return Status::OK();
}

Status SubProcess::Start(const std::function<int()>& fn, StartOptions options) {
  DCHECK(!started_);
  if (started_) {
    return error::Internal("Start called twice.");
  }
  started_ = true;

  // Create the pipe, see `man pipe2` for how these 2 file descriptors are used.
  // Also set the pipe to be non-blocking, so when reading from pipe won't block.
  if (pipe2(pipefd_, O_NONBLOCK) == -1) {
    return error::Internal("Could not create pipe.");
  }

  child_pid_ = fork();
  if (child_pid_ < 0) {
    return error::Internal("Could not fork!");
  }

  // Child process.
  if (child_pid_ == 0) {
    SetupChild(options);
    exit(fn());
  }

  // Close write end, as write is done by child.
  close(pipefd_[kWrite]);
  return Status::OK();
}

bool SubProcess::IsRunning() {
  int status = -1;
  return waitpid(child_pid_, &status, WNOHANG) == 0;
}

// TODO(oazizi/yzhao): This implementation has unexpected behavior if the child pid terminates
// and is reused by the OS.
void SubProcess::Signal(int signal) {
  // See https://man7.org/linux/man-pages/man2/kill.2.html:
  // ```
  // If pid equals -1, then sig is sent to every process for which the calling process has
  // permission to send signals, except for process 1 (init), but see below.
  // ```
  // So we cannot allow sending signal to -1 as that will deliver the signal to the parent process
  // itself.
  if (child_pid_ != kUnstartedPID) {
    int status = kill(child_pid_, signal);
    if (status < 0) {
      LOG(WARNING) << absl::Substitute("Failed to send signal=$0 to pid=$1, error=$2", signal,
                                       child_pid_, std::strerror(errno));
    }
  }
}

// TODO(yzhao): Consider change SubProcess to be immutable. So that we can rely on SubProcess
// destructor to close the pipe to child process.
int SubProcess::Wait(bool close_pipe) {
  if (child_pid_ != kUnstartedPID) {
    int status = -1;
    // WUNTRACED is used such that a stopped process allows this call to return immediately.
    // See: https://stackoverflow.com/a/34845669
    waitpid(child_pid_, &status, WUNTRACED);
    if (close_pipe) {
      // Close the read endpoint of the pipe. This must happen after waitpid(), otherwise the
      // process will exits abnormally because it's STDOUT cannot be written.
      close(pipefd_[kRead]);
    }
    return status;
  }
  return 0;
}

Status SubProcess::Stdout(std::string* out) {
  char buffer[1024];

  // Try to deplete all available data from the pipe. But still proceed if there is no more data.
  int len;
  do {
    len = read(pipefd_[kRead], &buffer, sizeof(buffer));

    // Don't treat EAGAIN or EWOULDBLOCK as errors,
    // Treat them as if we've grabbed all the available data, since a future call will succeed.
    // Other errors are not recoverable, so return error.
    if (len == -1 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
      return error::System(std::strerror(errno));
    }

    if (len > 0) {
      out->append(buffer, len);
    }
  } while (len == sizeof(buffer));

  return Status::OK();
}

}  // namespace px
