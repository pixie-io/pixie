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

std::string AddrPortStr(struct in_addr in_addr, in_port_t in_port) {
  std::string addr;
  int port;

  Status s = ParseIPv4Addr(in_addr, &addr);
  CHECK(s.ok());
  port = ntohs(in_port);

  return absl::StrCat(addr, port);
}

MATCHER_P(HasLocalEndpoint, endpoint, "") {
  return AddrPortStr(arg.local_addr, arg.local_port) == endpoint;
}

TEST(SocketInfoTest, EstablishedConnection) {
  testing::TCPSocket client;
  testing::TCPSocket server;

  // A bind and connect is sufficient to establish a connection.
  server.Bind();
  client.Connect(server);

  std::string client_endpoint = AddrPortStr(client.addr(), client.port());
  std::string server_endpoint = AddrPortStr(server.addr(), server.port());

  SocketInfo socket_info;
  auto s = socket_info.InetConnections();
  ASSERT_OK(s);
  std::vector<SocketInfoEntry>& socket_info_entries = s.ValueOrDie();

  EXPECT_THAT(socket_info_entries, Contains(HasLocalEndpoint(client_endpoint)));

  client.Close();
  server.Close();
}

TEST(SocketInfoTest, ClosedConnection) {
  testing::TCPSocket client;
  testing::TCPSocket server;

  // A bind and connect is sufficient to establish a connection.
  server.Bind();
  client.Connect(server);

  std::string client_endpoint = AddrPortStr(client.addr(), client.port());

  client.Close();
  server.Close();

  SocketInfo socket_info;
  auto s = socket_info.InetConnections();
  ASSERT_OK(s);
  std::vector<SocketInfoEntry>& socket_info_entries = s.ValueOrDie();

  EXPECT_THAT(socket_info_entries, Not(Contains(HasLocalEndpoint(client_endpoint))));
}

}  // namespace system
}  // namespace pl
