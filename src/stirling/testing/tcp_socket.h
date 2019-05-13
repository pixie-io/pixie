#pragma once

#include <netinet/in.h>

#include <string>
#include <string_view>

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
  void Close();
  int Write(std::string_view data) const;
  int Send(std::string_view data) const;
  void Connect(const TCPSocket& addr);
  bool Read(std::string* data);
  const struct sockaddr_in& Addr() const { return addr_; }

 private:
  bool closed = false;
  int sockfd_;
  struct sockaddr_in addr_;
  static constexpr int kBufSize = 128;
  char buf_[kBufSize];
};

}  // namespace testing
}  // namespace stirling
}  // namespace pl
