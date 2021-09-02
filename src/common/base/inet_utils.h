/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

// C++-style wrappers of C-style socket addresses.
// TODO(oazizi): Consider renaming the file to sock_utils instead of inet_utils.

#include <arpa/inet.h>
#include <farmhash.h>
#include <netinet/in.h>
#include <sys/un.h>

#include <map>
#include <string>
#include <utility>
#include <variant>

#include "src/common/base/enum_utils.h"
#include "src/common/base/error.h"
#include "src/common/base/hash_utils.h"
#include "src/common/base/status.h"
#include "src/common/base/statusor.h"

namespace px {

//-----------------------------------------------------------------------------
// Functions on native linux types.
//-----------------------------------------------------------------------------

/**
 * Checks if the IPv6 addr is a mappped IPv4 address (e.g. ::ffff:x.x.x.x).
 */
inline bool IsIPv4Mapped(const struct in6_addr& a) {
  return ((a.s6_addr16[0]) == 0) && ((a.s6_addr16[1]) == 0) && ((a.s6_addr16[2]) == 0) &&
         ((a.s6_addr16[3]) == 0) && ((a.s6_addr16[4]) == 0) && ((a.s6_addr16[5]) == 0xFFFF);
}

/**
 * Parses IPv4 address to string.
 */
StatusOr<std::string> IPv4AddrToString(const struct in_addr& in_addr);

/**
 * Parses IPv6 address to string. IPv4 mapped address are printed in IPv4 style.
 */
StatusOr<std::string> IPv6AddrToString(const struct in6_addr& in6_addr);

/**
 * Parses IPv4/IPv6 Socket address to string.
 */
StatusOr<std::string> IPv4SockAddrToString(const struct sockaddr_in& a);
StatusOr<std::string> IPv6SockAddrToString(const struct sockaddr_in6& a);

/**
 * Prints a sockaddr to string, using the address family to choose the output.
 *
 * Note that the input is a pointer rather than a reference because we
 * need to reinterpret the data to the correct type based on the address family.
 */
std::string ToString(const struct sockaddr* sa);

/**
 * Parses string into an IP address.
 */
Status ParseIPv4Addr(std::string_view addr_str_view, struct in_addr* in_addr);
Status ParseIPv6Addr(std::string_view addr_str_view, struct in6_addr* in6_addr);

inline bool operator==(struct in_addr a, struct in_addr b) { return a.s_addr == b.s_addr; }

inline bool operator==(const struct in6_addr& a, const struct in6_addr& b) {
  return (a.s6_addr32[0] == b.s6_addr32[0]) && (a.s6_addr32[1] == b.s6_addr32[1]) &&
         (a.s6_addr32[2] == b.s6_addr32[2]) && (a.s6_addr32[3] == b.s6_addr32[3]);
}

//-----------------------------------------------------------------------------
// C++ versions of Linux types.
//-----------------------------------------------------------------------------

enum class InetAddrFamily { kUnspecified, kIPv4, kIPv6 };

struct InetAddr {
  InetAddrFamily family = InetAddrFamily::kUnspecified;
  std::variant<struct in_addr, struct in6_addr> addr;

  std::string AddrStr() const;

  bool IsLoopback() const;
};

/**
 * Classless Inter Domain Routing Block. Follows the notations at:
 * https://en.wikipedia.org/wiki/Classless_Inter-Domain_Routing
 */
struct CIDRBlock {
  InetAddr ip_addr;
  size_t prefix_length = 0;

  std::string ToString() const {
    return absl::Substitute("$0/$1", ip_addr.AddrStr(), prefix_length);
  }
};

enum class SockAddrFamily { kUnspecified, kIPv4, kIPv6, kUnix, kOther };
static const std::map<int64_t, std::string_view> kSockAddrFamilyDecoder =
    px::EnumDefToMap<SockAddrFamily>();

struct SockAddrUnix {
  std::string path;
  uint32_t inode_num = -1;
};

struct SockAddrIPv4 {
  struct in_addr addr;
  uint16_t port = -1;

  std::string AddrStr() const { return IPv4AddrToString(addr).ValueOr("<Could not decode>"); }
};

struct SockAddrIPv6 {
  struct in6_addr addr;
  uint16_t port = -1;

  std::string AddrStr() const { return IPv6AddrToString(addr).ValueOr("<Could not decode>"); }
};

struct SockAddrIPv4HashFn {
  size_t operator()(const SockAddrIPv4& x) const {
    size_t hash = std::hash<uint32_t>{}(x.addr.s_addr);
    hash = px::HashCombine(hash, std::hash<uint32_t>{}(x.port));
    return hash;
  }
};

struct SockAddrIPv4EqFn {
  size_t operator()(const SockAddrIPv4& a, const SockAddrIPv4& b) const {
    return a.addr.s_addr == b.addr.s_addr && a.port == b.port;
  }
};

struct SockAddrIPv6HashFn {
  size_t operator()(const SockAddrIPv6& x) const {
    size_t hash = ::util::Hash64(reinterpret_cast<const char*>(&x.addr), sizeof(struct in6_addr));
    hash = px::HashCombine(hash, std::hash<uint32_t>{}(x.port));
    return hash;
  }
};

struct SockAddrIPv6EqFn {
  size_t operator()(const SockAddrIPv6& a, const SockAddrIPv6& b) const {
    return (a.addr == b.addr) && (a.port == b.port);
  }
};

/**
 * Describes a connection from user space. This corresponds to struct conn_info_t in
 * src/stirling/bcc_bpf_interface/socket_trace.h.
 */
struct SockAddr {
  SockAddrFamily family = SockAddrFamily::kUnspecified;
  std::variant<std::monostate, SockAddrIPv4, SockAddrIPv6, SockAddrUnix> addr;

  std::string AddrStr() const;
  int port() const;

  std::string ToString() const {
    return absl::Substitute("[family=$0 addr=$1 port=$2]", magic_enum::enum_name(family), AddrStr(),
                            port());
  }

  StatusOr<InetAddr> ToInetAddr() {
    switch (family) {
      case SockAddrFamily::kIPv4:
        return InetAddr{InetAddrFamily::kIPv4, std::get<SockAddrIPv4>(addr).addr};
      case SockAddrFamily::kIPv6:
        return InetAddr{InetAddrFamily::kIPv6, std::get<SockAddrIPv6>(addr).addr};
      default:
        return error::Internal("Address family $0 cannot be converted to InetAddr.",
                               magic_enum::enum_name(family));
    }
  }
};

// TODO(oazizi): Consider converting the Populate/Parse functions into constructors of the output
// type.

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

/**
 * Parses a string as IPv4 or IPv6 address.
 */
Status ParseIPAddress(std::string_view addr_str, InetAddr* ip_addr);

/**
 * Returns a SockAddr in IPv6 format that follows the mapping rule:
 * https://en.wikipedia.org/wiki/IPv6#IPv4-mapped_IPv6_addresses
 */
InetAddr MapIPv4ToIPv6(const InetAddr& addr);

inline std::string ToString(const CIDRBlock& cidr) {
  return absl::Substitute("$0/$1", cidr.ip_addr.AddrStr(), cidr.prefix_length);
}

inline bool operator==(const CIDRBlock& lhs, const CIDRBlock& rhs) {
  // TODO(oazizi): Better to compare raw bytes using memcmp than converting to a string.
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
bool CIDRContainsIPAddr(const CIDRBlock& block, const InetAddr& ip_addr);

/**
 * Returns a CIDRBlock in IPv6 format that follows the mapping rule:
 * https://en.wikipedia.org/wiki/IPv6#IPv4-mapped_IPv6_addresses
 */
CIDRBlock MapIPv4ToIPv6(const CIDRBlock& addr);

}  // namespace px
