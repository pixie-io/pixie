#pragma once

#include <arpa/inet.h>
#include <linux/inet_diag.h>
#include <linux/unix_diag.h>
#include <netinet/in.h>

#include <vector>

#include "src/common/base/base.h"

namespace pl {
namespace system {

struct SocketInfoEntry {
  uint32_t inode;
  uint8_t family;
  struct in_addr local_addr;
  in_port_t local_port;
  struct in_addr remote_addr;
  in_port_t remote_port;
};

/**
 * The SocketInfo class uses NetLink to probe the Linux kernel about active connections.
 */
class SocketInfo {
 public:
  SocketInfo();
  ~SocketInfo();

  /**
   * Returns a vector of IPv4 or IPv6 TCP connections in the established state.
   *
   * @return list of established TCP connections, or error if information could not be obtained.
   */
  StatusOr<std::vector<SocketInfoEntry>> InetConnections();

 private:
  Status SendInetDiagReq();
  StatusOr<std::vector<SocketInfoEntry>> RecvInetDiagResp();

  int fd_;
};

}  // namespace system
}  // namespace pl
