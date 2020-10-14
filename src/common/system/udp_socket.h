#pragma once

#include <netinet/in.h>

#include <memory>
#include <string>
#include <vector>

namespace pl {
namespace system {

/**
 * A simple wrapper of the syscalls for IPv4 UDP socket.
 *
 * Note: Not meant for use in production code. This class uses CHECKs instead of Status/error.
 */
class UDPSocket {
 public:
  UDPSocket();
  ~UDPSocket();

  void BindAndListen(int port = 0);
  void Close();

  int sockfd() const { return sockfd_; }
  const struct in_addr& addr() const { return addr_.sin_addr; }
  in_port_t port() const { return addr_.sin_port; }

  /**
   * Sends data to the specified destination socket.
   */
  ssize_t SendTo(std::string_view data, const UDPSocket& dst) const;

  /**
   * Receives data from the socket, returns a UDPSocket with information about the sender.
   */
  std::unique_ptr<UDPSocket> RecvFrom(std::string* data) const;

 private:
  // This is the core constructor, which is used to internally create an empty UDPSockets.
  // In contrast, the public UDPSocket constructor always creates an initialized UDPSocket.
  // The argument is actually useless, but is used to differentiate the two constructor signatures.
  explicit UDPSocket(int internal);
  int sockfd_ = 0;
  struct sockaddr_in addr_;

  static constexpr int kBufSize = 128;
};

}  // namespace system
}  // namespace pl
