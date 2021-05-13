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

#include <pthread.h>

#include <csignal>
#include <memory>

#include "src/common/system/unix_socket.h"

#include "src/common/base/base.h"

using ::px::system::UnixSocket;

// Note that Content-Length is 1 byte extra,
// so that the connection does not close after writing the response.
char response[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=UTF-8\r\n"
    "Content-Length: 18\r\n"
    "\r\n"
    "Goodbye, world!\r\n";

UnixSocket g_socket;

void SignalHandler(int /* signum */) {
  // Clean-up the filesystem...we don't want to leave trash.
  if (!g_socket.path().empty()) {
    unlink(g_socket.path().data());
  }

  // To make sure we leak map entries, abort instead of exit.
  // Otherwise, destructors will call close().
  abort();
}

void* RunServer(void*) {
  std::string unix_socket_path = "/tmp/leaky_unix_sock.server";
  g_socket.BindAndListen(unix_socket_path);

  LOG(INFO) << absl::Substitute("Listening for connections on: $0", unix_socket_path);

  // Declaration of conn is outside the loop, otherwise conn will fall out of scope and call
  // Close(), thereby preventing the very leak we are trying to create.
  std::unique_ptr<UnixSocket> conn;

  while (1) {
    conn = g_socket.Accept();
    conn->Send(response);

    // NOTE: This missing close is intentional. It's what makes potentially causes a leak in BPF
    // maps, because the BPF map is usually deallocated at close(). While this is a bad
    // implementation, in reality a process can be killed/terminated with open files. The Linux
    // kernel will clean-up these resources during exit()/kill(), but we don't have the
    // corresponding BPF traces to clean up our maps.
    // conn->Close(client_fd);
  }

  g_socket.Close();
}

int main(int argc, char** argv) {
  px::EnvironmentGuard env_guard(&argc, argv);

  signal(SIGINT, SignalHandler);

  // One can build this server with or without pthreads.
  // See BUILD.bazel, where this define is controlled.
  // We use the different versions to stress our tests.
#ifdef PTHREAD_IMPL
  // Create a pthread to handle the work.
  // Note that the main thread will exit while the spawned thread lives on.
  // This is to stress usage models for our tests.
  pthread_t tid;
  int err = pthread_create(&tid, nullptr, &RunServer, nullptr);
  CHECK_EQ(err, 0);

  pthread_exit(0);
#else
  RunServer(nullptr);
#endif
}
