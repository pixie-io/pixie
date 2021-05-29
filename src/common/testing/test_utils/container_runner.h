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

#include <filesystem>
#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/exec/subprocess.h"

namespace px {

// ContainerRunner runs a container.
class ContainerRunner {
 public:
  /**
   * Set-up a container runner with image from docker registry.
   *
   * @param image Image to run.
   * @param instance_name_prefix The container instance name prefix. The instance name will
   * automatically be suffixed with a timestamp.
   * @param ready_message A pattern in the container logs that indicates that the container is
   * ready. The Run() function will not return until this pattern is observed. Leave blank to skip
   * this feature.
   */
  ContainerRunner(std::string_view image, std::string_view instance_name_prefix,
                  std::string_view ready_message);

  /**
   * Set-up a container runner from local tarball image.
   *
   * @param image_tar Image tarball.
   * @param instance_name_prefix The container instance name prefix. The instance name will
   * automatically be suffixed with a timestamp.
   * @param ready_message A pattern in the container logs that indicates that the container is
   * ready. The Run() function will not return until this pattern is observed. Leave blank to skip
   * this feature.
   */
  ContainerRunner(std::filesystem::path image_tar, std::string_view instance_name_prefix,
                  std::string_view ready_message);

  ~ContainerRunner();

  /**
   * Run the container created by the constructor.
   *
   * @param timeout Amount of time after which the container will be killed.
   * @param options Environment variables to pass to the container (e.g. "--env=FOO=bar")
   * @return error stdout of the container, or error if container fails to reach the ready state.
   */
  StatusOr<std::string> Run(const std::chrono::seconds& timeout = std::chrono::seconds{60},
                            const std::vector<std::string>& options = {},
                            const std::vector<std::string>& args = {});

  /**
   * Wait for container to terminate.
   */
  void Wait();

  /**
   * The PID of the process within the container.
   * Note that short-lived containers may return -1,
   * as there may not have been enough time to grab the PID after running.
   */
  int process_pid() {
    // If this ever returns -1, then the caller is either using it incorrectly,
    // or a race has occurred in obtaining the PID. Either way, DCHECK so we can track it down.
    DCHECK_NE(process_pid_, -1);
    return process_pid_;
  }

  /**
   * Instance name of the container.
   */
  std::string_view container_name() { return std::string_view(container_name_); }

 private:
  /**
   * Stops the container by sending it an interrupt signal.
   */
  void Stop();

  // Image to run.
  std::string image_;

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

}  // namespace px
