#pragma once

#include <netinet/in.h>

#include <memory>
#include <string>
#include <vector>

namespace pl {
namespace system {

/**
 * A simple wrapper of the syscalls for IPv4 TCP socket.
 *
 * Note: Not meant for use in production code. This class uses CHECKs instead of Status/error.
 */
class TCPSocket {
 public:
  TCPSocket();
  ~TCPSocket();

  void BindAndListen(int port = 0);
  std::unique_ptr<TCPSocket> Accept();
  void Connect(const TCPSocket& addr);
  void Close();

  int sockfd() const { return sockfd_; }
  const struct in_addr& addr() const { return addr_.sin_addr; }
  in_port_t port() const { return addr_.sin_port; }

  ssize_t Write(std::string_view data) const;
  ssize_t WriteV(const std::vector<std::string_view>& data) const;
  ssize_t Send(std::string_view data) const;
  ssize_t SendMsg(const std::vector<std::string_view>& data) const;
  bool Read(std::string* data) const;
  ssize_t ReadV(std::string* data) const;
  bool Recv(std::string* data) const;
  ssize_t RecvMsg(std::vector<std::string>* data) const;

 private:
  // This is the core constructor, which is used to internally create an empty TCPSockets.
  // In contrast, the public TCPSocket constructor always creates an initialized TCPSocket.
  // The argument is actually useless, but is used to differentiate the two constructor signatures.
  explicit TCPSocket(int internal);
  int sockfd_ = 0;
  struct sockaddr_in addr_;
  // Do not reduce this to less than 16 bytes; otherwise tests like GRPCTest.BasicTracingForCPP in
  // src/stirling/grpc_trace_bpf_test.cc will be broken.
  //
  // HTTP response detection requires at least 16 bytes to see the HTTP header, any buffer size less
  // than that will causes BPF unable to detect HTTP responses.
  static constexpr int kBufSize = 128;
};

}  // namespace system
}  // namespace pl
