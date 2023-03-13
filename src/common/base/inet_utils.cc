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

#include "src/common/base/inet_utils.h"

#include <vector>

namespace px {

constexpr int kIPv4BitLen = 32;
constexpr int kIPv6BitLen = 128;

// The IPv4 IP is located in the last 32-bit word of IPv6 address.
constexpr int kIPv4Offset = 3;

std::string SockAddr::AddrStr() const {
  switch (family) {
    case SockAddrFamily::kUnspecified:
      return "-";
    case SockAddrFamily::kIPv4:
      return IPv4AddrToString(std::get<SockAddrIPv4>(addr).addr)
          .ValueOr("<Error decoding address>");
    case SockAddrFamily::kIPv6:
      return IPv6AddrToString(std::get<SockAddrIPv6>(addr).addr)
          .ValueOr("<Error decoding address>");
    case SockAddrFamily::kUnix:
      return absl::StrCat("unix_socket:", std::get<SockAddrUnix>(addr).path);
    default:
      return absl::Substitute("<unknown SockAddrFamily $0>", magic_enum::enum_name(family));
  }
}

int SockAddr::port() const {
  switch (family) {
    case SockAddrFamily::kUnspecified:
      return -1;
    case SockAddrFamily::kIPv4:
      return std::get<SockAddrIPv4>(addr).port;
    case SockAddrFamily::kIPv6:
      return std::get<SockAddrIPv6>(addr).port;
    case SockAddrFamily::kUnix:
      return std::get<SockAddrUnix>(addr).inode_num;
    default:
      return -1;
  }
}

std::string InetAddr::AddrStr() const {
  std::string out;

  Status s;
  switch (family) {
    case InetAddrFamily::kUnspecified:
      return "-";
    case InetAddrFamily::kIPv4:
      return IPv4AddrToString(std::get<struct in_addr>(addr)).ValueOr("<Error decoding address>");
    case InetAddrFamily::kIPv6:
      return IPv6AddrToString(std::get<struct in6_addr>(addr)).ValueOr("<Error decoding address>");
    default:
      return absl::Substitute("<unknown InetAddrFamily $0>", magic_enum::enum_name(family));
  }
}

bool InetAddr::IsLoopback() const {
  switch (family) {
    case InetAddrFamily::kUnspecified:
      return false;
    case InetAddrFamily::kIPv4:
      return std::get<struct in_addr>(addr).s_addr == ntohl(INADDR_LOOPBACK);
    case InetAddrFamily::kIPv6:
      return std::get<struct in6_addr>(addr) == in6addr_loopback;
    default:
      return false;
  }
}

StatusOr<std::string> IPv4AddrToString(const struct in_addr& in_addr) {
  char buf[INET_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &in_addr, buf, INET_ADDRSTRLEN) == nullptr) {
    return error::Internal("Could not parse sockaddr (AF_INET) errno=$0", errno);
  }
  return std::string(buf);
}

StatusOr<std::string> IPv6AddrToString(const struct in6_addr& in6_addr) {
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

  return std::string(buf);
}

StatusOr<std::string> IPv4SockAddrToString(const struct sockaddr_in& a) {
  PX_ASSIGN_OR_RETURN(std::string addr_str, IPv4AddrToString(a.sin_addr));
  return absl::Substitute("[family=$0 addr=$1 port=$2]", a.sin_family, addr_str, a.sin_port);
}

StatusOr<std::string> IPv6SockAddrToString(const struct sockaddr_in6& a) {
  PX_ASSIGN_OR_RETURN(std::string addr_str, IPv6AddrToString(a.sin6_addr));
  return absl::Substitute("[family=$0 addr=$1 port=$2]", a.sin6_family, addr_str, a.sin6_port);
}

StatusOr<std::string> UnixSockAddrToString(const struct sockaddr_un& a) {
  return absl::Substitute("[family=$0 addr=$1]", a.sun_family, a.sun_path);
}

std::string ToString(const struct sockaddr* sa) {
  switch (sa->sa_family) {
    case AF_INET:
      return IPv4SockAddrToString(*reinterpret_cast<const struct sockaddr_in*>(sa))
          .ValueOr("<Error decoding address>");
    case AF_INET6:
      return IPv6SockAddrToString(*reinterpret_cast<const struct sockaddr_in6*>(sa))
          .ValueOr("<Error decoding address>");
    case AF_UNIX:
      return UnixSockAddrToString(*reinterpret_cast<const struct sockaddr_un*>(sa))
          .ValueOr("<Error decoding address>");
    default:
      return absl::Substitute("<unsupported address family $0>", sa->sa_family);
  }
}

Status ParseIPv4Addr(std::string_view addr_str_view, struct in_addr* in_addr) {
  const std::string addr_str(addr_str_view);
  if (!inet_pton(AF_INET, addr_str.c_str(), in_addr)) {
    return error::Internal("Could not parse IPv4 (AF_INET) address: $0", addr_str);
  }
  return Status::OK();
}

Status ParseIPv6Addr(std::string_view addr_str_view, struct in6_addr* in6_addr) {
  const std::string addr_str(addr_str_view);
  if (!inet_pton(AF_INET6, addr_str.c_str(), in6_addr)) {
    return error::Internal("Could not parse IPv6 (AF_INET6) address: $0", addr_str);
  }
  return Status::OK();
}

InetAddr MapIPv4ToIPv6(const InetAddr& addr) {
  DCHECK(addr.family == InetAddrFamily::kIPv4);

  const struct in_addr& v4_addr = std::get<struct in_addr>(addr.addr);

  struct in6_addr v6_addr = {{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0, 0, 0, 0}}};
  v6_addr.s6_addr32[kIPv4Offset] = v4_addr.s_addr;

  return InetAddr{InetAddrFamily::kIPv6, v6_addr};
}

void PopulateInetAddr(struct in_addr in_addr, in_port_t port, SockAddr* addr) {
  addr->family = SockAddrFamily::kIPv4;
  addr->addr = SockAddrIPv4{in_addr, ntohs(port)};
}

void PopulateInet6Addr(struct in6_addr in6_addr, in_port_t port, SockAddr* addr) {
  addr->family = SockAddrFamily::kIPv6;
  addr->addr = SockAddrIPv6{in6_addr, ntohs(port)};
}

void PopulateUnixAddr(const char* sun_path, uint32_t inode_num, SockAddr* addr) {
  addr->family = SockAddrFamily::kUnix;
  addr->addr = SockAddrUnix{sun_path, inode_num};
}

void PopulateSockAddr(const struct sockaddr* sa, SockAddr* addr) {
  switch (sa->sa_family) {
    case AF_INET: {
      const auto* sa_in = reinterpret_cast<const struct sockaddr_in*>(sa);
      PopulateInetAddr(sa_in->sin_addr, sa_in->sin_port, addr);
      break;
    }
    case AF_INET6: {
      const auto* sa_in6 = reinterpret_cast<const struct sockaddr_in6*>(sa);
      PopulateInet6Addr(sa_in6->sin6_addr, sa_in6->sin6_port, addr);
      break;
    }
    case AF_UNIX: {
      const auto* sa_un = reinterpret_cast<const struct sockaddr_un*>(sa);
      PopulateUnixAddr(sa_un->sun_path, -1, addr);
      break;
    }
    case AF_UNSPEC: {
      addr->family = SockAddrFamily::kUnspecified;
      addr->addr = std::monostate();
      break;
    }
    default: {
      addr->family = SockAddrFamily::kOther;
      addr->addr = std::monostate();
    }
  }
}

Status ParseIPAddress(std::string_view addr_str_view, InetAddr* ip_addr) {
  struct in_addr v4_addr = {};
  struct in6_addr v6_addr = {};

  if (ParseIPv4Addr(addr_str_view, &v4_addr).ok()) {
    ip_addr->family = InetAddrFamily::kIPv4;
    ip_addr->addr = v4_addr;
  } else if (ParseIPv6Addr(addr_str_view, &v6_addr).ok()) {
    ip_addr->family = InetAddrFamily::kIPv6;
    ip_addr->addr = v6_addr;
  } else {
    return error::InvalidArgument("Cannot parse input '$0' as IP address", addr_str_view);
  }

  return Status::OK();
}

namespace {

bool IPv4CIDRContains(struct in_addr cidr_ip, size_t prefix_length, struct in_addr ip) {
  return ntohl(cidr_ip.s_addr) >> (kIPv4BitLen - prefix_length) ==
         ntohl(ip.s_addr) >> (kIPv4BitLen - prefix_length);
}

bool IPv6CIDRContains(struct in6_addr cidr_ip, size_t prefix_length, struct in6_addr ip) {
  for (size_t i = 0; i < prefix_length; ++i) {
    int oct = i / 8;
    int bit = 7 - (i % 8);
    if ((cidr_ip.s6_addr[oct] & (1 << bit)) != (ip.s6_addr[oct] & (1 << bit))) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool CIDRContainsIPAddr(const CIDRBlock& block, const InetAddr& ip_addr) {
  if (block.ip_addr.family == InetAddrFamily::kIPv4 && ip_addr.family == InetAddrFamily::kIPv4) {
    return IPv4CIDRContains(std::get<struct in_addr>(block.ip_addr.addr), block.prefix_length,
                            std::get<struct in_addr>(ip_addr.addr));
  }

  if (block.ip_addr.family == InetAddrFamily::kIPv6 && ip_addr.family == InetAddrFamily::kIPv6) {
    return IPv6CIDRContains(std::get<struct in6_addr>(block.ip_addr.addr), block.prefix_length,
                            std::get<struct in6_addr>(ip_addr.addr));
  }

  // From this point on, we have mixed IP modes. Convert both to IPv6, then compare.
  CIDRBlock block6 = (block.ip_addr.family == InetAddrFamily::kIPv4) ? MapIPv4ToIPv6(block) : block;
  InetAddr ip_addr6 = (ip_addr.family == InetAddrFamily::kIPv4) ? MapIPv4ToIPv6(ip_addr) : ip_addr;

  DCHECK(block6.ip_addr.family == InetAddrFamily::kIPv6);
  DCHECK(ip_addr6.family == InetAddrFamily::kIPv6);

  return IPv6CIDRContains(std::get<struct in6_addr>(block6.ip_addr.addr), block6.prefix_length,
                          std::get<struct in6_addr>(ip_addr6.addr));
}

Status ParseCIDRBlock(std::string_view cidr_str, CIDRBlock* cidr) {
  std::vector<std::string_view> fields = absl::StrSplit(cidr_str, '/');
  if (fields.size() != 2) {
    return error::InvalidArgument("The format must be <ipv4/6 address>/<prefix length>, got: '$0'",
                                  cidr_str);
  }

  int prefix_length = 0;
  if (!absl::SimpleAtoi(fields[1], &prefix_length)) {
    return error::InvalidArgument("Could not parse $0 as integer", fields[1]);
  }
  if (prefix_length < 0) {
    return error::InvalidArgument("Prefix length must be >= 0, got: '$0'", prefix_length);
  }

  InetAddr addr;
  PX_RETURN_IF_ERROR(ParseIPAddress(fields[0], &addr));

  if (addr.family == InetAddrFamily::kIPv4 && prefix_length > kIPv4BitLen) {
    return error::InvalidArgument("Prefix length for IPv4 CIDR block must be <=$0, got: '$1'",
                                  kIPv4BitLen, prefix_length);
  }
  if (addr.family == InetAddrFamily::kIPv6 && prefix_length > kIPv6BitLen) {
    return error::InvalidArgument("Prefix length for IPv6 CIDR block must be <=$0, got: '$1'",
                                  kIPv6BitLen, prefix_length);
  }

  cidr->ip_addr = std::move(addr);
  cidr->prefix_length = prefix_length;
  return Status::OK();
}

CIDRBlock MapIPv4ToIPv6(const CIDRBlock& addr) {
  const int kBitPrefixLen = 96;
  return CIDRBlock{MapIPv4ToIPv6(addr.ip_addr), kBitPrefixLen + addr.prefix_length};
}

}  // namespace px
