#include <arpa/inet.h>
#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/system/testing/tcp_socket.h"

namespace pl {
namespace system {
namespace testing {

TEST(TCPSocketTest, DataIsWrittenAndReceivedCorrectly) {
  TCPSocket server;
  server.Bind();

  std::vector<std::string> received_data;
  TCPSocket client;
  std::thread client_thread([&server, &client, &received_data]() {
    client.Connect(server);
    std::string data;
    while (client.Read(&data)) {
      received_data.push_back(data);
    }
  });
  server.Accept();
  EXPECT_EQ(2, server.Write("a,"));
  EXPECT_EQ(3, server.Send("bc,"));
  EXPECT_EQ(4, server.Send("END,"));
  EXPECT_EQ(7, server.SendMsg({"send", "msg"}));

  server.Close();
  client_thread.join();
  // read() might get all data from multiple write() because of kernel buffering, so we can only
  // check the concatenated string.
  EXPECT_EQ("a,bc,END,sendmsg", absl::StrJoin(received_data, ""));
}

TEST(TCPSocketTest, SendMsgAndRecvMsg) {
  TCPSocket server;
  server.Bind();

  std::vector<std::string> received_data;
  TCPSocket client;
  std::thread client_thread([&server, &client, &received_data]() {
    client.Connect(server);
    while (client.RecvMsg(&received_data) > 0) {
    }
  });
  server.Accept();
  EXPECT_EQ(14, server.SendMsg({"sendmsg", "recvmsg"}));

  server.Close();
  client_thread.join();

  EXPECT_EQ("sendmsgrecvmsg", absl::StrJoin(received_data, ""));
}

TEST(TCPSocketTest, WriteVandReadV) {
  TCPSocket server;
  server.Bind();

  std::vector<std::string> received_data;
  TCPSocket client;
  std::thread client_thread([&server, &client, &received_data]() {
    client.Connect(server);
    std::string buf;
    while (client.ReadV(&buf) > 0) {
      received_data.emplace_back(std::move(buf));
    }
  });
  server.Accept();
  EXPECT_EQ(11, server.WriteV({"writev", "readv"}));

  server.Close();
  client_thread.join();

  EXPECT_EQ("writevreadv", absl::StrJoin(received_data, ""));
}

TEST(TCPSocketTest, ServerAddrAndPort) {
  TCPSocket server;
  TCPSocket client;

  // Check the server address and port.
  {
    // Only a bind is required for port to be assigned,
    // and for client to be able to successfully connect.
    server.Bind();

    std::string server_addr;
    server_addr.resize(INET_ADDRSTRLEN);

    auto server_in_addr = server.addr();
    inet_ntop(AF_INET, &server_in_addr, server_addr.data(), INET_ADDRSTRLEN);
    server_addr.erase(server_addr.find('\0'));

    uint16_t server_port = ntohs(server.port());

    EXPECT_GT(server_port, 0);
    EXPECT_EQ(server_addr, "0.0.0.0");
  }

  // Check th client address and port.
  {
    client.Connect(server);
    std::string client_addr;
    client_addr.resize(INET_ADDRSTRLEN);

    auto client_in_addr = client.addr();
    inet_ntop(AF_INET, &client_in_addr, client_addr.data(), INET_ADDRSTRLEN);
    client_addr.erase(client_addr.find('\0'));

    uint16_t client_port = ntohs(server.port());

    EXPECT_GT(client_port, 0);
    EXPECT_EQ(client_addr, "127.0.0.1");
  }

  client.Close();
  server.Close();
}

}  // namespace testing
}  // namespace system
}  // namespace pl
