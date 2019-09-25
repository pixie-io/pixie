#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>

#include <string>

#include "src/common/base/error.h"
#include "src/common/base/status.h"

namespace pl {

inline Status ParseIPv4Addr(struct in_addr in_addr, std::string* addr) {
  addr->resize(INET_ADDRSTRLEN);
  if (inet_ntop(AF_INET, &in_addr, addr->data(), INET_ADDRSTRLEN) == nullptr) {
    return error::Internal("Could not parse sockaddr (AF_INET) errno=$0", errno);
  }
  addr->erase(addr->find('\0'));

  return Status::OK();
}

inline Status ParseIPv6Addr(struct in6_addr in6_addr, std::string* addr) {
  addr->resize(INET6_ADDRSTRLEN);
  if (inet_ntop(AF_INET6, &in6_addr, addr->data(), INET6_ADDRSTRLEN) == nullptr) {
    return error::InvalidArgument("Could not parse sockaddr (AF_INET6) errno=$0", errno);
  }
  addr->erase(addr->find('\0'));

  return Status::OK();
}

inline Status ParseSockAddr(const struct sockaddr& sa, std::string* addr, int* port) {
  switch (sa.sa_family) {
    case AF_INET: {
      const auto* sa_in = reinterpret_cast<const struct sockaddr_in*>(&sa);
      *port = ntohs(sa_in->sin_port);
      return ParseIPv4Addr(sa_in->sin_addr, addr);
    }
    case AF_INET6: {
      const auto* sa_in6 = reinterpret_cast<const struct sockaddr_in6*>(&sa);
      *port = ntohs(sa_in6->sin6_port);
      return ParseIPv6Addr(sa_in6->sin6_addr, addr);
    }
    default:
      return error::InvalidArgument("Unhandled sockaddr family: $0", sa.sa_family);
  }
}

}  // namespace pl
