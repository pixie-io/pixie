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

TCPSocket::TCPSocket() : TCPSocket(0) {
  // TODO(yzhao): For reference, we think AF_INET & AF_INET6 is largely independent to our code
  // base. So here we only uses AF_INET, i.e. IPv4. Later, if needed, we can add TCPSocketV6 as a
  // subclass to this, which uses AF_INET6.
  sockfd_ = socket(AF_INET, SOCK_STREAM, /*protocol*/ 0);
  CHECK(sockfd_ > 0) << "Failed to create socket, error message: " << strerror(errno);
}

TCPSocket::TCPSocket(int internal) {
  memset(&addr_, 0, sizeof(struct sockaddr_in));
  // Required to differentiate the private vs public TCPSocket constructor.
  PL_UNUSED(internal);
}

TCPSocket::~TCPSocket() { Close(); }

void TCPSocket::BindAndListen(int port) {
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

  CHECK(listen(sockfd_, /*backlog*/ 5) == 0)
      << "Failed to listen socket, error message: " << strerror(errno);
}

std::unique_ptr<TCPSocket> TCPSocket::AcceptWithNullAddr() {
  auto new_conn = std::unique_ptr<TCPSocket>(new TCPSocket(0));

  new_conn->sockfd_ = accept4(sockfd_, /*addr*/ nullptr, /*addr_len*/ nullptr, /*flags*/ 0);
  CHECK(new_conn->sockfd_ >= 0) << "Failed to accept, error message: " << strerror(errno);
  LOG(INFO) << absl::Substitute("Accept(): remote_port=$0 on local_port=$1",
                                ntohs(new_conn->port()), ntohs(port()));

  return new_conn;
}

std::unique_ptr<TCPSocket> TCPSocket::Accept() {
  auto new_conn = std::unique_ptr<TCPSocket>(new TCPSocket(0));

  socklen_t remote_addr_len = sizeof(struct sockaddr_in);
  new_conn->sockfd_ =
      accept4(sockfd_, reinterpret_cast<struct sockaddr*>(&new_conn->addr_), &remote_addr_len,
              /*flags*/ 0);
  CHECK(new_conn->sockfd_ >= 0) << "Failed to accept, error message: " << strerror(errno);
  CHECK(remote_addr_len == sizeof(struct sockaddr_in))
      << "Address length is wrong, " << remote_addr_len << " vs. " << sizeof(struct sockaddr_in);
  LOG(INFO) << absl::Substitute("Accept(): remote_port=$0 on local_port=$1",
                                ntohs(new_conn->port()), ntohs(port()));

  return new_conn;
}

void TCPSocket::Connect(const TCPSocket& addr) {
  const int retval = connect(sockfd_, reinterpret_cast<const struct sockaddr*>(&addr.addr_),
                             sizeof(struct sockaddr_in));
  CHECK(retval == 0) << "Failed to connect, error message: " << strerror(errno);

  socklen_t addr_len = sizeof(struct sockaddr_in);
  CHECK(getsockname(sockfd_, reinterpret_cast<struct sockaddr*>(&addr_), &addr_len) == 0)
      << "Failed to get socket name, error message: " << strerror(errno);
  LOG(INFO) << absl::Substitute("Connect(): remote_port=$0 on local_port=$1", ntohs(addr.port()),
                                ntohs(port()));

  CHECK(addr_len == sizeof(struct sockaddr_in)) << "Address size is incorrect";
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
  return sendmsg(sockfd_, &msg, /*flags*/ 0);
}

ssize_t TCPSocket::SendFile(const std::filesystem::path path) const {
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
    data->assign(buf, size);
  }
  return size;
}

bool TCPSocket::Read(std::string* data) const {
  char buf[kBufSize];
  ssize_t size = read(sockfd_, static_cast<void*>(buf), sizeof(buf));
  if (size <= 0) {
    return false;
  }
  data->assign(buf, size);
  return true;
}

bool TCPSocket::Recv(std::string* data) const {
  char buf[kBufSize];
  ssize_t size = recv(sockfd_, static_cast<void*>(buf), sizeof(buf), /*flags*/ 0);
  if (size <= 0) {
    return false;
  }
  data->assign(buf, size);
  return true;
}

}  // namespace system
}  // namespace px
