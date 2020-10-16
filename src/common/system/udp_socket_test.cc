#include <arpa/inet.h>
#include <fcntl.h>
#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/system/udp_socket.h"

namespace pl {
namespace system {

TEST(UDPSocketTest, SendToAndRecvFrom) {
  UDPSocket server;
  server.BindAndListen();

  UDPSocket client;
  std::string recv_data;
  std::thread server_thread([&server, &recv_data]() { server.RecvFrom(&recv_data); });

  EXPECT_EQ(5, client.SendTo("pixie", server.sockaddr())) << absl::Substitute("errno=$0", errno);

  server_thread.join();

  EXPECT_EQ(recv_data, "pixie");

  client.Close();
  server.Close();
}

TEST(UDPSocketTest, AddrAndPort) {
  UDPSocket server;
  UDPSocket client;

  {
    // Only a bind is required for port to be assigned,
    // and for client to be able to successfully connect.
    server.BindAndListen();

    std::string server_addr;
    server_addr.resize(INET_ADDRSTRLEN);

    auto server_in_addr = server.addr();
    inet_ntop(AF_INET, &server_in_addr, server_addr.data(), INET_ADDRSTRLEN);
    server_addr.erase(server_addr.find('\0'));

    uint16_t server_port = ntohs(server.port());

    EXPECT_GT(server_port, 0);
    EXPECT_EQ(server_addr, "0.0.0.0");
  }

  {
    client.SendTo("foo", server.sockaddr());

    std::string data;
    struct sockaddr_in source = server.RecvFrom(&data);

    ASSERT_NE(source.sin_addr.s_addr, 0);
    ASSERT_NE(source.sin_port, 0);

    auto client_in_addr = source.sin_addr;
    std::string client_addr;
    client_addr.resize(INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &client_in_addr, client_addr.data(), INET_ADDRSTRLEN);
    client_addr.erase(client_addr.find('\0'));

    uint16_t client_port = ntohs(source.sin_port);

    EXPECT_GT(client_port, 0);
    EXPECT_EQ(client_addr, "127.0.0.1");
  }

  client.Close();
  server.Close();
}

}  // namespace system
}  // namespace pl
