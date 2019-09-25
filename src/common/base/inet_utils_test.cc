#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/inet_utils.h"

namespace pl {

TEST(ParseSockAddr, Basic) {
  // Create an IP address for the test.
  struct sockaddr_in sockaddr;
  sockaddr.sin_family = AF_INET;
  inet_pton(AF_INET, "10.1.2.3", &sockaddr.sin_addr);
  sockaddr.sin_port = htons(53000);

  // Now check InetAddrToString produces the expected string.
  std::string addr;
  int port;
  Status s = ParseSockAddr(*reinterpret_cast<struct sockaddr*>(&sockaddr), &addr, &port);
  EXPECT_OK(s);
  EXPECT_EQ(addr, "10.1.2.3");
  EXPECT_EQ(port, 53000);
}

TEST(ParseSockAddr, Unsupported) {
  // Create an IP address for the test.
  struct sockaddr_in sockaddr;
  sockaddr.sin_family = AF_UNIX;
  inet_pton(AF_INET, "10.1.2.3", &sockaddr.sin_addr);
  sockaddr.sin_port = htons(53000);

  std::string addr;
  int port;
  Status s = ParseSockAddr(*reinterpret_cast<struct sockaddr*>(&sockaddr), &addr, &port);
  EXPECT_NOT_OK(s);
}

}  // namespace pl
