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

#include "src/common/base/base.h"
#include "src/common/base/env.h"
#include "src/common/system/udp_socket.h"

using px::system::UDPSocket;

int main(int argc, char** argv) {
  px::EnvironmentGuard env_guard(&argc, argv);

  if (argc < 3) {
    LOG(FATAL) << absl::Substitute("Expected server address and port to be provided, instead received $0", *argv);
  }
  std::string_view msg = "Hello, World!";
  UDPSocket client;
  
  sockaddr_in server_addr;
  int status = inet_pton(AF_INET, argv[1], &server_addr.sin_addr);
  if (status != 1) {
    LOG(FATAL) << absl::Substitute("Failed to parse server address $0", argv[1]);
  }
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(atoi(argv[2]));
  client.SendTo(msg, server_addr, 0);
}
