#include "src/common/base/ip.h"

#include <absl/strings/numbers.h>
#include <absl/strings/str_split.h>
#include <arpa/inet.h>

#include <vector>

#include "src/common/base/inet_utils.h"
#include "src/common/base/utils.h"

namespace pl {

const int kIPv4BitLen = 32;
const int kIPv6BitLen = 128;

StatusOr<IPv4Address> IPv4Address::FromStr(std::string_view addr_str) {
  IPv4Address addr;
  addr.str = addr_str;
  PL_RETURN_IF_ERROR(ParseIPv4Addr(addr.str, &addr.in_addr));
  return addr;
}

StatusOr<IPv6Address> IPv6Address::FromStr(std::string_view addr_str) {
  IPv6Address addr;
  addr.str = addr_str;
  PL_RETURN_IF_ERROR(ParseIPv6Addr(addr.str, &addr.in6_addr));
  return addr;
}

CIDRBlock::CIDRBlock(IPv4Address addr, size_t prefix_length)
    : addr_(addr), prefix_length_(prefix_length) {}

CIDRBlock::CIDRBlock(IPv6Address addr, size_t prefix_length)
    : addr_(addr), prefix_length_(prefix_length) {}

bool CIDRBlock::Contains(const IPv4Address& addr) const {
  if (!std::holds_alternative<IPv4Address>(addr_)) {
    return false;
  }
  const auto& my_addr = std::get<IPv4Address>(addr_);
  if ((ntohl(addr.in_addr.s_addr) >> (kIPv4BitLen - prefix_length_)) !=
      (ntohl(my_addr.in_addr.s_addr) >> (kIPv4BitLen - prefix_length_))) {
    return false;
  }
  return true;
}

bool CIDRBlock::Contains(const IPv6Address& addr) const {
  if (!std::holds_alternative<IPv6Address>(addr_)) {
    return false;
  }
  const auto& my_addr = std::get<IPv6Address>(addr_);
  for (size_t i = 0; i < prefix_length_; ++i) {
    int oct = i / 8;
    int bit = 7 - (i % 8);
    if ((my_addr.in6_addr.s6_addr[oct] & (1 << bit)) != (addr.in6_addr.s6_addr[oct] & (1 << bit))) {
      return false;
    }
  }
  return true;
}

StatusOr<CIDRBlock> CIDRBlock::FromStr(std::string_view cidr_str) {
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
  StatusOr<IPv4Address> v4_addr_or = IPv4Address::FromStr(fields[0]);
  if (v4_addr_or.ok()) {
    if (prefix_length > kIPv4BitLen) {
      return error::InvalidArgument("Prefix length for IPv4 CIDR block must be <=$0, got: '$1'",
                                    kIPv4BitLen, prefix_length);
    }
    return CIDRBlock(v4_addr_or.ConsumeValueOrDie(), prefix_length);
  }
  StatusOr<IPv6Address> v6_addr_or = IPv6Address::FromStr(fields[0]);
  if (v6_addr_or.ok()) {
    if (prefix_length > kIPv6BitLen) {
      return error::InvalidArgument("Prefix length for IPv6 CIDR block must be <=$0, got: '$1'",
                                    kIPv6BitLen, prefix_length);
    }
    return CIDRBlock(v6_addr_or.ConsumeValueOrDie(), prefix_length);
  }
  return error::InvalidArgument("Cannot parse IP address: '$0'", fields[0]);
}

}  // namespace pl
