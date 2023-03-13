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

#include "src/common/system/tcp_socket.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <memory>
#include <vector>

#include "src/common/base/base.h"

namespace px {
namespace system {

// NOTE: Must convert CHECKs to Status if this code is ever used outside test code.

namespace {

constexpr size_t SockAddrSize(sa_family_t sa_family) {
  switch (sa_family) {
    case AF_INET:
      return sizeof(struct sockaddr_in);
    case AF_INET6:
      return sizeof(struct sockaddr_in6);
    default:
      COMPILE_TIME_ASSERT(false, "Unexpected address family");
      return 0;
  }
}

}  // namespace

TCPSocket::TCPSocket(sa_family_t sa_family) : TCPSocket(sa_family, 0) {
  sockfd_ = socket(kAF, SOCK_STREAM, /*protocol*/ 0);
  CHECK(sockfd_ > 0) << "Failed to create socket, error message: " << strerror(errno);
}

TCPSocket::TCPSocket(sa_family_t sa_family, int internal)
    : kAF(sa_family), kSockAddrSize(SockAddrSize(sa_family)) {
  memset(&addr_, 0, kSockAddrSize);

  // Required to differentiate the private vs public TCPSocket constructor.
  PX_UNUSED(internal);
}

TCPSocket::~TCPSocket() { Close(); }

void TCPSocket::BindAndListen(int port) {
  if (kAF == AF_INET) {
    addr4_.sin_family = AF_INET;
    addr4_.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr4_.sin_port = htons(port);
  } else {
    addr6_.sin6_family = kAF;
    addr6_.sin6_addr = in6addr_loopback;
    addr6_.sin6_port = htons(port);
  }

  CHECK(bind(sockfd_, &addr_, kSockAddrSize) == 0)
      << "Failed to bind socket, error message: " << strerror(errno);

  socklen_t addr_len = kSockAddrSize;
  CHECK(getsockname(sockfd_, &addr_, &addr_len) == 0)
      << "Failed to get socket name, error message: " << strerror(errno);
  LOG(INFO) << "Listening on port: " << ntohs(this->port());

  CHECK(addr_len == kSockAddrSize) << "Address size is incorrect";

  CHECK(listen(sockfd_, /*backlog*/ 5) == 0)
      << "Failed to listen socket, error message: " << strerror(errno);
}

std::unique_ptr<TCPSocket> TCPSocket::Accept(bool populate_remote_addr) {
  auto new_conn = std::unique_ptr<TCPSocket>(new TCPSocket(kAF, 0));

  socklen_t remote_addr_len = kSockAddrSize;

  struct sockaddr* addr_ptr = populate_remote_addr ? &new_conn->addr_ : nullptr;
  socklen_t* len_ptr = populate_remote_addr ? &remote_addr_len : nullptr;

  new_conn->sockfd_ = accept4(sockfd_, addr_ptr, len_ptr, /*flags*/ 0);
  CHECK(new_conn->sockfd_ >= 0) << "Failed to accept, error message: " << strerror(errno);
  LOG(INFO) << absl::Substitute("Accept(): remote_port=$0 on local_port=$1",
                                ntohs(new_conn->port()), ntohs(port()));

  return new_conn;
}

void TCPSocket::Connect(const TCPSocket& addr) {
  const int retval = connect(sockfd_, &addr.addr_, kSockAddrSize);
  CHECK(retval == 0) << "Failed to connect, error message: " << strerror(errno);

  socklen_t addr_len = kSockAddrSize;
  CHECK(getsockname(sockfd_, &addr_, &addr_len) == 0)
      << "Failed to get socket name, error message: " << strerror(errno);
  LOG(INFO) << absl::Substitute("Connect(): remote_port=$0 on local_port=$1", ntohs(addr.port()),
                                ntohs(port()));

  CHECK(addr_len == kSockAddrSize) << "Address size is incorrect";
}

void TCPSocket::Close() {
  if (sockfd_ > 0) {
    CHECK(close(sockfd_) == 0) << "Failed to close socket, error message: " << strerror(errno);
    sockfd_ = 0;
  }
}

ssize_t TCPSocket::Write(std::string_view data) const {
  return write(sockfd_, data.data(), data.size());
}

ssize_t TCPSocket::Send(std::string_view data) const {
  return send(sockfd_, data.data(), data.size(), /*flags*/ 0);
}

ssize_t TCPSocket::SendMsg(const std::vector<std::string_view>& data) const {
  struct msghdr msg = {};
  msg.msg_iovlen = data.size();
  auto msg_iov = std::make_unique<struct iovec[]>(data.size());
  msg.msg_iov = msg_iov.get();
  for (size_t i = 0; i < data.size(); ++i) {
    msg.msg_iov[i].iov_base = const_cast<char*>(data[i].data());
    msg.msg_iov[i].iov_len = data[i].size();
  }
  return sendmsg(sockfd_, &msg, /* flags */ 0);
}

ssize_t TCPSocket::SendMMsg(std::string_view data) const {
  struct iovec msg_iov;
  msg_iov.iov_len = data.size();
  msg_iov.iov_base = const_cast<void*>(reinterpret_cast<const void*>(data.data()));

  struct mmsghdr msg = {};
  msg.msg_hdr.msg_iovlen = 1;
  msg.msg_hdr.msg_iov = &msg_iov;

  int retval = sendmmsg(sockfd_, &msg, 1, /* flags */ 0);

  return (retval < 0) ? retval : msg.msg_len;
}

ssize_t TCPSocket::SendFile(const std::filesystem::path& path) const {
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    return -1;
  }

  struct stat stat_buf;
  fstat(fd, &stat_buf);

  off_t offset = 0;
  size_t count = stat_buf.st_size;

  return sendfile(sockfd_, fd, &offset, count);
}

ssize_t TCPSocket::RecvMsg(std::vector<std::string>* data) const {
  char buf[kBufSize];

  struct iovec iov;
  iov.iov_base = buf;
  iov.iov_len = kBufSize;

  struct msghdr msg = {};
  msg.msg_name = nullptr;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  const ssize_t size = recvmsg(sockfd_, &msg, /*flags*/ 0);
  if (size <= 0) {
    return size;
  }

  ssize_t copied_size = 0;
  for (size_t i = 0; i < msg.msg_iovlen && copied_size < size; ++i) {
    // recvmsg() will fill each buffer one after another. But do not rewrite iov_len to the actually
    // written data. Therefore, we need to track through the total written data.
    const size_t size_to_copy = std::min<size_t>(msg.msg_iov[i].iov_len, size - copied_size);
    data->emplace_back(static_cast<const char*>(msg.msg_iov[i].iov_base), size_to_copy);
    copied_size += size_to_copy;
  }
  return size;
}

bool TCPSocket::RecvMMsg(std::string* data) const {
  char buf[kBufSize];

  struct iovec msg_iov = {};
  msg_iov.iov_base = buf;
  msg_iov.iov_len = kBufSize;

  struct mmsghdr msg = {};
  msg.msg_hdr.msg_iovlen = 1;
  msg.msg_hdr.msg_iov = &msg_iov;

  ssize_t retval = recvmmsg(sockfd_, &msg, 1, /* flags */ 0, nullptr);
  if (retval <= 0 || msg.msg_len == 0) {
    return false;
  }
  data->append(buf, msg.msg_len);
  return true;
}

ssize_t TCPSocket::WriteV(const std::vector<std::string_view>& data) const {
  auto iov = std::make_unique<struct iovec[]>(data.size());
  for (size_t i = 0; i < data.size(); ++i) {
    iov[i].iov_base = const_cast<char*>(data[i].data());
    iov[i].iov_len = data[i].size();
  }
  return writev(sockfd_, iov.get(), data.size());
}

ssize_t TCPSocket::ReadV(std::string* data) const {
  char buf[kBufSize];
  struct iovec iov;
  iov.iov_base = buf;
  iov.iov_len = sizeof(buf);

  const ssize_t size = readv(sockfd_, &iov, 1);
  if (size > 0) {
    data->append(buf, size);
  }
  return size;
}

bool TCPSocket::Read(std::string* data) const {
  char buf[kBufSize];
  ssize_t size = read(sockfd_, static_cast<void*>(buf), sizeof(buf));
  if (size <= 0) {
    return false;
  }
  data->append(buf, size);
  return true;
}

bool TCPSocket::Recv(std::string* data) const {
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
