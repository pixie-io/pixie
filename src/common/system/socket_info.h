#pragma once

#include <arpa/inet.h>
#include <linux/inet_diag.h>
#include <linux/unix_diag.h>
#include <netinet/in.h>

#include <map>
#include <memory>

#include "src/common/base/base.h"

namespace pl {
namespace system {

struct SocketInfo {
  uint8_t family;
  struct in6_addr local_addr;
  in_port_t local_port;
  struct in6_addr remote_addr;
  in_port_t remote_port;
};

/**
 * The NetlinkSocketProber class uses NetLink to probe the Linux kernel about active connections.
 */
class NetlinkSocketProber {
 public:
  NetlinkSocketProber();
  ~NetlinkSocketProber();

  /**
   * Returns IPv4 or IPv6 TCP connections in the established state.
   *
   * @return list of established TCP connections as a map of inode to SocketInfoEntry,
   * or error if information could not be obtained.
   */
  StatusOr<std::unique_ptr<std::map<int, SocketInfo>>> InetConnections();

 private:
  Status SendInetDiagReq();
  StatusOr<std::unique_ptr<std::map<int, SocketInfo>>> RecvInetDiagResp();

  int fd_;
};

}  // namespace system
}  // namespace pl
