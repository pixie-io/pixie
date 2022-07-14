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

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/system/proc_parser.h"
#include "src/common/system/socket_info.h"

namespace px {
namespace stirling {

/**
 * SocketResolver tries to determine the socket inode number of a given a PID and FD.
 *
 * Because FDs can be reused and there is no (known) way to probe Linux for the time when a socket
 * was created, this module monitors the /proc filesystem over time, sampling the inode number of
 * the PID+FD.
 *
 * If the inode number is stable across samples, then this module can return the inode number for a
 * query within the valid time window.
 */
class FDResolver {
 public:
  /**
   * Creates a SocketResolver for the PID and FD.
   *
   * @param proc_parser Pointer to a /proc parser which is used to read the FD info
   * @param pid The PID to monitor.
   * @param fd The FD of the PID to monitor.
   */
  FDResolver(system::ProcParser* proc_parser, int pid, int fd);

  /**
   * Collects the first sample from Linux, to begin the tracking process.
   */
  bool Setup();

  /**
   * Collects another sample from Linux to update its view of the PID+FD.
   */
  bool Update();

  /**
   * Queries the SocketResolver to see whether it knows the inode number for a given time.
   *
   * @param time The time at which the socket inode number is requested.
   * @return the socket inode number, if it is known. Otherwise, returns std::nullopt_t.
   */
  std::optional<std::string_view> InferFDLink(
      std::chrono::time_point<std::chrono::steady_clock> time);

  /**
   * Whether the tracking is still active. Once inactive, then resolver will not collect any new
   * information about the FD being tracked, and will not learn any new information.
   *
   * @return Whether tracking is active.
   */
  bool IsActive() { return active_; }

  std::string DebugInfo() {
    return absl::Substitute("pid=$0 fd=$1 t=$2-$3 active=$4 fdlink=$5", pid_, fd_,
                            first_timestamp_.time_since_epoch().count(),
                            last_timestamp_.time_since_epoch().count(), active_, fd_link_);
  }

 private:
  system::ProcParser* proc_parser_;
  int pid_;
  int fd_;

  bool active_;

  // The FD link contents that at setup time.
  // Potentially contains the socket inode number.
  std::string fd_link_;

  // This is a time after the fd_link_ was recorded.
  // Used to determine the valid time window for the link, if the link is still
  // the same on a subsequent sample.
  std::chrono::time_point<std::chrono::steady_clock> first_timestamp_;
  std::chrono::time_point<std::chrono::steady_clock> last_timestamp_;
};

}  // namespace stirling
}  // namespace px
