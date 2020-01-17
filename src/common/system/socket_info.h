#pragma once

#include <netinet/in.h>

#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/fs/inode_utils.h"

#include "src/common/base/base.h"

namespace pl {
namespace system {

// See tcp_states.h for other states if we ever need them.
constexpr int kTCPEstablishedState = 1 << 1;
constexpr int kTCPListeningState = 1 << 10;

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
  /**
   * Create a socket prober within the current network namespace.
   */
  static StatusOr<std::unique_ptr<NetlinkSocketProber>> Create();

  /**
   * Create a socket prober within the network namespace of the provided PID.
   * Note that this requires superuser privileges.
   * Also note that this provides access to all connections within the network namespaces,
   * not just those for provided PID; the PID is just a means of specifying the namespace.
   *
   * @param net_ns_pid PID from which the network namespace is used.
   */
  static StatusOr<std::unique_ptr<NetlinkSocketProber>> Create(int net_ns_pid);

  ~NetlinkSocketProber();

  /**
   * Finds IPv4 or IPv6 TCP connections.
   *
   * @param socket_info_entries map of inode to SocketInfoEntry that will be populated with
   * established connections.
   * @param conn_states bit vector of connection states to return.
   *
   * @return error if connection information could not be obtained from kernel.
   */
  Status InetConnections(std::map<int, SocketInfo>* socket_info_entries,
                         int conn_states = kTCPEstablishedState);

  /**
   * Finds Unix domain socket connections.
   *
   * @param socket_info_entries map of inode to SocketInfoEntry that will be populated with
   * established connections.
   * @param conn_states bit vector of connection states to return.
   *
   * @return error if connection information could not be obtained from kernel.
   */
  Status UnixConnections(std::map<int, SocketInfo>* socket_info_entries,
                         int conn_states = kTCPEstablishedState);

 private:
  NetlinkSocketProber() = default;

  Status Connect();

  template <typename TDiagReqType>
  Status SendDiagReq(const TDiagReqType& msg_req);

  template <typename TDiagMsgType>
  Status RecvDiagResp(std::map<int, SocketInfo>* socket_info_entries);

  int fd_ = -1;
};

std::map<uint32_t, std::vector<int>> PIDsByNetNamespace(std::filesystem::path proc);

}  // namespace system
}  // namespace pl
