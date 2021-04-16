#pragma once

#include <csignal>

#include <string>
#include <utility>
#include <vector>

#include "src/common/base/mixins.h"
#include "src/common/base/status.h"

namespace px {

/**
 * A simple class to fork and exec a binary command.
 * Inherit from NotCopyMoveable because copying a SubProcess instance
 * after starting will kill the sub-process (because of the requirements
 * in the parent pid branch in the Start() method).
 */
class SubProcess : public NotCopyMoveable {
 public:
  /**
   * Start the command.
   *
   * @return OK if succeed, otherwise an error status.
   */
  Status Start(const std::vector<std::string>& args, bool stderr_to_stdout = false);

  /**
   * Returns true if the forked subprocess is still running.
   */
  bool IsRunning();

  /**
   * Send a signal to the process.
   */
  void Signal(int signal);

  /**
   * Kill the started process.
   */
  void Kill() { Signal(SIGKILL); }

  /**
   * Wait for the subprocess to finish, and return its exit code.
   */
  int Wait();

  /**
   * Read the child process' stdout, and append to provided string pointer.
   * Returns whatever data is available, and does not block if there is no data.
   */
  Status Stdout(std::string* out);

  int child_pid() const { return child_pid_; }

 private:
  // Handy constants to access the pipe's two file descriptor array.
  const int kRead = 0;
  const int kWrite = 1;

  int child_pid_ = -1;
  bool started_ = false;

  // This is a pipe used to fetch the child process' STDOUT.
  int pipefd_[2] = {};
};

}  // namespace px
