#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <absl/strings/str_cat.h>

#include "src/common/base/ip.h"
#include "src/common/testing/testing.h"

namespace pl {

using ::testing::StrEq;

TEST(IPTest, ParseIPv4Address) { EXPECT_OK(IPv4Address::FromStr("1.2.3.4")); }

TEST(IPTest, ParseIPv6Address) { EXPECT_OK(IPv6Address::FromStr("::1")); }

TEST(CIDRBlockTest, ContainsIPv4Address) {
  ASSERT_OK_AND_ASSIGN(IPv4Address addr, IPv4Address::FromStr("1.2.3.4"));
  CIDRBlock block(addr, 24);
  for (int i = 0; i < 256; ++i) {
    ASSERT_OK_AND_ASSIGN(IPv4Address addr, IPv4Address::FromStr(absl::StrCat("1.2.3.", i)));
    EXPECT_TRUE(block.Contains(addr));
  }
  for (int i = 0; i < 256; ++i) {
    ASSERT_OK_AND_ASSIGN(IPv4Address addr, IPv4Address::FromStr(absl::StrCat("1.2.4.", i)));
    EXPECT_FALSE(block.Contains(addr));
  }
}

TEST(CIDRBlockTest, ContainsIPv6Address) {
  std::string addr_str = "1111:1112:1113:1114:1115:1116:1117:1100";
  ASSERT_OK_AND_ASSIGN(IPv6Address addr6, IPv6Address::FromStr(addr_str));
  CIDRBlock block(addr6, 120);
  for (char a = 'a'; a <= 'f'; ++a) {
    for (char b = 'a'; b <= 'f'; ++b) {
      std::string addr_str2 = "1111:1112:1113:1114:1115:1116:1117:11";
      addr_str2 += a;
      addr_str2 += b;
      ASSERT_OK_AND_ASSIGN(IPv6Address addr6_2, IPv6Address::FromStr(addr_str2));
      EXPECT_TRUE(block.Contains(addr6_2));
    }
  }
}

TEST(CIDRBlockTest, ParsesIPv4String) {
  EXPECT_OK(CIDRBlock::FromStr("1.2.3.4/32"));
  EXPECT_OK(CIDRBlock::FromStr("1.2.3.4/0"));
  EXPECT_THAT(CIDRBlock::FromStr("1.2.3.4/-1").msg(),
              StrEq("Prefix length must be >= 0, got: '-1'"));
  EXPECT_THAT(CIDRBlock::FromStr("1.2.3.4/33").msg(),
              StrEq("Prefix length for IPv4 CIDR block must be <=32, got: '33'"));
}

TEST(CIDRBlockTest, ParsesIPv6String) {
  EXPECT_OK(CIDRBlock::FromStr("::1/0"));
  EXPECT_OK(CIDRBlock::FromStr("::1/128"));
  EXPECT_THAT(CIDRBlock::FromStr("::1/-1").msg(), StrEq("Prefix length must be >= 0, got: '-1'"));
  EXPECT_THAT(CIDRBlock::FromStr("::1/129").msg(),
              StrEq("Prefix length for IPv6 CIDR block must be <=128, got: '129'"));
}

TEST(CIDRBlockTest, ParseInvalidIPAddressString) {
  EXPECT_THAT(CIDRBlock::FromStr("non-ip/0").msg(), StrEq("Cannot parse IP address: 'non-ip'"));
  EXPECT_THAT(CIDRBlock::FromStr("aaa").msg(),
              StrEq("The format must be <ipv4/6 address>/<prefix length>, got: 'aaa'"));
}

}  // namespace pl
