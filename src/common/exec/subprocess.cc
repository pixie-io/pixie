#include "src/common/exec/subprocess.h"

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>

#include "src/common/base/error.h"
#include "src/common/system/config.h"
#include "src/common/system/proc_parser.h"

namespace pl {

Status SubProcess::Start(const std::vector<std::string>& args, bool stderr_to_stdout) {
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
    // Redirect STDOUT to pipe
    if (dup2(pipefd_[kWrite], STDOUT_FILENO) == -1) {
      return error::Internal("Could not redirect STDOUT to pipe");
    }
    if (stderr_to_stdout) {
      if (dup2(pipefd_[kWrite], STDERR_FILENO) == -1) {
        return error::Internal("Could not redirect STDERR to pipe");
      }
    }

    close(pipefd_[kRead]);   // Close read end, as read is done by parent.
    close(pipefd_[kWrite]);  // Close after being duplicated.

    // This will run "ls -la" as if it were a command:
    // char* cmd = "ls";
    // char* argv[3];
    // argv[0] = "ls";
    // argv[1] = "-la";
    // argv[2] = NULL;
    // execvp(cmd, argv);
    int retval = execvp(exec_args.front(), exec_args.data());
    if (retval == -1) {
      exit(1);
    }
  } else {
    // We wanted to wait until the execution has started.
    // Test code might still want to wait for the child process actually initiated and one can
    // interact with it.
    system::ProcParser proc_parser(system::Config::GetInstance());

    // Wait until the exe path changed.
    while (proc_parser.GetExePath(child_pid_) == proc_parser.GetExePath(getpid())) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    close(pipefd_[kWrite]);  // Close write end, as write is done by child.
    return Status::OK();
  }
  return Status::OK();
}

// TODO(oazizi/yzhao): This implementation has unexpected behavior if the child pid terminates
// and is reused by the OS.
void SubProcess::Signal(int signal) {
  if (child_pid_ != -1) {
    kill(child_pid_, signal);
  }
}

int SubProcess::Wait() {
  if (child_pid_ != -1) {
    int status = -1;
    waitpid(child_pid_, &status, WUNTRACED);
    // Close the read endpoint of the pipe. This must happen after waitpid(), otherwise the process
    // will exits abnormally because it's STDOUT cannot be written.
    close(pipefd_[kRead]);
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

}  // namespace pl
