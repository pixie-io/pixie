#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <absl/strings/str_cat.h>

#include "src/common/base/ip.h"
#include "src/common/testing/testing.h"

namespace pl {

using ::testing::StrEq;

TEST(IPTest, ParseIPv4Address) {
  StatusOr<IPv4Address> addr_or = IPv4Address::FromStr("1.2.3.4");
  EXPECT_OK(addr_or);
}

TEST(IPTest, ParseIPv6Address) {
  StatusOr<IPv6Address> addr6_or = IPv6Address::FromStr("::1");
  EXPECT_OK(addr6_or);
}

TEST(CIDRBlockTest, ContainsIPv4Address) {
  StatusOr<IPv4Address> addr_or = IPv4Address::FromStr("1.2.3.4");
  EXPECT_OK(addr_or);
  CIDRBlock block(addr_or.ValueOrDie(), 24);
  for (int i = 0; i < 256; ++i) {
    StatusOr<IPv4Address> addr_or = IPv4Address::FromStr(absl::StrCat("1.2.3.", i));
    ASSERT_OK(addr_or);
    EXPECT_TRUE(block.Contains(addr_or.ValueOrDie()));
  }
  for (int i = 0; i < 256; ++i) {
    StatusOr<IPv4Address> addr_or = IPv4Address::FromStr(absl::StrCat("1.2.4.", i));
    ASSERT_OK(addr_or);
    EXPECT_FALSE(block.Contains(addr_or.ValueOrDie()));
  }
}

TEST(CIDRBlockTest, ContainsIPv6Address) {
  std::string addr_str = "1111:1112:1113:1114:1115:1116:1117:1100";
  StatusOr<IPv6Address> addr6_or = IPv6Address::FromStr(addr_str);
  EXPECT_OK(addr6_or);
  CIDRBlock block(addr6_or.ValueOrDie(), 120);
  for (char a = 'a'; a <= 'f'; ++a) {
    for (char b = 'a'; b <= 'f'; ++b) {
      std::string addr_str2 = "1111:1112:1113:1114:1115:1116:1117:11";
      addr_str2 += a;
      addr_str2 += b;
      StatusOr<IPv6Address> addr6_or2 = IPv6Address::FromStr(addr_str2);
      ASSERT_OK(addr6_or2);
      EXPECT_TRUE(block.Contains(addr6_or2.ValueOrDie()));
    }
  }
}

TEST(CIDRBlockTest, ParsesIPv4String) {
  EXPECT_OK(CIDRBlock::FromStr("1.2.3.4/32"));
  EXPECT_OK(CIDRBlock::FromStr("1.2.3.4/0"));
  EXPECT_THAT(CIDRBlock::FromStr("1.2.3.4/-1").msg(), StrEq("Prefix length must be >= 0, got: -1"));
  EXPECT_THAT(CIDRBlock::FromStr("1.2.3.4/33").msg(),
              StrEq("Prefix length for IPv4 CIDR block must be <=32, got: 33"));
}

TEST(CIDRBlockTest, ParsesIPv6String) {
  EXPECT_OK(CIDRBlock::FromStr("::1/0"));
  EXPECT_OK(CIDRBlock::FromStr("::1/128"));
  EXPECT_THAT(CIDRBlock::FromStr("::1/-1").msg(), StrEq("Prefix length must be >= 0, got: -1"));
  EXPECT_THAT(CIDRBlock::FromStr("::1/129").msg(),
              StrEq("Prefix length for IPv6 CIDR block must be <=128, got: 129"));
}

}  // namespace pl
