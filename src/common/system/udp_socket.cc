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

namespace pl {
namespace system {

// NOTE: Must convert CHECKs to Status if this code is ever used outside test code.

UDPSocket::UDPSocket() : UDPSocket(0) {
  sockfd_ = socket(AF_INET, SOCK_DGRAM, /*protocol*/ 0);
  CHECK(sockfd_ > 0) << "Failed to create socket, error message: " << strerror(errno);
}

UDPSocket::UDPSocket(int internal) {
  memset(&addr_, 0, sizeof(struct sockaddr_in));
  // Required to differentiate the private vs public UDPSocket constructor.
  PL_UNUSED(internal);
}

UDPSocket::~UDPSocket() { Close(); }

void UDPSocket::BindAndListen(int port) {
  addr_.sin_family = AF_INET;
  addr_.sin_addr.s_addr = INADDR_ANY;
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

ssize_t UDPSocket::SendTo(std::string_view data, const UDPSocket& dst) const {
  return sendto(sockfd_, data.data(), data.size(), /*flags*/ 0,
                reinterpret_cast<const struct sockaddr*>(&dst.addr_), sizeof(struct sockaddr_in));
}

std::unique_ptr<UDPSocket> UDPSocket::RecvFrom(std::string* data) const {
  auto src = std::unique_ptr<UDPSocket>(new UDPSocket(0));
  socklen_t len = sizeof(struct sockaddr_in);
  char buf[kBufSize];
  ssize_t size = recvfrom(sockfd_, static_cast<void*>(buf), sizeof(buf), /*flags*/ 0,
                          reinterpret_cast<struct sockaddr*>(&src->addr_), &len);
  if (size <= 0) {
    return nullptr;
  }
  CHECK(len == sizeof(struct sockaddr_in));
  data->assign(buf, size);
  return src;
}

}  // namespace system
}  // namespace pl
