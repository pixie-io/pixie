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
  uint32_t local_port;
  struct in6_addr remote_addr;
  uint32_t remote_port;
};

/**
 * The NetlinkSocketProber class uses NetLink to probe the Linux kernel about active connections.
 */
class NetlinkSocketProber {
 public:
  NetlinkSocketProber();
  ~NetlinkSocketProber();

  /**
   * Finds IPv4 or IPv6 TCP connections in the established state.
   *
   * @param map of inode to SocketInfoEntry that will be populated with established connections.
   * @return error if connection information could not be obtained from kernel.
   */
  Status InetConnections(std::map<int, SocketInfo>* socket_info_entries);

  /**
   * Finds Unix domain socket connections in the established state.
   *
   * @param map of inode to SocketInfoEntry that will be populated with established connections.
   * @return error if connection information could not be obtained from kernel.
   */
  Status UnixConnections(std::map<int, SocketInfo>* socket_info_entries);

 private:
  template <typename TDiagReqType>
  Status SendDiagReq(const TDiagReqType& msg_req);

  template <typename TDiagMsgType>
  Status RecvDiagResp(std::map<int, SocketInfo>* socket_info_entries);

  int fd_;
};

}  // namespace system
}  // namespace pl
