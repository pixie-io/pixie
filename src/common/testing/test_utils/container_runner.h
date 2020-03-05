#pragma once

#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/exec/subprocess.h"

namespace pl {

// ContainerRunner runs a container.
class ContainerRunner {
 public:
  /**
   * Set-up a container runner.
   *
   * @param image Image to run.
   * @param instance_name_prefix The container instance name prefix. The instance name will
   * automatically be suffixed with a timestamp.
   * @param ready_message A pattern in the container logs that indicates that the container is
   * ready. The Run() function will not return until this pattern is observed. Leave blank to skip
   * this feature.
   */
  ContainerRunner(std::string_view image, std::string_view instance_name_prefix,
                  std::string_view ready_message)
      : image_(image), instance_name_prefix_(instance_name_prefix), ready_message_(ready_message) {}

  ~ContainerRunner() { Stop(); }

  /**
   * Run the container created by the constructor.
   *
   * @param timeout Amount of time after which the container will be killed.
   * @param options Environment variables to pass to the container (e.g. "--env=FOO=bar")
   * @return error if container fails to reach the ready state.
   */
  Status Run(int timeout = 60, const std::vector<std::string>& options = {});

  /**
   * Stops the container by sending it an interrupt signal.
   */
  void Stop();

  /**
   * The PID of the process within the container.
   */
  int process_pid() { return process_pid_; }

  /**
   * Instance name of the container.
   */
  std::string_view container_name() { return std::string_view(container_name_); }

 private:
  // Image to run.
  const std::string image_;

  // The instance of the container will have a name that starts with this prefix.
  // A timestamp is appended to the prefix to make it unique.
  const std::string instance_name_prefix_;

  // A message in the container logs that indicates the container is in ready state.
  // Leave blank if no such concept exists.
  const std::string ready_message_;

  // Number of seconds to wait between each attempt.
  inline static constexpr int kSleepSeconds = 1;

  // The subprocess running the container.
  SubProcess container_;

  // The instance name of the container.
  std::string container_name_;

  // The PID of the process within the container.
  int process_pid_ = -1;
};

}  // namespace pl
