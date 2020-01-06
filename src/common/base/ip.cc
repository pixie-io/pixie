#include "src/common/base/ip.h"

#include <arpa/inet.h>

#include "src/common/base/inet_utils.h"
#include "src/common/base/utils.h"

namespace pl {

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
  const size_t kIPv4BitLen = 32;
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

}  // namespace pl
