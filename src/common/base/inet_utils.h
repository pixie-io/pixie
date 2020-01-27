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

enum class IPVersion { kIPv4, kIPv6 };

/**
 * Describes a connection from user space. This corresponds to struct conn_info_t in
 * src/stirling/bcc_bpf_interface/socket_trace.h.
 */
// TODO(yzhao): This should be renamed to SockAddr as IPAddress does not really have port.
struct IPAddress {
  IPVersion version = IPVersion::kIPv4;
  std::variant<struct in_addr, struct in6_addr> in_addr;
  // TODO(yzhao): Consider removing this as it can be derived from above.
  std::string addr_str = "-";
  int port = -1;
};

/**
 * Parses IPv4 address to string. Does not mutate the result argument on parse failure.
 */
inline Status IPv4AddrToString(const struct in_addr& in_addr, std::string* addr) {
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
inline Status IPv4AddrToString(const struct in6_addr& in6_addr, std::string* addr) {
  const struct in_addr* in_addr = reinterpret_cast<const struct in_addr*>(&in6_addr);
  return IPv4AddrToString(*in_addr, addr);
}

inline Status ParseIPv4Addr(std::string_view addr_str_view, struct in_addr* in_addr) {
  const std::string addr_str(addr_str_view);
  if (!inet_pton(AF_INET, addr_str.c_str(), in_addr)) {
    return error::Internal("Could not parse IPv4 (AF_INET) address: $0", addr_str);
  }
  return Status::OK();
}

inline Status ParseIPv4Addr(std::string_view addr_str_view, struct in6_addr* in6_addr) {
  const std::string addr_str(addr_str_view);
  return ParseIPv4Addr(addr_str, reinterpret_cast<struct in_addr*>(in6_addr));
}

/**
 * Parses IPv6 address to string. Does not mutate the result argument on parse failure.
 */
inline Status IPv6AddrToString(const struct in6_addr& in6_addr, std::string* addr) {
  char buf[INET6_ADDRSTRLEN];
  if (inet_ntop(AF_INET6, &in6_addr, buf, INET6_ADDRSTRLEN) == nullptr) {
    return error::InvalidArgument("Could not parse sockaddr (AF_INET6) errno=$0", errno);
  }
  addr->assign(buf);

  return Status::OK();
}

inline Status ParseIPv6Addr(std::string_view addr_str_view, struct in6_addr* in6_addr) {
  const std::string addr_str(addr_str_view);
  if (!inet_pton(AF_INET6, addr_str.c_str(), in6_addr)) {
    return error::Internal("Could not parse IPv6 (AF_INET6) address: $0", addr_str);
  }
  return Status::OK();
}

/**
 * Parses sockaddr into a C++ style IPAddress. Only accept IPv4 and IPv6 addresses.
 * Does not mutate the result argument on parse failure.
 */
Status ParseSockAddr(const struct sockaddr* sa, IPAddress* addr);

/**
 * Parses a string as IPv4 or IPv6 address.
 */
Status ParseIPAddress(std::string_view addr_str, IPAddress* ip_addr);

/**
 * Returns an IPAddress in IPv6 format that follows the mapping rule:
 * https://en.wikipedia.org/wiki/IPv6#IPv4-mapped_IPv6_addresses
 */
IPAddress MapIPv4ToIPv6(const IPAddress& addr);

/**
 * Classless Inter Domain Routing Block. Follows the notations at:
 * https://en.wikipedia.org/wiki/Classless_Inter-Domain_Routing
 */
struct CIDRBlock {
  IPAddress ip_addr;
  size_t prefix_length = 0;
};

/**
 * Parses the input string as a CIDR block.
 * Does not mutate the result argument on parse failure.
 */
Status ParseCIDRBlock(std::string_view cidr_str, CIDRBlock* cidr);

/**
 * Returns true if block contains ip_addr.
 */
bool CIDRContainsIPAddr(const CIDRBlock& block, const IPAddress& ip_addr);

/**
 * Returns a CIDRBlock in IPv6 format that follows the mapping rule:
 * https://en.wikipedia.org/wiki/IPv6#IPv4-mapped_IPv6_addresses
 */
CIDRBlock MapIPv4ToIPv6(const CIDRBlock& addr);

}  // namespace pl
