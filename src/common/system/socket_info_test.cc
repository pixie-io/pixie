#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>
#include <string>

#include "absl/strings/numbers.h"
#include "src/common/base/inet_utils.h"
#include "src/common/system/socket_info.h"
#include "src/common/system/testing/tcp_socket.h"

namespace pl {
namespace system {

using ::testing::Contains;
using ::testing::Not;

// Keep two versions of AddrPortStr, in case the host machine is using IPv6.
std::string AddrPortStr(struct in6_addr in_addr, in_port_t in_port) {
  std::string addr;
  int port;

  Status s = ParseIPv6Addr(in_addr, &addr);
  CHECK(s.ok());
  port = ntohs(in_port);

  return absl::StrCat(addr, ":", port);
}

std::string AddrPortStr(struct in_addr in_addr, in_port_t in_port) {
  std::string addr;
  int port;

  Status s = ParseIPv4Addr(in_addr, &addr);
  CHECK(s.ok());
  port = ntohs(in_port);

  return absl::StrCat(addr, ":", port);
}

MATCHER_P(HasLocalEndpoint, endpoint, "") {
  switch (arg.second.family) {
    case AF_INET:
      return AddrPortStr(*reinterpret_cast<const struct in_addr*>(&arg.second.local_addr),
                         arg.second.local_port) == endpoint;
    case AF_INET6:
      return AddrPortStr(arg.second.local_addr, arg.second.local_port) == endpoint;
    default:
      return false;
  }
}

TEST(NetlinkSocketProberTest, EstablishedConnection) {
  testing::TCPSocket client;
  testing::TCPSocket server;

  // A bind and connect is sufficient to establish a connection.
  server.Bind();
  client.Connect(server);

  std::string client_endpoint = AddrPortStr(client.addr(), client.port());
  std::string server_endpoint = AddrPortStr(server.addr(), server.port());

  NetlinkSocketProber socket_prober;
  auto s = socket_prober.InetConnections();
  ASSERT_OK(s);
  std::unique_ptr<std::map<int, SocketInfo>> socket_info_entries = s.ConsumeValueOrDie();

  EXPECT_THAT(*socket_info_entries, Contains(HasLocalEndpoint(client_endpoint)));

  client.Close();
  server.Close();
}

TEST(NetlinkSocketProberTest, ClosedConnection) {
  testing::TCPSocket client;
  testing::TCPSocket server;

  // A bind and connect is sufficient to establish a connection.
  server.Bind();
  client.Connect(server);

  std::string client_endpoint = AddrPortStr(client.addr(), client.port());

  client.Close();
  server.Close();

  NetlinkSocketProber socket_prober;
  auto s = socket_prober.InetConnections();
  ASSERT_OK(s);
  std::unique_ptr<std::map<int, SocketInfo>> socket_info_entries = s.ConsumeValueOrDie();

  EXPECT_THAT(*socket_info_entries, Not(Contains(HasLocalEndpoint(client_endpoint))));
}

}  // namespace system
}  // namespace pl
