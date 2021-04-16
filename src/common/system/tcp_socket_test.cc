#include <arpa/inet.h>
#include <fcntl.h>
#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/system/tcp_socket.h"

namespace px {
namespace system {

TEST(TCPSocketTest, DataIsWrittenAndReceivedCorrectly) {
  TCPSocket server;
  server.BindAndListen();

  std::vector<std::string> received_data;
  TCPSocket client;
  std::thread client_thread([&server, &client, &received_data]() {
    client.Connect(server);
    std::string data;
    while (client.Read(&data)) {
      received_data.push_back(data);
    }
  });
  std::unique_ptr<TCPSocket> conn = server.Accept();
  EXPECT_EQ(2, conn->Write("a,"));
  EXPECT_EQ(3, conn->Send("bc,"));
  EXPECT_EQ(4, conn->Send("END,"));
  EXPECT_EQ(7, conn->SendMsg({"send", "msg"}));
  conn->Close();

  server.Close();
  client_thread.join();
  // read() might get all data from multiple write() because of kernel buffering, so we can only
  // check the concatenated string.
  EXPECT_EQ("a,bc,END,sendmsg", absl::StrJoin(received_data, ""));
}

TEST(TCPSocketTest, SendMsgAndRecvMsg) {
  TCPSocket server;
  server.BindAndListen();

  std::vector<std::string> received_data;
  TCPSocket client;
  std::thread client_thread([&server, &client, &received_data]() {
    client.Connect(server);
    while (client.RecvMsg(&received_data) > 0) {
    }
  });
  std::unique_ptr<TCPSocket> conn = server.Accept();
  EXPECT_EQ(14, conn->SendMsg({"sendmsg", "recvmsg"}));
  conn->Close();

  server.Close();
  client_thread.join();

  EXPECT_EQ("sendmsgrecvmsg", absl::StrJoin(received_data, ""));
}

TEST(TCPSocketTest, WriteVandReadV) {
  TCPSocket server;
  server.BindAndListen();

  std::vector<std::string> received_data;
  TCPSocket client;
  std::thread client_thread([&server, &client, &received_data]() {
    client.Connect(server);
    std::string buf;
    while (client.ReadV(&buf) > 0) {
      received_data.emplace_back(std::move(buf));
    }
  });
  std::unique_ptr<TCPSocket> conn = server.Accept();
  EXPECT_EQ(11, conn->WriteV({"writev", "readv"}));
  conn->Close();

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
    server.BindAndListen();

    std::string server_addr;
    server_addr.resize(INET_ADDRSTRLEN);

    auto server_in_addr = server.addr();
    inet_ntop(AF_INET, &server_in_addr, server_addr.data(), INET_ADDRSTRLEN);
    server_addr.erase(server_addr.find('\0'));

    uint16_t server_port = ntohs(server.port());

    EXPECT_GT(server_port, 0);
    EXPECT_EQ(server_addr, "127.0.0.1");
  }

  // Check the client address and port.
  {
    client.Connect(server);
    std::string client_addr;
    client_addr.resize(INET_ADDRSTRLEN);

    auto client_in_addr = client.addr();
    inet_ntop(AF_INET, &client_in_addr, client_addr.data(), INET_ADDRSTRLEN);
    client_addr.erase(client_addr.find('\0'));

    uint16_t client_port = ntohs(client.port());

    EXPECT_GT(client_port, 0);
    EXPECT_EQ(client_addr, "127.0.0.1");
  }

  client.Close();
  server.Close();
}

TEST(TCPSocketTest, MultipleSequencialConnectsFailed) {
  {
    TCPSocket server;
    server.BindAndListen();

    TCPSocket client;
    client.Connect(server);
    EXPECT_DEATH(client.Connect(server), "Transport endpoint is already connected");
  }
  {
    TCPSocket server;
    server.BindAndListen();

    TCPSocket client;
    int flags = fcntl(client.sockfd(), F_GETFL);
    ASSERT_GT(flags, 0);
    int rv = fcntl(client.sockfd(), F_SETFL, flags | O_NONBLOCK);
    ASSERT_EQ(rv, 0);
    EXPECT_DEATH(client.Connect(server), "Operation now in progress");
    client.Connect(server);
    EXPECT_DEATH(client.Connect(server), "Transport endpoint is already connected");
  }
}

}  // namespace system
}  // namespace px
