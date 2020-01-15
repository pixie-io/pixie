#pragma once

#include <csignal>

#include <string>
#include <utility>
#include <vector>

#include "src/common/base/status.h"

namespace pl {

/**
 * @brief A simple class to fork and exec a binary command.
 */
class SubProcess {
 public:
  /**
   * @brief Start the command.
   *
   * @return OK if succeed, otherwise an error status.
   */
  Status Start(const std::vector<std::string>& args);

  /**
   * @brief Send a signal to the process.
   */
  void Signal(int signal);

  /**
   * @brief Kill the started process.
   */
  void Kill() { Signal(SIGKILL); }

  /**
   * @brief Wait for the subprocess to finish, and return its exit code.
   */
  int Wait();

  /**
   * @brief Return string from the child process' stdout.
   * Returns whatever data is available, and does not block if there is no data.
   */
  std::string Stdout();

  int child_pid() const { return child_pid_; }

 private:
  // Handy constants to access the pipe's two file descriptor array.
  const int kRead = 0;
  const int kWrite = 1;

  int child_pid_ = -1;

  // This is a pipe used to fetch the child process' STDOUT.
  int pipefd_[2] = {};
};

}  // namespace pl
