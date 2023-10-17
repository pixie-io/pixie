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

#include <sys/wait.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "src/common/system/tcp_socket.h"

namespace px {
namespace stirling {
namespace testing {

struct ReqAndResp {
  // Request and response are vectors, for the sake of syscalls like RecvMsg/ReadV,
  // which support a scatter-gather model.
  std::vector<std::string_view> req;
  std::vector<std::string_view> resp;
};
using SendRecvScript = std::vector<ReqAndResp>;
using TCPSocket = px::system::TCPSocket;

class ClientServerSystem {
 public:
  ClientServerSystem(
      std::chrono::milliseconds server_response_latency = std::chrono::milliseconds{0})
      : server_response_latency_(server_response_latency) {
    server_.BindAndListen();
  }

  /**
   * Create and run a client-server system with the provided send-recv script.
   * Server is run as a new thread (same PID).
   * Client is run as a new process (different PID).
   *
   * @tparam TRecvFn: choose receive implementation from TCPSocket (&TCPSocket::Recv,
   * &TCPSocket::Read).
   * @tparam TSendFn: choose send implementation from TCPSocket (&TCPSocket::Send,
   * &TCPSocket::Write).
   * @param script: script that client server use to send messages to each other.
   */
#define RunClientServerImpl(script)      \
  SpawnClient<TRecvFn, TSendFn>(script); \
  SpawnServer<TRecvFn, TSendFn>(script); \
  JoinThreads();

  template <bool (TCPSocket::*TRecvFn)(std::string*) const = &TCPSocket::Recv,
            ssize_t (TCPSocket::*TSendFn)(std::string_view) const = &TCPSocket::Send>
  void RunClientServer(const SendRecvScript& script) {
    RunClientServerImpl(script)
  }

  template <ssize_t (TCPSocket::*TRecvFn)(std::string* data) const = &TCPSocket::ReadV,
            ssize_t (TCPSocket::*TSendFn)(const std::vector<std::string_view>&)
                const = &TCPSocket::WriteV>
  void RunClientServer(const SendRecvScript& script) {
    RunClientServerImpl(script)
  }

  template <ssize_t (TCPSocket::*TRecvFn)(std::vector<std::string>* data)
                const = &TCPSocket::RecvMsg,
            ssize_t (TCPSocket::*TSendFn)(const std::vector<std::string_view>&)
                const = &TCPSocket::SendMsg>
  void RunClientServer(const SendRecvScript& script) {
    RunClientServerImpl(script)
  }

 public:  // Required as an `arc lint` workaround.
  // PID of client, if spawned. Otherwise -1.
  uint32_t ClientPID() { return client_pid_; }
  uint32_t ClientFD() { return client_fd_; }

  // PID of server, if spawned. Otherwise -1.
  uint32_t ServerPID() { return server_pid_; }
  uint32_t ServerFD() { return server_fd_; }

 private:
  /**
   * Wrapper around TCPSocket Send/Write functionality.
   * @tparam TSendFn: choose send implementation from TCPSocket (&TCPSocket::Send,
   * &TCPSocket::Write).
   */
  template <ssize_t (TCPSocket::*TSendFn)(std::string_view) const>
  static void SendData(const TCPSocket& socket, const std::vector<std::string_view>& data) {
    for (auto d : data) {
      CHECK_EQ(static_cast<ssize_t>(d.length()), (socket.*TSendFn)(d));
    }
  }

  /**
   * Wrapper around TCPSocket Recv/Read functionality.
   * @tparam TRecvFn: choose receive implementation from TCPSocket (&TCPSocket::Recv,
   * &TCPSocket::Read).
   */
  template <bool (TCPSocket::*TRecvFn)(std::string*) const>
  static void RecvData(const TCPSocket& socket, size_t expected_size) {
    size_t s = 0;
    while (s < expected_size) {
      std::string msg;
      (socket.*TRecvFn)(&msg);
      s += msg.size();
    }
  }

  /**
   * Wrapper around TCPSocket SendMsg/WriteV functionality.
   * @tparam TSendFn: choose send implementation from TCPSocket (&TCPSocket::SendMsg,
   * &TCPSocket::WriteV).
   */
  template <ssize_t (TCPSocket::*TSendFn)(const std::vector<std::string_view>&) const>
  static void SendData(const TCPSocket& socket, const std::vector<std::string_view>& write_data) {
    (socket.*TSendFn)(write_data);
  }

  /**
   * Wrapper around TCPSocket ReadV functionality.
   * @tparam TSendFn: choose receive implementation from TCPSocket (only &TCPSocket::ReadV is
   * valid).
   */
  template <ssize_t (TCPSocket::*TRecvFn)(std::string* data) const>
  static void RecvData(const TCPSocket& socket, size_t expected_size) {
    size_t s = 0;
    while (s < expected_size) {
      std::string msg;
      (socket.*TRecvFn)(&msg);
      s += msg.size();
    }
  }

  /**
   * Wrapper around TCPSocket ReadMsg functionality.
   * @tparam TSendFn: choose receive implementation from TCPSocket (only &TCPSocket::ReadMsg is
   * valid).
   */
  template <ssize_t (TCPSocket::*TRecvFn)(std::vector<std::string>* data) const>
  static void RecvData(const TCPSocket& socket, size_t expected_size) {
    size_t s = 0;
    while (s < expected_size) {
      std::vector<std::string> msgs;
      (socket.*TRecvFn)(&msgs);
      for (const auto& m : msgs) {
        s += m.size();
      }
    }
  }

  /**
   * Run the script as a client.
   * @tparam TRecvFn: choose receive implementation from TCPSocket (&TCPSocket::Recv,
   * &TCPSocket::Read).
   * @tparam TSendFn: choose send implementation from TCPSocket (&TCPSocket::Send,
   * &TCPSocket::Write).
   * @param socket: client or server socket.
   */
#define RunClientImpl(script, socket)         \
  for (const auto& x : script) {              \
    /* Send request. */                       \
    SendData<TSendFn>(socket, x.req);         \
                                              \
    /* Receive reply */                       \
    size_t expected_size = 0;                 \
    for (const auto& resp : x.resp) {         \
      expected_size += resp.size();           \
    }                                         \
    RecvData<TRecvFn>(socket, expected_size); \
  }

  template <bool (TCPSocket::*TRecvFn)(std::string*) const,
            ssize_t (TCPSocket::*TSendFn)(std::string_view) const>
  void RunClient(const SendRecvScript& script, const TCPSocket& socket) {
    RunClientImpl(script, socket);
  }

  template <ssize_t (TCPSocket::*TRecvFn)(std::string* data) const,
            ssize_t (TCPSocket::*TSendFn)(const std::vector<std::string_view>&) const>
  void RunClient(const SendRecvScript& script, const TCPSocket& socket) {
    RunClientImpl(script, socket);
  }

  template <ssize_t (TCPSocket::*TRecvFn)(std::vector<std::string>* data) const,
            ssize_t (TCPSocket::*TSendFn)(const std::vector<std::string_view>&) const>
  void RunClient(const SendRecvScript& script, const TCPSocket& socket) {
    RunClientImpl(script, socket);
  }

  /**
   * Run the script as a server.
   * @tparam TRecvFn: choose receive implementation from TCPSocket (&TCPSocket::Recv,
   * &TCPSocket::Read).
   * @tparam TSendFn: choose send implementation from TCPSocket (&TCPSocket::Send,
   * &TCPSocket::Write).
   * @param socket: client or server socket.
   */
#define RunServerImpl(script, socket)                      \
  for (const auto& x : script) {                           \
    /* Receive request */                                  \
    size_t expected_size = 0;                              \
    for (const auto& req : x.req) {                        \
      expected_size += req.size();                         \
    }                                                      \
    RecvData<TRecvFn>(socket, expected_size);              \
                                                           \
    std::this_thread::sleep_for(server_response_latency_); \
                                                           \
    /* Send response. */                                   \
    SendData<TSendFn>(socket, x.resp);                     \
  }

  template <bool (TCPSocket::*TRecvFn)(std::string*) const,
            ssize_t (TCPSocket::*TSendFn)(std::string_view) const>
  void RunServer(const SendRecvScript& script, const TCPSocket& socket) {
    RunServerImpl(script, socket);
  }

  template <ssize_t (TCPSocket::*TRecvFn)(std::string* data) const,
            ssize_t (TCPSocket::*TSendFn)(const std::vector<std::string_view>&) const>
  void RunServer(const SendRecvScript& script, const TCPSocket& socket) {
    RunServerImpl(script, socket);
  }

  template <ssize_t (TCPSocket::*TRecvFn)(std::vector<std::string>* data) const,
            ssize_t (TCPSocket::*TSendFn)(const std::vector<std::string_view>&) const>
  void RunServer(const SendRecvScript& script, const TCPSocket& socket) {
    RunServerImpl(script, socket);
  }

  /**
   * Spawn a client with the provided script.
   * On even-phases, it will send the script value.
   * On odd-phases, it will expect to receive the script value.
   */
#define SpawnClientImpl(script)                                                  \
  int fd[2];                                                                     \
  CHECK_EQ(pipe(fd), 0);                                                         \
  client_pid_ = fork();                                                          \
  if (client_pid_ == 0) {                                                        \
    client_.Connect(server_);                                                    \
    client_fd_ = client_.sockfd();                                               \
    CHECK_EQ(static_cast<size_t>(write(fd[1], &client_fd_, sizeof(client_fd_))), \
             sizeof(client_fd_));                                                \
    RunClient<TRecvFn, TSendFn>(script, client_);                                \
    client_.Close();                                                             \
    exit(0);                                                                     \
  } else {                                                                       \
    CHECK_EQ(static_cast<size_t>(read(fd[0], &client_fd_, sizeof(client_fd_))),  \
             sizeof(client_fd_));                                                \
    LOG(INFO) << "Client PID: " << client_pid_;                                  \
    LOG(INFO) << "Client FD: " << client_fd_;                                    \
    close(fd[0]);                                                                \
    close(fd[1]);                                                                \
  }

  template <bool (TCPSocket::*TRecvFn)(std::string*) const,
            ssize_t (TCPSocket::*TSendFn)(std::string_view) const>
  void SpawnClient(const SendRecvScript& script) {
    SpawnClientImpl(script);
  }

  template <ssize_t (TCPSocket::*TRecvFn)(std::string* data) const,
            ssize_t (TCPSocket::*TSendFn)(const std::vector<std::string_view>&) const>
  void SpawnClient(const SendRecvScript& script) {
    SpawnClientImpl(script);
  }

  template <ssize_t (TCPSocket::*TRecvFn)(std::vector<std::string>* data) const,
            ssize_t (TCPSocket::*TSendFn)(const std::vector<std::string_view>&) const>
  void SpawnClient(const SendRecvScript& script) {
    SpawnClientImpl(script);
  }

  /**
   * Spawn a server with the provided script.
   * On even-phases, it will expect to receive the script value.
   * On odd-phases, it will send the script value.
   */
#define SpawnServerImpl(script)                            \
  server_pid_ = getpid();                                  \
  LOG(INFO) << "Server PID: " << server_pid_;              \
  server_thread_ = std::thread([this, script]() {          \
    std::this_thread::sleep_for(server_response_latency_); \
    std::unique_ptr<TCPSocket> conn = server_.Accept();    \
    server_fd_ = conn->sockfd();                           \
    RunServer<TRecvFn, TSendFn>(script, *conn);            \
    server_.Close();                                       \
  });

  template <bool (TCPSocket::*TRecvFn)(std::string*) const,
            ssize_t (TCPSocket::*TSendFn)(std::string_view) const>
  void SpawnServer(const SendRecvScript& script) {
    SpawnServerImpl(script);
  }

  template <ssize_t (TCPSocket::*TRecvFn)(std::string* data) const,
            ssize_t (TCPSocket::*TSendFn)(const std::vector<std::string_view>&) const>
  void SpawnServer(const SendRecvScript& script) {
    SpawnServerImpl(script);
  }

  template <ssize_t (TCPSocket::*TRecvFn)(std::vector<std::string>* data) const,
            ssize_t (TCPSocket::*TSendFn)(const std::vector<std::string_view>&) const>
  void SpawnServer(const SendRecvScript& script) {
    SpawnServerImpl(script);
  }

  void JoinThreads() {
    server_thread_.join();
    waitpid(client_pid_, 0, 0);
  }

  // Sometimes we want to model longer lived connections; this variable allows the server
  // to add an artificial delay for this purpose.
  // This is particularly important in tests where a new connection needs to be detected
  // before tracing can begin (see SystemWideStandaloneContext).
  std::chrono::milliseconds server_response_latency_;

  TCPSocket client_;
  TCPSocket server_;

  uint32_t client_pid_ = -1;
  uint32_t server_pid_ = -1;

  int client_fd_ = -1;
  int server_fd_ = -1;

  std::thread server_thread_;
  // No thread for client, it is run via fork().
};

}  // namespace testing
}  // namespace stirling
}  // namespace px
