/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/common/base/inet_utils.h"
#include "src/common/testing/testing.h"

namespace px {

using ::testing::StrEq;

TEST(InetUtils, PopulateInetAddr) {
  struct in_addr in_addr;
  inet_pton(AF_INET, "10.1.2.3", &in_addr);
  in_port_t in_port = htons(12345);

  SockAddr addr;
  PopulateInetAddr(in_addr, in_port, &addr);
  ASSERT_EQ(addr.family, SockAddrFamily::kIPv4);
  auto& addr4 = std::get<SockAddrIPv4>(addr.addr);
  EXPECT_EQ(addr4.AddrStr(), "10.1.2.3");
  EXPECT_EQ(addr4.port, 12345);
}

TEST(InetUtils, PopulateInet6Addr) {
  struct in6_addr in6_addr = IN6ADDR_LOOPBACK_INIT;
  in_port_t in6_port = htons(12345);

  SockAddr addr;
  PopulateInet6Addr(in6_addr, in6_port, &addr);
  ASSERT_EQ(addr.family, SockAddrFamily::kIPv6);
  auto& addr6 = std::get<SockAddrIPv6>(addr.addr);
  EXPECT_EQ(addr6.AddrStr(), "::1");
  EXPECT_EQ(addr6.port, 12345);
}

TEST(InetUtils, PopulateUnixAddr) {
  // Using sun_path rom struct sockaddr_un:
  // struct sockaddr_un {
  //   sa_family_t sun_family;               /* AF_UNIX */
  //   char        sun_path[108];            /* Pathname */
  // };
  const char path[108] = "/path/to/unix/domain/socket";
  uint32_t inode_num = 54321;

  SockAddr addr;
  PopulateUnixAddr(path, inode_num, &addr);
  auto& addr_un = std::get<SockAddrUnix>(addr.addr);
  EXPECT_EQ(addr_un.path, "/path/to/unix/domain/socket");
  EXPECT_EQ(addr_un.inode_num, 54321);
}

TEST(InetUtils, PopulateSockAddrInet4) {
  // Create an IP address for the test.
  struct sockaddr_in sockaddr;
  sockaddr.sin_family = AF_INET;
  inet_pton(AF_INET, "10.1.2.3", &sockaddr.sin_addr);
  sockaddr.sin_port = htons(53000);

  // Now check PopulateSockAddr produces the expected string.
  SockAddr addr;
  PopulateSockAddr(reinterpret_cast<struct sockaddr*>(&sockaddr), &addr);
  ASSERT_EQ(addr.family, SockAddrFamily::kIPv4);
  auto& addr4 = std::get<SockAddrIPv4>(addr.addr);
  EXPECT_EQ(addr4.AddrStr(), "10.1.2.3");
  EXPECT_EQ(addr4.port, 53000);
}

TEST(InetUtils, PopulateSockAddrInet6) {
  struct sockaddr_in6 sockaddr;
  sockaddr.sin6_family = AF_INET6;
  EXPECT_OK(ParseIPv6Addr("::1", &sockaddr.sin6_addr));
  sockaddr.sin6_port = htons(12345);

  SockAddr addr;
  PopulateSockAddr(reinterpret_cast<struct sockaddr*>(&sockaddr), &addr);
  ASSERT_EQ(addr.family, SockAddrFamily::kIPv6);
  auto& addr6 = std::get<SockAddrIPv6>(addr.addr);
  EXPECT_EQ(addr6.AddrStr(), "::1");
  EXPECT_EQ(addr6.port, 12345);
}

TEST(InetUtils, PopulateSockAddrUnix) {
  struct sockaddr_un sockaddr;
  sockaddr.sun_family = AF_UNIX;
  const char kUnixPath[108] = "/path/to/unix/domain/socket";
  memcpy(&sockaddr.sun_path, kUnixPath, sizeof(kUnixPath));

  SockAddr addr;
  PopulateSockAddr(reinterpret_cast<struct sockaddr*>(&sockaddr), &addr);
  ASSERT_EQ(addr.family, SockAddrFamily::kUnix);
  auto& addr_unix = std::get<SockAddrUnix>(addr.addr);
  EXPECT_EQ(addr_unix.path, "/path/to/unix/domain/socket");
  EXPECT_EQ(addr_unix.inode_num, -1);
}

TEST(InetUtils, PopulateSockAddrUnspecified) {
  struct sockaddr sockaddr;
  sockaddr.sa_family = AF_UNSPEC;

  SockAddr addr;
  PopulateSockAddr(reinterpret_cast<struct sockaddr*>(&sockaddr), &addr);
  ASSERT_EQ(addr.family, SockAddrFamily::kUnspecified);
}

TEST(ParseIPAddr, ipv4) {
  // Test address.
  struct in_addr in_addr;
  EXPECT_OK(ParseIPv4Addr("1.2.3.4", &in_addr));

  // Now check for the expected string.
  ASSERT_OK_AND_EQ(IPv4AddrToString(in_addr), "1.2.3.4");
}

TEST(InetAddr, ipv4) {
  InetAddr addr;

  ASSERT_OK(ParseIPAddress("1.2.3.4", &addr));
  ASSERT_FALSE(addr.IsLoopback());

  ASSERT_OK(ParseIPAddress("127.0.0.1", &addr));
  ASSERT_TRUE(addr.IsLoopback());
}

TEST(InetAddr, ipv6) {
  InetAddr addr;

  ASSERT_OK(ParseIPAddress("2001:0db8:85a3:0000:0000:8a2e:0370:7334", &addr));
  ASSERT_FALSE(addr.IsLoopback());

  ASSERT_OK(ParseIPAddress("::1", &addr));
  ASSERT_TRUE(addr.IsLoopback());
}

TEST(ParseIPAddr, ipv6) {
  // Test address.
  struct in6_addr in6_addr;
  EXPECT_OK(ParseIPv6Addr("2001:0db8:85a3:0000:0000:8a2e:0370:7334", &in6_addr));

  // Now check for the expected string.
  // Note that formatting is slightly different (zeros removed).
  ASSERT_OK_AND_EQ(IPv6AddrToString(in6_addr), "2001:db8:85a3::8a2e:370:7334");
}

TEST(ParseIPAddr, ipv4_mapped_into_ipv6) {
  // Test address.
  struct in6_addr in6_addr;
  EXPECT_OK(ParseIPv6Addr("::ffff:1.2.3.4", &in6_addr));

  // Now check for the expected string.
  // Note that formatting is slightly different (::ffff: removed).
  ASSERT_OK_AND_EQ(IPv6AddrToString(in6_addr), "1.2.3.4");
}

TEST(CIDRBlockTest, ContainsIPv4Address) {
  CIDRBlock block;
  ASSERT_OK(ParseCIDRBlock("1.2.3.4/24", &block));
  for (int i = 0; i < 256; ++i) {
    InetAddr addr;
    EXPECT_OK(ParseIPAddress(absl::StrCat("1.2.3.", i), &addr));
    EXPECT_TRUE(CIDRContainsIPAddr(block, addr));
  }
  for (int i = 0; i < 256; ++i) {
    InetAddr addr;
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
      InetAddr addr6_2;
      EXPECT_OK(ParseIPAddress(addr_str2, &addr6_2));
      EXPECT_TRUE(CIDRContainsIPAddr(block, addr6_2));
    }
  }
}

TEST(CIDRBlockTest, ContainsMixedAddress) {
  {
    CIDRBlock block6;
    InetAddr addr4;

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
    InetAddr addr6;

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
  InetAddr v4_addr;
  EXPECT_OK(ParseIPAddress("1.2.3.4", &v4_addr));
  {
    InetAddr v6_addr = MapIPv4ToIPv6(v4_addr);
    EXPECT_EQ("1.2.3.4", v6_addr.AddrStr());
  }
  {
    CIDRBlock v4_cidr{v4_addr, 10};
    CIDRBlock v6_cidr = MapIPv4ToIPv6(v4_cidr);
    EXPECT_EQ("1.2.3.4", v6_cidr.ip_addr.AddrStr());
  }
}

}  // namespace px
