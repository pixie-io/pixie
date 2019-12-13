#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdlib>
#include <string>

#include <absl/strings/numbers.h>
#include "src/common/base/base.h"
#include "src/common/system/proc_parser.h"
#include "src/common/system/socket_info.h"
#include "src/common/system/testing/tcp_socket.h"
#include "src/common/testing/testing.h"

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

MATCHER_P(HasLocalIPEndpoint, endpoint, "") {
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

MATCHER_P(HasLocalUnixEndpoint, endpoint, "") {
  switch (arg.second.family) {
    case AF_UNIX:
      return endpoint == absl::Substitute("socket:[$0]", arg.second.local_port);
    default:
      return false;
  }
}

TEST(NetlinkSocketProberTest, EstablishedInetConnection) {
  testing::TCPSocket client;
  testing::TCPSocket server;

  // A bind and connect is sufficient to establish a connection.
  server.Bind();
  client.Connect(server);

  std::string client_endpoint = AddrPortStr(client.addr(), client.port());
  std::string server_endpoint = AddrPortStr(server.addr(), server.port());

  NetlinkSocketProber socket_prober;
  std::map<int, SocketInfo> socket_info_entries;
  auto s = socket_prober.InetConnections(&socket_info_entries);
  ASSERT_OK(s);

  EXPECT_THAT(socket_info_entries, Contains(HasLocalIPEndpoint(client_endpoint)));

  client.Close();
  server.Close();
}

TEST(NetlinkSocketProberTest, EstablishedUnixConnection) {
  Status s;
  int retval;

  // Create client and server, and connect them together.
  struct sockaddr_un server_addr = {AF_UNIX, ""};
  int server_listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(-1, server_listen_fd);

  retval =
      bind(server_listen_fd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr));
  ASSERT_EQ(0, retval) << absl::Substitute("bind() failed with errno=$0", errno);

  retval = listen(server_listen_fd, 2);
  ASSERT_EQ(0, retval) << absl::Substitute("listen() failed with errno=$0", errno);

  int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  ASSERT_NE(-1, client_fd) << absl::Substitute("socket() failed with errno=$0", errno);

  retval =
      connect(client_fd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr));
  ASSERT_EQ(0, retval);

  struct sockaddr_un client_addr;
  socklen_t len = sizeof(client_addr);
  int server_accept_fd =
      accept(server_listen_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &len);
  ASSERT_NE(-1, server_accept_fd) << absl::Substitute("accept() failed with errno=$0", errno);

  // Extract inode numbers.
  auto proc_parser = std::make_unique<system::ProcParser>(system::Config::GetInstance());
  std::string server_socket_id;
  s = proc_parser->ReadProcPIDFDLink(getpid(), server_accept_fd, &server_socket_id);
  ASSERT_OK(s);

  std::string client_socket_id;
  s = proc_parser->ReadProcPIDFDLink(getpid(), client_fd, &client_socket_id);
  ASSERT_OK(s);

  // Now begin the test of NetlinkSocketProber.
  NetlinkSocketProber socket_prober;
  std::map<int, SocketInfo> socket_info_entries;
  s = socket_prober.UnixConnections(&socket_info_entries);
  ASSERT_OK(s);

  EXPECT_THAT(socket_info_entries, Contains(HasLocalUnixEndpoint(client_socket_id)));
  EXPECT_THAT(socket_info_entries, Contains(HasLocalUnixEndpoint(server_socket_id)));

  close(client_fd);
  close(server_accept_fd);
  close(server_listen_fd);
}

TEST(NetlinkSocketProberTest, ClosedInetConnection) {
  testing::TCPSocket client;
  testing::TCPSocket server;

  // A bind and connect is sufficient to establish a connection.
  server.Bind();
  client.Connect(server);

  std::string client_endpoint = AddrPortStr(client.addr(), client.port());

  client.Close();
  server.Close();

  NetlinkSocketProber socket_prober;
  std::map<int, SocketInfo> socket_info_entries;
  auto s = socket_prober.InetConnections(&socket_info_entries);
  ASSERT_OK(s);

  EXPECT_THAT(socket_info_entries, Not(Contains(HasLocalIPEndpoint(client_endpoint))));
}

}  // namespace system
}  // namespace pl
