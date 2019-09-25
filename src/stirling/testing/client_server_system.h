#pragma once

#include <string>
#include <vector>

#include "src/common/system/testing/tcp_socket.h"

namespace pl {
namespace stirling {
namespace testing {

using SendRecvScript = std::vector<std::vector<std::string_view>>;
using TCPSocket = pl::system::testing::TCPSocket;

class ClientServerSystem {
 public:
  ClientServerSystem() { server_.Bind(); }

  /**
   * Create and run a client-server system with the provided send-recv script.
   *
   * @tparam TRecvFn: choose receive implementation from TCPSocket (&TCPSocket::Recv,
   * &TCPSocket::Read).
   * @tparam TSendFn: choose send implementation from TCPSocket (&TCPSocket::Send,
   * &TCPSocket::Write).
   * @param script: script that client server use to send messages to each other.
   */
  template <bool (TCPSocket::*TRecvFn)(std::string*) const = &TCPSocket::Recv,
            ssize_t (TCPSocket::*TSendFn)(std::string_view) const = &TCPSocket::Send>
  void RunClientServer(const SendRecvScript& script) {
    SpawnClient<TRecvFn, TSendFn>(script);
    SpawnServer<TRecvFn, TSendFn>(script);
    JoinThreads();
  }

  /**
   * Create and run a client-server system with server writing data to client.
   *
   * @tparam TRecvFn: choose receive implementation from TCPSocket (&TCPSocket::Recv,
   * &TCPSocket::Read).
   * @tparam TSendFn: choose send implementation from TCPSocket (only &TCPSocket::ReadV is valid).
   * @param write_data: data that server will send to client.
   */
  template <ssize_t (TCPSocket::*TRecvFn)(std::string* data) const = &TCPSocket::ReadV,
            ssize_t (TCPSocket::*TSendFn)(const std::vector<std::string_view>&)
                const = &TCPSocket::SendMsg>
  void RunClientServer(const std::vector<std::vector<std::string_view>>& write_data) {
    SpawnMsgClient<TRecvFn>();
    SpawnMsgServer<TSendFn>(write_data);
    JoinThreads();
  }

  /**
   * Create and run a client-server system with server writing data to client.
   *
   * @tparam TRecvFn: choose receive implementation from TCPSocket (&TCPSocket::Recv,
   * &TCPSocket::Read).
   * @tparam TSendFn: choose send implementation from TCPSocket (only &TCPSocket::ReadMsg is
   * valid).
   * @param write_data: data that server will send to client.
   */
  template <ssize_t (TCPSocket::*TRecvFn)(std::vector<std::string>* data)
                const = &TCPSocket::RecvMsg,
            ssize_t (TCPSocket::*TSendFn)(const std::vector<std::string_view>&)
                const = &TCPSocket::SendMsg>
  void RunClientServer(const std::vector<std::vector<std::string_view>>& write_data) {
    SpawnMsgClient<TRecvFn>();
    SpawnMsgServer<TSendFn>(write_data);
    JoinThreads();
  }

 private:
  /**
   * Wrapper around TCPSocket Send/Write functionality.
   * @tparam TSendFn: choose send implementation from TCPSocket (&TCPSocket::Send,
   * &TCPSocket::Write).
   */
  template <ssize_t (TCPSocket::*TSendFn)(std::string_view) const>
  static void SendData(const TCPSocket& socket, const std::vector<std::string_view>& data) {
    for (auto d : data) {
      ASSERT_EQ(d.length(), (socket.*TSendFn)(d));
    }
  }

  /**
   * Wrapper around TCPSocket Recv/Read functionality.
   * @tparam TRecvFn: choose receive implementation from TCPSocket (&TCPSocket::Recv,
   * &TCPSocket::Read).
   */
  template <bool (TCPSocket::*TRecvFn)(std::string*) const>
  static void RecvData(const TCPSocket& socket, size_t expected_size) {
    std::string msg;
    size_t s = 0;
    while (s < expected_size) {
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
  static void SendData(const TCPSocket& socket,
                       const std::vector<std::vector<std::string_view>>& write_data) {
    std::string msg;
    for (const auto& data : write_data) {
      (socket.*TSendFn)(data);
    }
  }

  /**
   * Wrapper around TCPSocket ReadV functionality.
   * @tparam TSendFn: choose receive implementation from TCPSocket (only &TCPSocket::ReadV is
   * valid).
   */
  template <ssize_t (TCPSocket::*TRecvFn)(std::string* data) const>
  static void RecvData(const TCPSocket& socket) {
    std::string msg;
    while ((socket.*TRecvFn)(&msg) > 0) {
    }
  }

  /**
   * Wrapper around TCPSocket ReadMsg functionality.
   * @tparam TSendFn: choose receive implementation from TCPSocket (only &TCPSocket::ReadMsg is
   * valid).
   */
  template <ssize_t (TCPSocket::*TRecvFn)(std::vector<std::string>* data) const>
  static void RecvData(const TCPSocket& socket) {
    std::vector<std::string> msgs;
    while ((socket.*TRecvFn)(&msgs) > 0) {
    }
  }

  /**
   * Run the script in an alternating order, client sends and server receives during even phase,
   * and vice versa.
   * @tparam TRecvFn: choose receive implementation from TCPSocket (&TCPSocket::Recv,
   * &TCPSocket::Read).
   * @tparam TSendFn: choose send implementation from TCPSocket (&TCPSocket::Send,
   * &TCPSocket::Write).
   * @param socket: client or server socket.
   * @param is_client: whether it's the client or server.
   */
  template <bool (TCPSocket::*TRecvFn)(std::string*) const,
            ssize_t (TCPSocket::*TSendFn)(std::string_view) const>
  void Run(const SendRecvScript& script, const TCPSocket& socket, bool is_client) {
    size_t phase = 0;

    while (phase != script.size()) {
      // phase%2 == 0 |  client   | is_sender
      //      1       |     1     |  1
      //      1       |     0     |  0
      //      0       |     0     |  1
      //      0       |     1     |  0
      if (!((phase % 2 == 0) ^ is_client)) {
        SendData<TSendFn>(socket, script[phase]);
        phase++;
      } else {
        size_t expected_size = 0;
        for (size_t i = 0; i < script[phase].size(); ++i) {
          expected_size += script[phase][i].size();
        }
        RecvData<TRecvFn>(socket, expected_size);
        phase++;
      }
    }
  }

  template <bool (TCPSocket::*TRecvFn)(std::string*) const,
            ssize_t (TCPSocket::*TSendFn)(std::string_view) const>
  void SpawnClient(const SendRecvScript& script) {
    client_thread_ = std::thread([this, script]() {
      client_.Connect(server_);
      Run<TRecvFn, TSendFn>(script, client_, true);
      client_.Close();
    });
  }

  template <bool (TCPSocket::*TRecvFn)(std::string*) const,
            ssize_t (TCPSocket::*TSendFn)(std::string_view) const>
  void SpawnServer(const SendRecvScript& script) {
    server_thread_ = std::thread([this, script]() {
      server_.Accept();
      Run<TRecvFn, TSendFn>(script, server_, false);
      server_.Close();
    });
  }

  // TODO(chengruizhe): Currently SendMsg/RecvMsg & WriteV/ReadV doesn't follow the script and
  // multi-round communication model. Fix this.
  template <ssize_t (TCPSocket::*TSendFn)(const std::vector<std::string_view>&) const>
  void SpawnMsgServer(const std::vector<std::vector<std::string_view>>& write_data) {
    server_thread_ = std::thread([this, write_data]() {
      server_.Accept();
      SendData<TSendFn>(server_, write_data);
      server_.Close();
    });
  }

  template <ssize_t (TCPSocket::*TRecvFn)(std::string* data) const>
  void SpawnMsgClient() {
    client_thread_ = std::thread([this]() {
      client_.Connect(server_);
      RecvData<TRecvFn>(client_);
      client_.Close();
    });
  }

  template <ssize_t (TCPSocket::*TRecvFn)(std::vector<std::string>* data) const>
  void SpawnMsgClient() {
    client_thread_ = std::thread([this]() {
      client_.Connect(server_);
      RecvData<TRecvFn>(client_);
      client_.Close();
    });
  }

  void JoinThreads() {
    server_thread_.join();
    client_thread_.join();
  }

  TCPSocket client_;
  TCPSocket server_;

  std::thread client_thread_;
  std::thread server_thread_;
};

}  // namespace testing
}  // namespace stirling
}  // namespace pl
