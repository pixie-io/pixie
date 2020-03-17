#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include "src/common/base/base.h"

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

  int one = 1;
  int client_fd;
  struct sockaddr_in server_addr, client_addr;
  socklen_t sin_len = sizeof(client_addr);

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  CHECK(sock > 0) << "can't open socket";

  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

  int port = 8080;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  CHECK(bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != -1) << "Can't bind";

  listen(sock, 5);

  LOG(INFO) << absl::Substitute("Listening for connections on port: $0", port);

  while (1) {
    client_fd = accept(sock, (struct sockaddr*)&client_addr, &sin_len);

    if (client_fd == -1) {
      LOG(ERROR) << "Can't accept";
      continue;
    }

    CHECK_GT(write(client_fd, response, sizeof(response) - 1), 0);

    // NOTE: This missing close is intentional. It's what makes potentially causes a leak in BPF
    // maps, because the BPF map is usually deallocated at close(). While this is a bad
    // implementation, in reality a process can be killed/terminated with open files. The Linux
    // kernel will clean-up these resources during exit()/kill(), but we don't have the
    // corresponding BPF traces to clean up our maps.
    // close(client_fd);
  }
}
