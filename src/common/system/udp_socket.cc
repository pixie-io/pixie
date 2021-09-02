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

#include "src/common/system/udp_socket.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "src/common/base/base.h"

namespace px {
namespace system {

// NOTE: Must convert CHECKs to Status if this code is ever used outside test code.

UDPSocket::UDPSocket() {
  memset(&addr_, 0, sizeof(struct sockaddr_in));
  sockfd_ = socket(AF_INET, SOCK_DGRAM, /*protocol*/ 0);
  CHECK(sockfd_ > 0) << "Failed to create socket, error message: " << strerror(errno);
}

UDPSocket::~UDPSocket() { Close(); }

void UDPSocket::BindAndListen(int port) {
  addr_.sin_family = AF_INET;
  addr_.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr_.sin_port = htons(port);
  CHECK(bind(sockfd_, reinterpret_cast<const struct sockaddr*>(&addr_),
             sizeof(struct sockaddr_in)) == 0)
      << "Failed to bind socket, error message: " << strerror(errno);

  socklen_t addr_len = sizeof(struct sockaddr_in);
  CHECK(getsockname(sockfd_, reinterpret_cast<struct sockaddr*>(&addr_), &addr_len) == 0)
      << "Failed to get socket name, error message: " << strerror(errno);
  LOG(INFO) << "Listening on port: " << ntohs(this->port());

  CHECK(addr_len == sizeof(struct sockaddr_in)) << "Address size is incorrect";
}

void UDPSocket::Close() {
  if (sockfd_ > 0) {
    CHECK(close(sockfd_) == 0) << "Failed to close socket, error message: " << strerror(errno);
    sockfd_ = 0;
  }
}

ssize_t UDPSocket::SendTo(std::string_view data, const struct sockaddr_in& dst, int flags) const {
  return sendto(sockfd_, data.data(), data.size(), flags,
                reinterpret_cast<const struct sockaddr*>(&dst), sizeof(struct sockaddr_in));
}

ssize_t UDPSocket::SendMsg(std::string_view data, const struct sockaddr_in& dst, int flags) const {
  struct iovec msg_iov = {};
  msg_iov.iov_len = data.size();
  msg_iov.iov_base = const_cast<void*>(reinterpret_cast<const void*>(data.data()));

  struct msghdr msg = {};
  msg.msg_name = const_cast<void*>(reinterpret_cast<const void*>(&dst));
  msg.msg_namelen = sizeof(struct sockaddr_in);
  msg.msg_iovlen = 1;
  msg.msg_iov = &msg_iov;

  return sendmsg(sockfd_, &msg, flags);
}

ssize_t UDPSocket::SendMMsg(std::string_view data, const struct sockaddr_in& dst, int flags) const {
  struct iovec msg_iov;
  msg_iov.iov_len = data.size();
  msg_iov.iov_base = const_cast<void*>(reinterpret_cast<const void*>(data.data()));

  struct mmsghdr msg = {};
  msg.msg_hdr.msg_name = const_cast<void*>(reinterpret_cast<const void*>(&dst));
  msg.msg_hdr.msg_namelen = sizeof(struct sockaddr_in);
  msg.msg_hdr.msg_iovlen = 1;
  msg.msg_hdr.msg_iov = &msg_iov;

  int retval = sendmmsg(sockfd_, &msg, 1, flags);

  return (retval < 0) ? retval : msg.msg_len;
}

struct sockaddr_in UDPSocket::RecvFrom(std::string* data, int flags) const {
  struct sockaddr_in src;
  socklen_t len = sizeof(struct sockaddr_in);
  char buf[kBufSize];
  ssize_t size = recvfrom(sockfd_, static_cast<void*>(buf), sizeof(buf), flags,
                          reinterpret_cast<struct sockaddr*>(&src), &len);
  if (size <= 0) {
    return {};
  }
  CHECK(len == sizeof(struct sockaddr_in));
  data->append(buf, size);
  return src;
}

struct sockaddr_in UDPSocket::RecvMsg(std::string* data, int flags) const {
  struct sockaddr_in src;
  char buf[kBufSize];

  struct iovec msg_iov = {};
  msg_iov.iov_base = buf;
  msg_iov.iov_len = kBufSize;

  struct msghdr msg = {};
  msg.msg_name = reinterpret_cast<void*>(&src);
  msg.msg_namelen = sizeof(src);
  msg.msg_iovlen = 1;
  msg.msg_iov = &msg_iov;

  ssize_t size = recvmsg(sockfd_, &msg, flags);
  if (size <= 0) {
    return {};
  }
  data->append(buf, size);
  return src;
}

struct sockaddr_in UDPSocket::RecvMMsg(std::string* data, int flags) const {
  struct sockaddr_in src;
  char buf[kBufSize];

  struct iovec msg_iov = {};
  msg_iov.iov_base = buf;
  msg_iov.iov_len = kBufSize;

  struct mmsghdr msg = {};
  msg.msg_hdr.msg_name = reinterpret_cast<void*>(&src);
  msg.msg_hdr.msg_namelen = sizeof(src);
  msg.msg_hdr.msg_iovlen = 1;
  msg.msg_hdr.msg_iov = &msg_iov;

  ssize_t num_msg = recvmmsg(sockfd_, &msg, 1, flags, nullptr);
  if (num_msg <= 0) {
    return {};
  }
  data->append(buf, msg.msg_len);
  return src;
}

}  // namespace system
}  // namespace px
