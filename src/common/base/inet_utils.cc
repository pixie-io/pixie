#include "src/common/base/inet_utils.h"

#include <vector>

namespace pl {

const int kIPv4BitLen = 32;
const int kIPv6BitLen = 128;

std::string SockAddr::AddrStr() const {
  std::string out;

  Status s;
  switch (family) {
    case SockAddrFamily::kUnspecified:
      out = "-";
      break;
    case SockAddrFamily::kIPv4:
      s = IPv4AddrToString(std::get<struct in_addr>(addr), &out);
      break;
    case SockAddrFamily::kIPv6:
      s = IPv6AddrToString(std::get<struct in6_addr>(addr), &out);
      break;
    case SockAddrFamily::kUnix:
      out = absl::StrCat("unix_socket:", std::get<std::string>(addr));
      break;
    case SockAddrFamily::kOther:
      out = "other";
      break;
  }

  if (!s.ok()) {
    out = s.msg();
  }

  return out;
}

SockAddr MapIPv4ToIPv6(const SockAddr& addr) {
  DCHECK(addr.family == SockAddrFamily::kIPv4);

  struct in6_addr mapped_addr = {{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0, 0, 0, 0}}};
  mapped_addr.s6_addr32[kIPv4Offset] = std::get<struct in_addr>(addr.addr).s_addr;

  SockAddr v6_addr;
  v6_addr.family = SockAddrFamily::kIPv6;
  v6_addr.addr = mapped_addr;
  v6_addr.port = addr.port;
  return v6_addr;
}

void PopulateInetAddr(struct in_addr in_addr, in_port_t port, SockAddr* addr) {
  addr->family = SockAddrFamily::kIPv4;
  addr->addr = in_addr;
  addr->port = ntohs(port);
}

void PopulateInet6Addr(struct in6_addr in6_addr, in_port_t port, SockAddr* addr) {
  addr->family = SockAddrFamily::kIPv6;
  addr->addr = in6_addr;
  addr->port = ntohs(port);
}

void PopulateUnixAddr(const char* sun_path, uint32_t inode_num, SockAddr* addr) {
  addr->family = SockAddrFamily::kUnix;
  addr->addr = std::string(sun_path);
  addr->port = inode_num;
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
      addr->addr = {};
      addr->port = -1;
      break;
    }
    default: {
      addr->family = SockAddrFamily::kOther;
      addr->addr = {};
      addr->port = -1;
    }
  }
}

Status ParseIPAddress(std::string_view addr_str_view, SockAddr* ip_addr) {
  struct in_addr v4_addr = {};
  struct in6_addr v6_addr = {};

  if (ParseIPv4Addr(addr_str_view, &v4_addr).ok()) {
    ip_addr->family = SockAddrFamily::kIPv4;
    ip_addr->addr = v4_addr;
  } else if (ParseIPv6Addr(addr_str_view, &v6_addr).ok()) {
    ip_addr->family = SockAddrFamily::kIPv6;
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

bool CIDRContainsIPAddr(const CIDRBlock& block, const SockAddr& ip_addr) {
  if (block.ip_addr.family == SockAddrFamily::kIPv4 && ip_addr.family == SockAddrFamily::kIPv4) {
    return IPv4CIDRContains(std::get<struct in_addr>(block.ip_addr.addr), block.prefix_length,
                            std::get<struct in_addr>(ip_addr.addr));
  }

  if (block.ip_addr.family == SockAddrFamily::kIPv6 && ip_addr.family == SockAddrFamily::kIPv6) {
    return IPv6CIDRContains(std::get<struct in6_addr>(block.ip_addr.addr), block.prefix_length,
                            std::get<struct in6_addr>(ip_addr.addr));
  }

  // From this point on, we have mixed IP modes. Convert both to IPv6, then compare.
  CIDRBlock block6 = (block.ip_addr.family == SockAddrFamily::kIPv4) ? MapIPv4ToIPv6(block) : block;
  SockAddr ip_addr6 = (ip_addr.family == SockAddrFamily::kIPv4) ? MapIPv4ToIPv6(ip_addr) : ip_addr;

  DCHECK(block6.ip_addr.family == SockAddrFamily::kIPv6);
  DCHECK(ip_addr6.family == SockAddrFamily::kIPv6);

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

  SockAddr addr;
  PL_RETURN_IF_ERROR(ParseIPAddress(fields[0], &addr));

  if (addr.family == SockAddrFamily::kIPv4 && prefix_length > kIPv4BitLen) {
    return error::InvalidArgument("Prefix length for IPv4 CIDR block must be <=$0, got: '$1'",
                                  kIPv4BitLen, prefix_length);
  }
  if (addr.family == SockAddrFamily::kIPv6 && prefix_length > kIPv6BitLen) {
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
