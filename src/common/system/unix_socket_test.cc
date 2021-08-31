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

#include <fcntl.h>
#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/system/unix_socket.h"

namespace px {
namespace system {

TEST(UnixSocketTest, SendAndRecv) {
  UnixSocket server;

  std::string unix_socket_path = absl::Substitute(
      "/tmp/unix_sock_$0.server", std::chrono::steady_clock::now().time_since_epoch().count());

  server.BindAndListen(unix_socket_path);

  std::string received_data;
  UnixSocket client;
  std::thread client_thread([&server, &client, &received_data]() {
    client.Connect(server);
    while (client.Recv(&received_data)) {
    }
  });
  std::unique_ptr<UnixSocket> conn = server.Accept();
  EXPECT_EQ(2, conn->Send("a,"));
  EXPECT_EQ(3, conn->Send("bc,"));
  EXPECT_EQ(3, conn->Send("END"));
  conn->Close();

  server.Close();
  client_thread.join();
  // read() might get all data from multiple write() because of kernel buffering, so we can only
  // check the concatenated string.
  EXPECT_EQ(received_data, "a,bc,END");
}

TEST(UnixSocketTest, Path) {
  UnixSocket server;
  UnixSocket client;

  std::string unix_socket_path = absl::Substitute(
      "/tmp/unix_sock_$0.server", std::chrono::steady_clock::now().time_since_epoch().count());

  server.BindAndListen(unix_socket_path);
  EXPECT_EQ(server.path(), unix_socket_path);

  client.Connect(server);
  EXPECT_EQ(client.path(), "") << "Unbound Unix domain sockets (like clients), are unnamed";

  client.Close();
  server.Close();
}

}  // namespace system
}  // namespace px
