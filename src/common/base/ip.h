#pragma once

// TODO(yzhao): Merge this and .cc into inet_utils.{h,cc}.

#include <arpa/inet.h>

#include <string>
#include <string_view>
#include <variant>

#include "src/common/base/inet_utils.h"
#include "src/common/base/statusor.h"

namespace pl {

// TODO(yzhao): Remove this as it's superseded by IPAddress in inet_utils.h.
struct IPv4Address {
  std::string str;
  struct in_addr in_addr;

  static StatusOr<IPv4Address> FromStr(std::string_view addr_str);
};

// TODO(yzhao): Remove this as it's superseded by IPAddress in inet_utils.h.
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
  CIDRBlock() : addr_(), prefix_length_(0) {}

  // TODO(yzhao): Remove.
  bool Contains(const IPv4Address& addr) const;
  // TODO(yzhao): Remove.
  bool Contains(const IPv6Address& addr) const;

  bool Contains(const IPAddress& addr) const;

 private:
  std::variant<IPv4Address, IPv6Address> addr_;
  size_t prefix_length_;
};

}  // namespace pl
