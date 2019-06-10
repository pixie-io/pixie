#pragma once

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
  explicit SubProcess(std::vector<std::string> args);

  /**
   * @brief Start the command.
   *
   * @return OK if succeed, otherwise an error status.
   */
  Status Start();

  /**
   * @brief Kill the started process.
   */
  void Kill();

  /**
   * @brief Wait for the subprocess to finish, and return its exit code.
   */
  int Wait();

 private:
  const std::vector<std::string> args_;
  int child_pid_;
};

}  // namespace pl
