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
    : version_(IPVersion::kIPv4), addr_(addr), prefix_length_(prefix_length) {}

CIDRBlock::CIDRBlock(IPv6Address addr, size_t prefix_length)
    : version_(IPVersion::kIPv6), addr_(addr), prefix_length_(prefix_length) {}

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

bool CIDRBlock::Contains(const IPv4Address& addr) const {
  if (!std::holds_alternative<IPv4Address>(addr_)) {
    return false;
  }
  const auto& my_addr = std::get<IPv4Address>(addr_);
  return IPv4CIDRContains(my_addr.in_addr, prefix_length_, addr.in_addr);
}

bool CIDRBlock::Contains(const IPv6Address& addr) const {
  if (!std::holds_alternative<IPv6Address>(addr_)) {
    return false;
  }
  const auto& my_addr = std::get<IPv6Address>(addr_);
  return IPv6CIDRContains(my_addr.in6_addr, prefix_length_, addr.in6_addr);
}

bool CIDRBlock::Contains(const IPAddress& addr) const {
  if (std::holds_alternative<IPv4Address>(addr_) &&
      !std::holds_alternative<struct in_addr>(addr.addr)) {
    return false;
  }
  if (std::holds_alternative<IPv6Address>(addr_) &&
      !std::holds_alternative<struct in6_addr>(addr.addr)) {
    return false;
  }
  if (std::holds_alternative<IPv4Address>(addr_)) {
    const auto& cidr_ip = std::get<IPv4Address>(addr_).in_addr;
    const auto& ip = std::get<struct in_addr>(addr.addr);
    return IPv4CIDRContains(cidr_ip, prefix_length_, ip);
  }
  if (std::holds_alternative<IPv6Address>(addr_)) {
    const auto& cidr_ip = std::get<IPv6Address>(addr_).in6_addr;
    const auto& ip = std::get<struct in6_addr>(addr.addr);
    return IPv6CIDRContains(cidr_ip, prefix_length_, ip);
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
