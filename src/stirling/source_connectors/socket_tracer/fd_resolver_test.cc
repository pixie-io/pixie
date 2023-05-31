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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>

#include "src/common/base/types.h"
#include "src/common/system/tcp_socket.h"
#include "src/stirling/source_connectors/socket_tracer/fd_resolver.h"

namespace px {
namespace stirling {

using testing::AnyOf;
using testing::StartsWith;

class FDResolverTest : public ::testing::Test {
 protected:
  void SetUp() { proc_parser_ = std::make_unique<system::ProcParser>(); }

  std::unique_ptr<system::ProcParser> proc_parser_;
};

TEST_F(FDResolverTest, ResolveStdin) {
  int pid = getpid();
  int fd = STDOUT_FILENO;

  // Step 1 - Setup FDResolver.
  auto resolver = FDResolver(proc_parser_.get(), pid, fd);

  std::optional<std::string_view> fd_link;
  bool success;

  fd_link = resolver.InferFDLink(std::chrono::steady_clock::now());
  EXPECT_FALSE(fd_link.has_value());

  success = resolver.Setup();
  ASSERT_TRUE(success);

  fd_link = resolver.InferFDLink(std::chrono::steady_clock::now());
  EXPECT_FALSE(fd_link.has_value());

  // Assume some activity on the connection before next update (at time t).
  auto t = std::chrono::steady_clock::now();

  // Step 2 - Update FDResolver to capture connection if stable.
  success = resolver.Update();
  EXPECT_TRUE(success);

  // FD link captured during established time window.
  fd_link = resolver.InferFDLink(t);
  EXPECT_TRUE(fd_link.has_value());
  // Value should be a path or a pipe
  EXPECT_THAT(*fd_link, AnyOf(StartsWith("/"), StartsWith("pipe")));

  // Resolver can't conclude anything for outside the time window.
  fd_link = resolver.InferFDLink(std::chrono::steady_clock::now());
  EXPECT_FALSE(fd_link.has_value());
}

TEST_F(FDResolverTest, CaptureLongLivedSocket) {
  system::TCPSocket socket;
  int pid = getpid();
  int fd = socket.sockfd();

  // Step 1 - Setup FDResolver.
  auto resolver = FDResolver(proc_parser_.get(), pid, fd);

  std::optional<std::string_view> fd_link;
  bool success;

  fd_link = resolver.InferFDLink(std::chrono::steady_clock::now());
  EXPECT_FALSE(fd_link.has_value());

  success = resolver.Setup();
  ASSERT_TRUE(success);

  fd_link = resolver.InferFDLink(std::chrono::steady_clock::now());
  EXPECT_FALSE(fd_link.has_value());

  // Assume some activity on the connection before next update (at time t).
  auto t = std::chrono::steady_clock::now();

  // Step 2 - Update FDResolver to capture connection if stable.
  success = resolver.Update();
  EXPECT_TRUE(success);

  // FD link captured during established time window.
  fd_link = resolver.InferFDLink(t);
  EXPECT_TRUE(fd_link.has_value());

  // Resolver can't conclude anything for outside the time window.
  fd_link = resolver.InferFDLink(std::chrono::steady_clock::now());
  EXPECT_FALSE(fd_link.has_value());

  socket.Close();
}

TEST_F(FDResolverTest, MissShortLivedSocketNoWindow) {
  system::TCPSocket socket;
  int pid = getpid();
  int fd = socket.sockfd();

  std::optional<std::string_view> fd_link;
  bool success;

  // Step 1 - Setup FDResolver.
  auto resolver = FDResolver(proc_parser_.get(), pid, fd);
  success = resolver.Setup();
  ASSERT_TRUE(success);

  // Assume some activity on the connection before next update (time t), and then a close.
  auto t = std::chrono::steady_clock::now();
  socket.Close();

  // Step 2 - Update FDResolver to capture connection if stable.
  // Resolver should return nullopt, because connection was already gone before Update() call.
  // Resolver can build any time window validity for the socket.
  success = resolver.Update();
  EXPECT_FALSE(success);
  fd_link = resolver.InferFDLink(t);
  EXPECT_FALSE(fd_link.has_value());
}

TEST_F(FDResolverTest, MissShortLivedSocketNoSetup) {
  system::TCPSocket socket;
  int pid = getpid();
  int fd = socket.sockfd();
  // Some activity at time t, between socket creation and close.
  auto t = std::chrono::steady_clock::now();
  PX_UNUSED(t);
  socket.Close();

  auto resolver = FDResolver(proc_parser_.get(), pid, fd);
  bool success = resolver.Setup();
  EXPECT_FALSE(success);
}

TEST_F(FDResolverTest, NewSocketSameFD) {
  system::TCPSocket socket;
  int pid = getpid();
  int fd = socket.sockfd();

  std::optional<std::string_view> fd_link;
  bool success;

  // Step 1 - Setup FDResolver.
  auto resolver = FDResolver(proc_parser_.get(), pid, fd);
  success = resolver.Setup();
  ASSERT_TRUE(success);

  // Assume some activity on the connection before next update (time t), and then a close.
  auto t1 = std::chrono::steady_clock::now();
  socket.Close();

  system::TCPSocket socket2;
  int fd2 = socket2.sockfd();
  ASSERT_EQ(fd, fd2);
  auto t2 = std::chrono::steady_clock::now();

  // Step 2 - Update FDResolver to capture connection if stable.
  // Resolver should return nullopt, because connection was already gone before Update() call.
  // Resolver can build any time window validity for the socket.
  success = resolver.Update();
  EXPECT_FALSE(success);

  fd_link = resolver.InferFDLink(t1);
  EXPECT_FALSE(fd_link.has_value());

  fd_link = resolver.InferFDLink(t2);
  EXPECT_FALSE(fd_link.has_value());
}

}  // namespace stirling
}  // namespace px
