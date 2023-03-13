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

#include "src/common/system/unix_socket.h"

#include <sys/socket.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "src/common/base/base.h"

namespace px {
namespace system {

UnixSocket::UnixSocket() : UnixSocket(0) {
  sockfd_ = socket(AF_UNIX, SOCK_STREAM, /*protocol*/ 0);
  CHECK(sockfd_ > 0) << "Failed to create socket, error message: " << strerror(errno);
}

UnixSocket::UnixSocket(int internal) {
  memset(&addr_un_, 0, sizeof(struct sockaddr_un));
  // Required to differentiate the private vs public UnixSocket constructor.
  PX_UNUSED(internal);
}

UnixSocket::~UnixSocket() { Close(); }

void UnixSocket::BindAndListen(const std::string& path) {
  addr_un_.sun_family = AF_UNIX;
  strncpy(addr_un_.sun_path, path.c_str(), sizeof(addr_un_.sun_path) - 1);
  CHECK(bind(sockfd_, &addr_, sizeof(struct sockaddr_un)) == 0)
      << "Failed to bind socket, error message: " << strerror(errno);

  socklen_t addr_len = sizeof(struct sockaddr_un);
  CHECK(getsockname(sockfd_, &addr_, &addr_len) == 0)
      << "Failed to get socket name, error message: " << strerror(errno);
  LOG(INFO) << "Listening on path: " << path;

  CHECK(listen(sockfd_, /*backlog*/ 5) == 0)
      << "Failed to listen socket, error message: " << strerror(errno);
}

std::unique_ptr<UnixSocket> UnixSocket::Accept(bool populate_remote_addr) {
  auto new_conn = std::unique_ptr<UnixSocket>(new UnixSocket(0));

  socklen_t remote_addr_len = sizeof(struct sockaddr_un);

  struct sockaddr* addr_ptr = populate_remote_addr ? &new_conn->addr_ : nullptr;
  socklen_t* len_ptr = populate_remote_addr ? &remote_addr_len : nullptr;

  new_conn->sockfd_ = accept4(sockfd_, addr_ptr, len_ptr, /*flags*/ 0);
  CHECK(new_conn->sockfd_ >= 0) << "Failed to accept, error message: " << strerror(errno);

  return new_conn;
}

void UnixSocket::Connect(const UnixSocket& addr) {
  const int retval = connect(sockfd_, &addr.addr_, sizeof(struct sockaddr_un));
  CHECK(retval == 0) << "Failed to connect, error message: " << strerror(errno);

  socklen_t addr_len = sizeof(struct sockaddr_un);
  CHECK(getsockname(sockfd_, &addr_, &addr_len) == 0)
      << "Failed to get socket name, error message: " << strerror(errno);
}

void UnixSocket::Close() {
  if (sockfd_ > 0) {
    CHECK(close(sockfd_) == 0) << "Failed to close socket, error message: " << strerror(errno);
    sockfd_ = 0;
  }

  if (!path().empty()) {
    unlink(path().data());
  }
}

ssize_t UnixSocket::Send(std::string_view data) const {
  return send(sockfd_, data.data(), data.size(), /*flags*/ 0);
}

bool UnixSocket::Recv(std::string* data) const {
  char buf[kBufSize];
  ssize_t size = recv(sockfd_, static_cast<void*>(buf), sizeof(buf), /*flags*/ 0);
  if (size <= 0) {
    return false;
  }
  data->append(buf, size);
  return true;
}

}  // namespace system
}  // namespace px
