#pragma once

// C++-style wrappers of C-style socket addresses.
// TODO(oazizi): Consider renaming the file to sock_utils instead of inet_utils.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/un.h>

#include <string>
#include <utility>
#include <variant>

#include "src/common/base/error.h"
#include "src/common/base/status.h"

namespace pl {

enum class SockAddrFamily { kUnspecified, kIPv4, kIPv6, kUnix, kOther };

/**
 * Describes a connection from user space. This corresponds to struct conn_info_t in
 * src/stirling/bcc_bpf_interface/socket_trace.h.
 */
struct SockAddr {
  SockAddrFamily family = SockAddrFamily::kUnspecified;
  std::variant<struct in_addr, struct in6_addr, std::string> addr;
  int port = -1;

  // TODO(yzhao): Merge AddStr() and ToString().
  std::string AddrStr() const;
  std::string ToString() const {
    return absl::Substitute("[family=$0 addr=$1 port=$2]", magic_enum::enum_name(family), AddrStr(),
                            port);
  }
};

// The IPv4 IP is located in the last 32-bit word of IPv6 address.
constexpr int kIPv4Offset = 3;

/**
 * Checks if the IPv6 addr is a mappped IPv4 address (e.g. ::ffff:x.x.x.x).
 */
inline bool IsIPv4Mapped(const struct in6_addr& a) {
  return ((a.s6_addr16[0]) == 0) && ((a.s6_addr16[1]) == 0) && ((a.s6_addr16[2]) == 0) &&
         ((a.s6_addr16[3]) == 0) && ((a.s6_addr16[4]) == 0) && ((a.s6_addr16[5]) == 0xFFFF);
}

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
 * Parses IPv6 address to string. IPv4 mapped address are printed in IPv4 style.
 * Does not mutate the result argument on parse failure.
 */
inline Status IPv6AddrToString(const struct in6_addr& in6_addr, std::string* addr) {
  char buf[INET6_ADDRSTRLEN];
  if (!IsIPv4Mapped(in6_addr)) {
    if (inet_ntop(AF_INET6, &in6_addr, buf, INET6_ADDRSTRLEN) == nullptr) {
      return error::InvalidArgument("Could not parse sockaddr (AF_INET6) errno=$0", errno);
    }
  } else {
    if (inet_ntop(AF_INET, &in6_addr.s6_addr32[kIPv4Offset], buf, INET_ADDRSTRLEN) == nullptr) {
      return error::InvalidArgument(
          "Could not parse sockaddr (AF_INET mapped as AF_INET6) errno=$0", errno);
    }
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
 * Parses a C-style sockaddr_in (IPv4) to a C++ style SockAddr.
 */
void PopulateInetAddr(struct in_addr in_addr, in_port_t port, SockAddr* addr);

/**
 * Parses a C-style sockaddr_in6 (IPv6) to a C++ style SockAddr.
 */
void PopulateInet6Addr(struct in6_addr in6_addr, in_port_t port, SockAddr* addr);

/**
 * Parses a C-style sockaddr_un (unix domain socket) to a C++ style SockAddr.
 */
void PopulateUnixAddr(const char* sun_path, uint32_t inode, SockAddr* addr);

/**
 * Parses sockaddr into a C++ style SockAddr.
 * Supports IPv4, IPv6 and Unix domain sockets; all other addresses
 * are marked with a special address family, with an address of 0.
 */
void PopulateSockAddr(const struct sockaddr* sa, SockAddr* addr);

inline std::string ToString(const struct sockaddr_in6& sa) {
  SockAddr sock_addr;
  PopulateSockAddr(reinterpret_cast<const sockaddr*>(&sa), &sock_addr);
  return sock_addr.ToString();
}

/**
 * Parses a string as IPv4 or IPv6 address.
 */
Status ParseIPAddress(std::string_view addr_str, SockAddr* ip_addr);

/**
 * Returns a SockAddr in IPv6 format that follows the mapping rule:
 * https://en.wikipedia.org/wiki/IPv6#IPv4-mapped_IPv6_addresses
 */
SockAddr MapIPv4ToIPv6(const SockAddr& addr);

/**
 * Classless Inter Domain Routing Block. Follows the notations at:
 * https://en.wikipedia.org/wiki/Classless_Inter-Domain_Routing
 */
// TODO(oazizi/yzhao): We use SockAddr, but a CIDR shouldn't have a port. Clean-up.
struct CIDRBlock {
  SockAddr ip_addr;
  size_t prefix_length = 0;
};

inline std::string ToString(const CIDRBlock& cidr) {
  return absl::Substitute("$0/$1", cidr.ip_addr.AddrStr(), cidr.prefix_length);
}

inline bool operator==(const CIDRBlock& lhs, const CIDRBlock& rhs) {
  // TODO(oazizi): Better to compare raw bytes using memcmp than converting to a string.
  // Note that this ignores the port in CIDRBlock.SockAddr, since a CIDR shouldn't have a port.
  return lhs.ip_addr.AddrStr() == rhs.ip_addr.AddrStr() && lhs.prefix_length == rhs.prefix_length;
}
inline bool operator!=(const CIDRBlock& lhs, const CIDRBlock& rhs) { return !(lhs == rhs); }

/**
 * Parses the input string as a CIDR block.
 * Does not mutate the result argument on parse failure.
 */
Status ParseCIDRBlock(std::string_view cidr_str, CIDRBlock* cidr);

/**
 * Returns true if block contains ip_addr.
 */
bool CIDRContainsIPAddr(const CIDRBlock& block, const SockAddr& ip_addr);

/**
 * Returns a CIDRBlock in IPv6 format that follows the mapping rule:
 * https://en.wikipedia.org/wiki/IPv6#IPv4-mapped_IPv6_addresses
 */
CIDRBlock MapIPv4ToIPv6(const CIDRBlock& addr);

}  // namespace pl
