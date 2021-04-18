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

#include <netinet/in.h>

#include <memory>
#include <string>
#include <vector>

namespace px {
namespace system {

/**
 * A simple wrapper of the syscalls for IPv4 UDP socket.
 *
 * Note: Not meant for use in production code. This class uses CHECKs instead of Status/error.
 */
class UDPSocket {
 public:
  UDPSocket();
  ~UDPSocket();

  void BindAndListen(int port = 0);
  void Close();

  int sockfd() const { return sockfd_; }
  const struct sockaddr_in sockaddr() const { return addr_; }
  const struct in_addr& addr() const { return addr_.sin_addr; }
  in_port_t port() const { return addr_.sin_port; }

  /**
   * Sends data to the specified destination socket.
   */
  ssize_t SendTo(std::string_view data, const struct sockaddr_in& dst, int flags = 0) const;
  ssize_t SendMsg(std::string_view data, const struct sockaddr_in& dst, int flags = 0) const;
  ssize_t SendMMsg(std::string_view data, const struct sockaddr_in& dst, int flags = 0) const;

  /**
   * Receives data from the socket, returns the remote address from which the data is received.
   */
  struct sockaddr_in RecvFrom(std::string* data, int flags = 0) const;
  struct sockaddr_in RecvMsg(std::string* data, int flags = 0) const;
  struct sockaddr_in RecvMMsg(std::string* data, int flags = 0) const;

 private:
  int sockfd_ = 0;
  struct sockaddr_in addr_;

  static constexpr int kBufSize = 128;
};

}  // namespace system
}  // namespace px
