#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include <memory>

#include "src/common/system/tcp_socket.h"

#include "src/common/base/base.h"

using pl::system::TCPSocket;

// Note that Content-Length is 1 byte extra,
// so that the connection does not close after writing the response.
char response[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=UTF-8\r\n"
    "Content-Length: 18\r\n"
    "\r\n"
    "Goodbye, world!\r\n";

int main(int argc, char** argv) {
  pl::EnvironmentGuard env_guard(&argc, argv);

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
