#include "src/common/base/inet_utils.h"
#include "src/common/testing/testing.h"

namespace pl {

using ::testing::StrEq;

TEST(PopulateInetAddr, Basic) {
  struct in_addr in_addr;
  inet_pton(AF_INET, "10.1.2.3", &in_addr);
  in_port_t in_port = htons(12345);

  SockAddr addr;
  PopulateInetAddr(in_addr, in_port, &addr);
  EXPECT_EQ(addr.AddrStr(), "10.1.2.3");
  EXPECT_EQ(addr.port, 12345);
}

TEST(PopulateInet6Addr, Basic) {
  struct in6_addr in6_addr = IN6ADDR_LOOPBACK_INIT;
  in_port_t in6_port = htons(12345);

  SockAddr addr;
  PopulateInet6Addr(in6_addr, in6_port, &addr);
  EXPECT_EQ(addr.AddrStr(), "::1");
  EXPECT_EQ(addr.port, 12345);
}

TEST(PopulateUnixAddr, Basic) {
  // Using sun_path rom struct sockaddr_un:
  // struct sockaddr_un {
  //   sa_family_t sun_family;               /* AF_UNIX */
  //   char        sun_path[108];            /* Pathname */
  // };
  const char path[108] = "/path/to/unix/domain/socket";
  uint32_t inode_num = 54321;

  SockAddr addr;
  PopulateUnixAddr(path, inode_num, &addr);
  EXPECT_EQ(addr.AddrStr(), "unix_socket");
  EXPECT_EQ(addr.port, 54321);
}

TEST(ParseSockAddr, IPv4) {
  // Create an IP address for the test.
  struct sockaddr_in sockaddr;
  sockaddr.sin_family = AF_INET;
  inet_pton(AF_INET, "10.1.2.3", &sockaddr.sin_addr);
  sockaddr.sin_port = htons(53000);

  // Now check InetAddrToString produces the expected string.
  SockAddr addr;
  ASSERT_OK(PopulateSockAddr(reinterpret_cast<struct sockaddr*>(&sockaddr), &addr));
  EXPECT_EQ(addr.AddrStr(), "10.1.2.3");
  EXPECT_EQ(addr.port, 53000);
}

TEST(ParseSockAddr, IPv6) {
  struct sockaddr_in6 sockaddr;
  sockaddr.sin6_family = AF_INET6;
  EXPECT_OK(ParseIPv6Addr("::1", &sockaddr.sin6_addr));
  sockaddr.sin6_port = htons(12345);

  SockAddr addr;
  ASSERT_OK(PopulateSockAddr(reinterpret_cast<struct sockaddr*>(&sockaddr), &addr));
  EXPECT_EQ(addr.AddrStr(), "::1");
  EXPECT_EQ(addr.port, 12345);
}

TEST(ParseSockAddr, Unix) {
  struct sockaddr_un sockaddr;
  sockaddr.sun_family = AF_UNIX;
  const char kUnixPath[108] = "/path/to/unix/domain/socket";
  memcpy(&sockaddr.sun_path, kUnixPath, sizeof(kUnixPath));

  SockAddr addr;
  ASSERT_OK(PopulateSockAddr(reinterpret_cast<struct sockaddr*>(&sockaddr), &addr));
  EXPECT_EQ(addr.AddrStr(), "unix_socket");
  EXPECT_EQ(addr.port, -1);
}

TEST(ParseSockAddr, Unsupported) {
  // Create an IP address for the test.
  struct sockaddr_in sockaddr;
  sockaddr.sin_family = AF_UNSPEC;
  inet_pton(AF_INET, "10.1.2.3", &sockaddr.sin_addr);
  sockaddr.sin_port = htons(53000);

  SockAddr addr;
  EXPECT_NOT_OK(PopulateSockAddr(reinterpret_cast<struct sockaddr*>(&sockaddr), &addr));
  EXPECT_EQ(addr.family, SockAddrFamily::kUninitialized) << "addr should not be mutated";
}

TEST(ParseIPAddr, ipv4) {
  // Test address.
  struct in_addr in_addr;
  EXPECT_OK(ParseIPv4Addr("1.2.3.4", &in_addr));

  // Now check for the expected string.
  std::string addr;
  Status s = IPv4AddrToString(in_addr, &addr);
  EXPECT_OK(s);
  EXPECT_EQ(addr, "1.2.3.4");
}

TEST(ParseIPAddr, ipv6) {
  // Test address.
  struct in6_addr in6_addr;
  EXPECT_OK(ParseIPv6Addr("2001:0db8:85a3:0000:0000:8a2e:0370:7334", &in6_addr));

  // Now check for the expected string.
  std::string addr;
  Status s = IPv6AddrToString(in6_addr, &addr);
  EXPECT_OK(s);
  // Note that formatting is slightly different (zeros removed).
  EXPECT_EQ(addr, "2001:db8:85a3::8a2e:370:7334");
}

TEST(ParseIPAddr, ipv4_mapped_into_ipv6) {
  // Test address.
  struct in6_addr in6_addr;
  EXPECT_OK(ParseIPv6Addr("::ffff:1.2.3.4", &in6_addr));

  // Now check for the expected string.
  std::string addr;
  Status s = IPv6AddrToString(in6_addr, &addr);
  EXPECT_OK(s);
  // Note that formatting is slightly different (zeros removed).
  EXPECT_EQ(addr, "1.2.3.4");
}

TEST(ParseIPAddr, ipv4_using_in6_addr) {
  // Create an IP address for the test.
  struct in6_addr in6_addr;
  EXPECT_OK(ParseIPv4Addr("1.2.3.4", &in6_addr));

  // Now check for the expected string.
  std::string addr;
  Status s = IPv4AddrToString(in6_addr, &addr);
  EXPECT_OK(s);
  // Note that formatting is slightly different (zeros removed).
  EXPECT_EQ(addr, "1.2.3.4");
}

TEST(CIDRBlockTest, ContainsIPv4Address) {
  CIDRBlock block;
  ASSERT_OK(ParseCIDRBlock("1.2.3.4/24", &block));
  for (int i = 0; i < 256; ++i) {
    SockAddr addr;
    EXPECT_OK(ParseIPAddress(absl::StrCat("1.2.3.", i), &addr));
    EXPECT_TRUE(CIDRContainsIPAddr(block, addr));
  }
  for (int i = 0; i < 256; ++i) {
    SockAddr addr;
    EXPECT_OK(ParseIPAddress(absl::StrCat("1.2.4.", i), &addr));
    EXPECT_FALSE(CIDRContainsIPAddr(block, addr));
  }
}

TEST(CIDRBlockTest, ContainsIPv6Address) {
  CIDRBlock block;
  ASSERT_OK(ParseCIDRBlock("1111:1112:1113:1114:1115:1116:1117:1100/120", &block));
  for (char a = 'a'; a <= 'f'; ++a) {
    for (char b = 'a'; b <= 'f'; ++b) {
      std::string addr_str2 = "1111:1112:1113:1114:1115:1116:1117:11";
      addr_str2 += a;
      addr_str2 += b;
      SockAddr addr6_2;
      EXPECT_OK(ParseIPAddress(addr_str2, &addr6_2));
      EXPECT_TRUE(CIDRContainsIPAddr(block, addr6_2));
    }
  }
}

TEST(CIDRBlockTest, ContainsMixedAddress) {
  {
    CIDRBlock block6;
    SockAddr addr4;

    ASSERT_OK(ParseCIDRBlock("::ffff:10.64.0.0/16", &block6));
    ASSERT_OK(ParseIPAddress("10.64.5.1", &addr4));
    EXPECT_TRUE(CIDRContainsIPAddr(block6, addr4));

    ASSERT_OK(ParseCIDRBlock("::ffff:10.64.0.0/112", &block6));
    ASSERT_OK(ParseIPAddress("10.65.5.1", &addr4));
    EXPECT_FALSE(CIDRContainsIPAddr(block6, addr4));

    ASSERT_OK(ParseCIDRBlock("1111:1112:1113:1114:1115:1116:1117:1100/120", &block6));
    ASSERT_OK(ParseIPAddress("10.64.5.1", &addr4));
    EXPECT_FALSE(CIDRContainsIPAddr(block6, addr4));
  }

  {
    CIDRBlock block4;
    SockAddr addr6;

    ASSERT_OK(ParseCIDRBlock("10.64.0.0/16", &block4));
    EXPECT_OK(ParseIPAddress("::ffff:10.64.5.1", &addr6));
    EXPECT_TRUE(CIDRContainsIPAddr(block4, addr6));

    ASSERT_OK(ParseCIDRBlock("10.64.0.0/16", &block4));
    EXPECT_OK(ParseIPAddress("::ffff:10.65.5.1", &addr6));
    EXPECT_FALSE(CIDRContainsIPAddr(block4, addr6));

    ASSERT_OK(ParseCIDRBlock("10.64.0.0/16", &block4));
    ASSERT_OK(ParseIPAddress("1111:1112:1113:1114:1115:1116:1117:1100", &addr6));
    EXPECT_FALSE(CIDRContainsIPAddr(block4, addr6));
  }
}

TEST(CIDRBlockTest, ParsesIPv4String) {
  CIDRBlock block;
  EXPECT_OK(ParseCIDRBlock("1.2.3.4/32", &block));
  EXPECT_OK(ParseCIDRBlock("1.2.3.4/0", &block));
  EXPECT_THAT(ParseCIDRBlock("1.2.3.4/-1", &block).msg(),
              StrEq("Prefix length must be >= 0, got: '-1'"));
  EXPECT_THAT(ParseCIDRBlock("1.2.3.4/33", &block).msg(),
              StrEq("Prefix length for IPv4 CIDR block must be <=32, got: '33'"));
}

TEST(CIDRBlockTest, ParsesIPv6String) {
  CIDRBlock block;
  EXPECT_OK(ParseCIDRBlock("::1/0", &block));
  EXPECT_OK(ParseCIDRBlock("::1/128", &block));
  EXPECT_THAT(ParseCIDRBlock("::1/-1", &block).msg(),
              StrEq("Prefix length must be >= 0, got: '-1'"));
  EXPECT_THAT(ParseCIDRBlock("::1/129", &block).msg(),
              StrEq("Prefix length for IPv6 CIDR block must be <=128, got: '129'"));
}

TEST(CIDRBlockTest, ParseInvalidIPAddressString) {
  CIDRBlock block;
  EXPECT_THAT(ParseCIDRBlock("", &block).msg(),
              StrEq("The format must be <ipv4/6 address>/<prefix length>, got: ''"));
  EXPECT_THAT(ParseCIDRBlock("non-ip/0", &block).msg(),
              StrEq("Cannot parse input 'non-ip' as IP address"));
  EXPECT_THAT(ParseCIDRBlock("aaa", &block).msg(),
              StrEq("The format must be <ipv4/6 address>/<prefix length>, got: 'aaa'"));
}

TEST(MapIPv4ToIPv6Test, WorksAsExpected) {
  SockAddr v4_addr;
  EXPECT_OK(ParseIPAddress("1.2.3.4", &v4_addr));
  {
    SockAddr v6_addr = MapIPv4ToIPv6(v4_addr);
    EXPECT_EQ("1.2.3.4", v6_addr.AddrStr());
  }
  {
    CIDRBlock v4_cidr{v4_addr, 10};
    CIDRBlock v6_cidr = MapIPv4ToIPv6(v4_cidr);
    EXPECT_EQ("1.2.3.4", v6_cidr.ip_addr.AddrStr());
  }
}

}  // namespace pl
