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

#include "src/common/base/logging.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace px {
namespace system {

/**
 * A simple wrapper of the syscalls for IPv4 TCP socket.
 *
 * Note: Not meant for use in production code. This class uses CHECKs instead of Status/error.
 */
class TCPSocket {
 public:
  explicit TCPSocket(sa_family_t sa_family = AF_INET);
  ~TCPSocket();

  void BindAndListen(int port = 0);
  std::unique_ptr<TCPSocket> Accept(bool populate_remote_addr = true);

  void Connect(const TCPSocket& addr);
  void Close();

  int sockfd() const { return sockfd_; }
  const struct in_addr& addr() const {
    CHECK_EQ(kAF, AF_INET);
    return addr4_.sin_addr;
  }
  const struct in6_addr& addr6() const {
    CHECK_EQ(kAF, AF_INET6);
    return addr6_.sin6_addr;
  }
  in_port_t port() const { return kAF == AF_INET ? addr4_.sin_port : addr6_.sin6_port; }

  ssize_t Write(std::string_view data) const;
  ssize_t WriteV(const std::vector<std::string_view>& data) const;
  ssize_t Send(std::string_view data) const;
  ssize_t SendMsg(const std::vector<std::string_view>& data) const;
  ssize_t SendMMsg(std::string_view data) const;
  ssize_t SendFile(const std::filesystem::path& path) const;
  bool Read(std::string* data) const;
  ssize_t ReadV(std::string* data) const;
  bool Recv(std::string* data) const;
  ssize_t RecvMsg(std::vector<std::string>* data) const;
  bool RecvMMsg(std::string* data) const;

 private:
  // This is the core constructor, which is used to internally create an empty TCPSockets.
  // In contrast, the public TCPSocket constructor always creates an initialized TCPSocket.
  // The argument is actually useless, but is used to differentiate the two constructor signatures.
  TCPSocket(sa_family_t sa_family, int internal);
  int sockfd_ = 0;

  // Constants based on IPv4 vs IPv6.
  const sa_family_t kAF;
  const size_t kSockAddrSize;

  // Use union to support both IPv4 and IPv6.
  // We use &addr_ in the implementation, so using the union is important.
  union {
    struct sockaddr addr_;
    struct sockaddr_in addr4_;
    struct sockaddr_in6 addr6_;
  };

  // Do not reduce this to less than 16 bytes; otherwise tests like GRPCTest.BasicTracingForCPP in
  // src/stirling/grpc_trace_bpf_test.cc will be broken.
  //
  // HTTP response detection requires at least 16 bytes to see the HTTP header, any buffer size less
  // than that will causes BPF unable to detect HTTP responses.
  static constexpr int kBufSize = 128;
};

}  // namespace system
}  // namespace px
