#pragma once

// C++-style wrappers of C-style IP addresses APIs.

#include <arpa/inet.h>
#include <netinet/in.h>

#include <string>
#include <utility>
#include <variant>

#include "src/common/base/error.h"
#include "src/common/base/status.h"

namespace pl {

/**
 * Describes a connection from user space. This corresponds to struct conn_info_t in
 * src/stirling/bcc_bpf_interface/socket_trace.h.
 */
// TODO(yzhao): This should be renamed to SockAddr as IPAddress does not really have port.
struct IPAddress {
  std::variant<struct in_addr, struct in6_addr> addr;
  // TODO(yzhao): Consider removing this as it can be derived from above.
  std::string addr_str = "-";
  int port = -1;
};

/**
 * Parses IPv4 address to string. Does not mutate the result argument on parse failure.
 */
inline Status ParseIPv4Addr(const struct in_addr& in_addr, std::string* addr) {
  char buf[INET_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &in_addr, buf, INET_ADDRSTRLEN) == nullptr) {
    return error::Internal("Could not parse sockaddr (AF_INET) errno=$0", errno);
  }
  addr->assign(buf);
  return Status::OK();
}

// This version parses an IPv6 struct as an IPv4 address.
// This is handy, because we often use IPv6 structs as a container for either IPv4 addresses,
// where we have to support both protocols.
// In such cases, we use the sa_family to choose the correct function to call.
// This convenience function just avoids the tedious reinterpret_cast that is otherwise required.
inline Status ParseIPv4Addr(const struct in6_addr& in6_addr, std::string* addr) {
  const struct in_addr* in_addr = reinterpret_cast<const struct in_addr*>(&in6_addr);
  return ParseIPv4Addr(*in_addr, addr);
}

inline Status ParseIPv4Addr(const std::string& addr_str, struct in_addr* in_addr) {
  if (!inet_pton(AF_INET, addr_str.c_str(), in_addr)) {
    return error::Internal("Could not parse IPv4 (AF_INET) address: $0", addr_str);
  }
  return Status::OK();
}

inline Status ParseIPv4Addr(const std::string& addr_str, struct in6_addr* in6_addr) {
  return ParseIPv4Addr(addr_str, reinterpret_cast<struct in_addr*>(in6_addr));
}

/**
 * Parses IPv6 address to string. Does not mutate the result argument on parse failure.
 */
inline Status ParseIPv6Addr(const struct in6_addr& in6_addr, std::string* addr) {
  char buf[INET6_ADDRSTRLEN];
  if (inet_ntop(AF_INET6, &in6_addr, buf, INET6_ADDRSTRLEN) == nullptr) {
    return error::InvalidArgument("Could not parse sockaddr (AF_INET6) errno=$0", errno);
  }
  addr->assign(buf);

  return Status::OK();
}

inline Status ParseIPv6Addr(const std::string& addr_str, struct in6_addr* in6_addr) {
  if (!inet_pton(AF_INET6, addr_str.c_str(), in6_addr)) {
    return error::Internal("Could not parse IPv6 (AF_INET6) address: $0", addr_str);
  }
  return Status::OK();
}

/**
 * Parses sockaddr into a C++ style IPAddress. Only accept IPv4 and IPv6 addresses.
 * Does not mutate the result argument on parse failure.
 */
inline Status ParseSockAddr(const struct sockaddr* sa, IPAddress* addr) {
  switch (sa->sa_family) {
    case AF_INET: {
      const auto* sa_in = reinterpret_cast<const struct sockaddr_in*>(sa);
      PL_RETURN_IF_ERROR(ParseIPv4Addr(sa_in->sin_addr, &addr->addr_str));
      addr->addr = sa_in->sin_addr;
      addr->port = ntohs(sa_in->sin_port);
      return Status::OK();
    }
    case AF_INET6: {
      const auto* sa_in6 = reinterpret_cast<const struct sockaddr_in6*>(sa);
      PL_RETURN_IF_ERROR(ParseIPv6Addr(sa_in6->sin6_addr, &addr->addr_str));
      addr->addr = sa_in6->sin6_addr;
      addr->port = ntohs(sa_in6->sin6_port);
      return Status::OK();
    }
    default:
      return error::InvalidArgument("Unhandled sockaddr family: $0", sa->sa_family);
  }
}

}  // namespace pl
