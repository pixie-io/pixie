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

#include <sys/socket.h>
#include <sys/un.h>

#include <memory>
#include <string>
#include <vector>

namespace px {
namespace system {

/**
 * A simple wrapper of Unix domain sockets.
 *
 * Note: Not meant for use in production code. This class uses CHECKs instead of Status/error.
 */
class UnixSocket {
 public:
  UnixSocket();
  ~UnixSocket();

  void BindAndListen(const std::string& path);
  std::unique_ptr<UnixSocket> Accept(bool populate_remote_addr = true);
  void Connect(const UnixSocket& addr);
  void Close();

  int sockfd() const { return sockfd_; }
  std::string_view path() const { return std::string_view(addr_un_.sun_path); }

  ssize_t Send(std::string_view data) const;
  bool Recv(std::string* data) const;

 private:
  // This is the core constructor, which is used to internally create an empty UnixSockets.
  // In contrast, the public UnixSocket constructor always creates an initialized UnixSocket.
  // The argument is actually useless, but is used to differentiate the two constructor signatures.
  explicit UnixSocket(int internal);
  int sockfd_ = 0;

  union {
    struct sockaddr addr_;
    struct sockaddr_un addr_un_;
  };

  // Buffer size for Recv() calls.
  static constexpr int kBufSize = 128;
};

}  // namespace system
}  // namespace px
