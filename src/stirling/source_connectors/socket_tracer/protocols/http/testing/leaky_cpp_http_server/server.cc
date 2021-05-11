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

#include <memory>

#include "src/common/system/tcp_socket.h"

#include "src/common/base/base.h"

using px::system::TCPSocket;

// Note that Content-Length is 1 byte extra,
// so that the connection does not close after writing the response.
char response[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=UTF-8\r\n"
    "Content-Length: 18\r\n"
    "\r\n"
    "Goodbye, world!\r\n";

int main(int argc, char** argv) {
  px::EnvironmentGuard env_guard(&argc, argv);

  TCPSocket socket;

  int one = 1;
  setsockopt(socket.sockfd(), SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

  int port = 8080;
  socket.BindAndListen(port);

  LOG(INFO) << absl::Substitute("Listening for connections on port: $0", port);

  // Declaration of conn is outside the loop, otherwise conn will fall out of scope and call
  // Close(), thereby preventing the very leak we are trying to create.
  std::unique_ptr<TCPSocket> conn;

  while (1) {
    conn = socket.Accept();
    conn->Write(response);

    // NOTE: This missing close is intentional. It's what makes potentially causes a leak in BPF
    // maps, because the BPF map is usually deallocated at close(). While this is a bad
    // implementation, in reality a process can be killed/terminated with open files. The Linux
    // kernel will clean-up these resources during exit()/kill(), but we don't have the
    // corresponding BPF traces to clean up our maps.
    // conn->Close(client_fd);
  }

  socket.Close();
}
