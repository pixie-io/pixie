#include "src/common/base/inet_utils.h"

#include <vector>

#include "src/common/base/statusor.h"

namespace pl {

const int kIPv4BitLen = 32;
const int kIPv6BitLen = 128;

IPAddress MapIPv4ToIPv6(const IPAddress& addr) {
  DCHECK(addr.version == IPVersion::kIPv4);
  DCHECK(!addr.addr_str.empty());
  const std::string v6_addr_str = absl::StrCat("::ffff:", addr.addr_str);
  IPAddress v6_addr;
  ECHECK_OK(ParseIPAddress(v6_addr_str, &v6_addr));
  return v6_addr;
}

Status ParseSockAddr(const struct sockaddr* sa, IPAddress* addr) {
  switch (sa->sa_family) {
    case AF_INET: {
      const auto* sa_in = reinterpret_cast<const struct sockaddr_in*>(sa);
      PL_RETURN_IF_ERROR(IPv4AddrToString(sa_in->sin_addr, &addr->addr_str));
      addr->version = IPVersion::kIPv4;
      addr->in_addr = sa_in->sin_addr;
      addr->port = ntohs(sa_in->sin_port);
      return Status::OK();
    }
    case AF_INET6: {
      const auto* sa_in6 = reinterpret_cast<const struct sockaddr_in6*>(sa);
      PL_RETURN_IF_ERROR(IPv6AddrToString(sa_in6->sin6_addr, &addr->addr_str));
      addr->version = IPVersion::kIPv6;
      addr->in_addr = sa_in6->sin6_addr;
      addr->port = ntohs(sa_in6->sin6_port);
      return Status::OK();
    }
    default:
      return error::InvalidArgument("Unhandled sockaddr family: $0", sa->sa_family);
  }
}

Status ParseIPAddress(std::string_view addr_str_view, IPAddress* ip_addr) {
  struct in_addr v4_addr = {};
  struct in6_addr v6_addr = {};

  if (ParseIPv4Addr(addr_str_view, &v4_addr).ok()) {
    ip_addr->version = IPVersion::kIPv4;
    ip_addr->in_addr = v4_addr;
  } else if (ParseIPv6Addr(addr_str_view, &v6_addr).ok()) {
    ip_addr->version = IPVersion::kIPv6;
    ip_addr->in_addr = v6_addr;
  } else {
    return error::InvalidArgument("Cannot parse input '$0' as IP address", addr_str_view);
  }
  ip_addr->addr_str = addr_str_view;

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

bool CIDRContainsIPAddr(const CIDRBlock& block, const IPAddress& ip_addr) {
  DCHECK(block.ip_addr.version == ip_addr.version) << absl::Substitute(
      "CIDRBlock IPVersion: '$0' IP address: '$1'", magic_enum::enum_name(block.ip_addr.version),
      magic_enum::enum_name(ip_addr.version));
  switch (block.ip_addr.version) {
    case IPVersion::kIPv4:
      return IPv4CIDRContains(std::get<struct in_addr>(block.ip_addr.in_addr), block.prefix_length,
                              std::get<struct in_addr>(ip_addr.in_addr));
    case IPVersion::kIPv6:
      return IPv6CIDRContains(std::get<struct in6_addr>(block.ip_addr.in_addr), block.prefix_length,
                              std::get<struct in6_addr>(ip_addr.in_addr));
  }
  return false;
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

  IPAddress addr;
  PL_RETURN_IF_ERROR(ParseIPAddress(fields[0], &addr));

  if (addr.version == IPVersion::kIPv4 && prefix_length > kIPv4BitLen) {
    return error::InvalidArgument("Prefix length for IPv4 CIDR block must be <=$0, got: '$1'",
                                  kIPv4BitLen, prefix_length);
  }
  if (addr.version == IPVersion::kIPv6 && prefix_length > kIPv6BitLen) {
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

}  // namespace pl
