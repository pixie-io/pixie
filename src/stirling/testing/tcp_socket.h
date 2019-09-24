#pragma once

#include <netinet/in.h>

#include <string>
#include <string_view>
#include <vector>

namespace pl {
namespace stirling {
namespace testing {

/**
 * @brief A simple wrapper of the syscalls for IPv4 TCP socket.
 */
class TCPSocket {
 public:
  TCPSocket();
  ~TCPSocket();

  void Bind();
  void Accept();
  void Connect(const TCPSocket& addr);
  void Close();

  struct in_addr addr() {
    return addr_.sin_addr;
  }
  in_port_t port() { return addr_.sin_port; }

  ssize_t Write(std::string_view data) const;
  ssize_t WriteV(const std::vector<std::string_view>& data) const;
  ssize_t Send(std::string_view data) const;
  ssize_t SendMsg(const std::vector<std::string_view>& data) const;
  bool Read(std::string* data) const;
  ssize_t ReadV(std::string* data) const;
  bool Recv(std::string* data) const;
  ssize_t RecvMsg(std::vector<std::string>* data) const;

 private:
  bool closed = false;
  int sockfd_;
  struct sockaddr_in addr_;
  // Do not reduce this to less than 16 bytes; otherwise tests like GRPCTest.BasicTracingForCPP in
  // src/stirling/grpc_trace_bpf_test.cc will be broken.
  //
  // HTTP response detection requires at least 16 bytes to see the HTTP header, any buffer size less
  // than that will causes BPF unable to detect HTTP responses.
  static constexpr int kBufSize = 128;
};

}  // namespace testing
}  // namespace stirling
}  // namespace pl
