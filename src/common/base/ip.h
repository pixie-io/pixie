#pragma once

#include <arpa/inet.h>

#include <string>
#include <string_view>
#include <variant>

#include "src/common/base/statusor.h"

namespace pl {

struct IPv4Address {
  std::string str;
  struct in_addr in_addr;

  static StatusOr<IPv4Address> FromStr(std::string_view addr_str);
};

struct IPv6Address {
  std::string str;
  struct in6_addr in6_addr;

  static StatusOr<IPv6Address> FromStr(std::string_view addr_str);
};

/**
 * Classless Inter Domain Routing Block. Follows the notations at:
 * https://en.wikipedia.org/wiki/Classless_Inter-Domain_Routing
 */
class CIDRBlock {
 public:
  /**
   * Returns a CIDRBlock that matches the input string.
   */
  static StatusOr<CIDRBlock> FromStr(std::string_view cidr_str);

  CIDRBlock(IPv4Address addr, size_t prefix_length);
  CIDRBlock(IPv6Address addr, size_t prefix_length);

  // For StatusOr<CIDRBlock> to compile.
  CIDRBlock() = default;

  bool Contains(const IPv4Address& addr) const;
  bool Contains(const IPv6Address& addr) const;

 private:
  std::variant<IPv4Address, IPv6Address> addr_;
  size_t prefix_length_;
};

}  // namespace pl
