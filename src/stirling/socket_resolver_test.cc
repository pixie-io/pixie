#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/socket.h>

#include <chrono>

#include "src/common/base/types.h"
#include "src/common/system/tcp_socket.h"
#include "src/stirling/socket_resolver.h"

namespace pl {
namespace stirling {

class SocketResolverTest : public ::testing::Test {
 protected:
  void SetUp() {
    proc_parser_ = std::make_unique<system::ProcParser>(system::Config::GetInstance());
  }

  std::unique_ptr<system::ProcParser> proc_parser_;
};

TEST_F(SocketResolverTest, CaptureLongLivedSocket) {
  system::TCPSocket socket;
  int pid = getpid();
  int fd = socket.sockfd();

  // Step 1 - Setup SocketResolver.
  auto resolver = SocketResolver(proc_parser_.get(), pid, fd);

  std::optional<int> inode_num;
  bool success;

  inode_num = resolver.InferSocket(std::chrono::steady_clock::now());
  EXPECT_FALSE(inode_num.has_value());

  success = resolver.Setup();
  ASSERT_TRUE(success);

  inode_num = resolver.InferSocket(std::chrono::steady_clock::now());
  EXPECT_FALSE(inode_num.has_value());

  // Assume some activity on the connection before next update (at time t).
  auto t = std::chrono::steady_clock::now();

  // Step 2 - Update SocketResolver to capture connection if stable.
  success = resolver.Update();
  EXPECT_TRUE(success);

  // Inode captured during established time window.
  inode_num = resolver.InferSocket(t);
  EXPECT_TRUE(inode_num.has_value());

  // Resolver can't conlude anything for outside the time window.
  inode_num = resolver.InferSocket(std::chrono::steady_clock::now());
  EXPECT_FALSE(inode_num.has_value());

  socket.Close();
}

TEST_F(SocketResolverTest, MissShortLivedSocketNoWindow) {
  system::TCPSocket socket;
  int pid = getpid();
  int fd = socket.sockfd();

  std::optional<int> inode_num;
  bool success;

  // Step 1 - Setup SocketResolver.
  auto resolver = SocketResolver(proc_parser_.get(), pid, fd);
  success = resolver.Setup();
  ASSERT_TRUE(success);

  // Assume some activity on the connection before next update (time t), and then a close.
  auto t = std::chrono::steady_clock::now();
  socket.Close();

  // Step 2 - Update SocketResolver to capture connection if stable.
  // Resolver should return nullopt, because connection was already gone before Update() call.
  // Resolver can build any time window validity for the socket.
  success = resolver.Update();
  EXPECT_FALSE(success);
  inode_num = resolver.InferSocket(t);
  EXPECT_FALSE(inode_num.has_value());
}

TEST_F(SocketResolverTest, MissShortLivedSocketNoSetup) {
  system::TCPSocket socket;
  int pid = getpid();
  int fd = socket.sockfd();
  // Some activity at time t, between socket creation and close.
  auto t = std::chrono::steady_clock::now();
  PL_UNUSED(t);
  socket.Close();

  auto resolver = SocketResolver(proc_parser_.get(), pid, fd);
  bool success = resolver.Setup();
  EXPECT_FALSE(success);
}

TEST_F(SocketResolverTest, NewSocketSameFD) {
  system::TCPSocket socket;
  int pid = getpid();
  int fd = socket.sockfd();

  std::optional<int> inode_num;
  bool success;

  // Step 1 - Setup SocketResolver.
  auto resolver = SocketResolver(proc_parser_.get(), pid, fd);
  success = resolver.Setup();
  ASSERT_TRUE(success);

  // Assume some activity on the connection before next update (time t), and then a close.
  auto t1 = std::chrono::steady_clock::now();
  socket.Close();

  system::TCPSocket socket2;
  int fd2 = socket2.sockfd();
  ASSERT_EQ(fd, fd2);
  auto t2 = std::chrono::steady_clock::now();

  // Step 2 - Update SocketResolver to capture connection if stable.
  // Resolver should return nullopt, because connection was already gone before Update() call.
  // Resolver can build any time window validity for the socket.
  success = resolver.Update();
  EXPECT_FALSE(success);

  inode_num = resolver.InferSocket(t1);
  EXPECT_FALSE(inode_num.has_value());

  inode_num = resolver.InferSocket(t2);
  EXPECT_FALSE(inode_num.has_value());
}

}  // namespace stirling
}  // namespace pl
