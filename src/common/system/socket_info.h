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

#pragma once

#include <netinet/in.h>

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/fs/inode_utils.h"

#include "src/common/base/base.h"

// For convenience, a unix domain socket path, from struct sockaddr_un.
// Leave outside the namespace, so it feels like an OS struct.
// struct sockaddr_un {
//   sa_family_t sun_family;               /* AF_UNIX */
//   char        sun_path[108];            /* Pathname */
// };
struct un_path_t {
  char path[108];
};

namespace px {
namespace system {

// See linux tcp_states.h for other states if we ever need them.
enum class TCPConnState {
  kUnknown = 0,
  kEstablished = 1,
  kTimeWait = 6,
  kCloseWait = 8,
  kListening = 10,
};

enum class UnixConnState {
  kUnknown = 0,
  kEstablished = 1,
  kListening = 3,
};

enum class ClientServerRole { kUnknown = 0, kClient = 1, kServer = 2 };

// These are bit-select versions of ConnState above.
// Note: These are not an enum, so they can be or'ed together.
constexpr int kTCPEstablishedState = 1 << static_cast<int>(TCPConnState::kEstablished);
constexpr int kTCPTimeWaitState = 1 << static_cast<int>(TCPConnState::kTimeWait);
constexpr int kTCPCloseWaitState = 1 << static_cast<int>(TCPConnState::kCloseWait);
constexpr int kTCPListeningState = 1 << static_cast<int>(TCPConnState::kListening);

constexpr int kUnixEstablishedState = 1 << static_cast<int>(TCPConnState::kEstablished);
constexpr int kUnixListeningState = 1 << static_cast<int>(TCPConnState::kEstablished);

struct SocketInfo {
  sa_family_t family;
  std::variant<struct in6_addr, struct in_addr, struct un_path_t> local_addr;
  std::variant<struct in6_addr, struct in_addr, struct un_path_t> remote_addr;
  // Use uint32_t instead of in_port_t, since it represents an inode for unix domain sockets.
  uint32_t local_port;
  uint32_t remote_port;
  TCPConnState state;
  ClientServerRole role = ClientServerRole::kUnknown;
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

/**
 * Returns the net namespace identifier (inode number) for the PID.
 *
 * @param proc Path to the proc filesystem.
 * @param pid PID for which network namespace is desired.
 * @return Network namespace as an inode number, or error if it could not be determined.
 */
StatusOr<uint32_t> NetNamespace(std::filesystem::path proc, uint32_t pid);

/**
 * Returns the net namespace identifier (inode number) for the PID.
 *
 * @param proc_pid Path to the process under proc filesystem (e.g. /proc/<pid>).
 * @return Network namespace as an inode number, or error if it could not be determined.
 */
StatusOr<uint32_t> NetNamespace(std::filesystem::path proc_pid);

/**
 * PIDsByNetNamespace scans /proc to find all unique network namespaces.
 *
 * @param proc Path to proc filesystem.
 * @return A map where key is the network namespace, and value is all the PIDs associated with that
 * network namespace.
 */
std::map<uint32_t, std::vector<int>> PIDsByNetNamespace(std::filesystem::path proc);

/**
 * SocketProberManager is a wrapper around the creation of NetlinkSocketProbers,
 * which caches socket_probers so that they can be reusued again without having to re-create
 * a socket prober.
 *
 * The cache is simple, and has a notion of rounds or iterations. If a socket prober is not accessed
 * in a given round, it will be removed in the next round. A round is defined by a call to Update().
 */
class SocketProberManager {
 public:
  static StatusOr<std::unique_ptr<SocketProberManager>> Create() {
    if (!IsRoot()) {
      return error::Internal("SocketProber requires root privileges");
    }
    return std::unique_ptr<SocketProberManager>(new SocketProberManager);
  }

  /**
   * Get a socket prober for the specified network namespace.
   *
   * @param ns Inode number of the network namespace.
   * @return Raw pointer to a netlink socket prober. Pointer will be nullptr if no socket prober
   * exists for the namespace. The manager holds the ownership of the socket prober, and the
   * returned pointer is only guaranteed to be valid until next call to Update().
   */
  NetlinkSocketProber* GetSocketProber(uint32_t ns);

  /**
   * Create a socket prober for the specified network namespace.
   *
   * @param ns Inode number of the network namespace.
   * @param pids Vector of PIDs associated with the namespace. At least one PID in this list must
   * still be active, otherwise a socket prober cannot be created.
   * @return A raw pointer a netlink socket prober, or error if a socket prober for the namespace
   * could not be created. Raw pointer will not be nullptr. The manager maintains ownership of the
   * created socket prober, and the returned pointer is only guaranteed to be valid until next call
   * to Update().
   */
  StatusOr<NetlinkSocketProber*> CreateSocketProber(uint32_t ns, const std::vector<int>& pids);

  /**
   * Get a socket prober for the specified network namespace, or create one if it does not exist in
   * the cache.
   *
   * @param ns Inode number of the network namespace.
   * @param pids Vector of PIDs associated with the namespace. At least one PID in this list must
   * still be active, otherwise a socket prober cannot be created.
   * @return A raw pointer a netlink socket prober, or error if a socket prober for the namespace
   * did not exist, and one could not be created. Raw pointer will not be nullptr. The manager
   * maintains ownership of the socket prober, and the returned pointer is only guaranteed to be
   * valid until next call to Update().
   */
  StatusOr<NetlinkSocketProber*> GetOrCreateSocketProber(uint32_t ns, const std::vector<int>& pids);

  /**
   * Removes any socket probers that were not accessed since the last call to this function.
   */
  void Update();

 private:
  SocketProberManager() = default;

  struct TaggedSocketProber {
    bool phase;
    std::unique_ptr<NetlinkSocketProber> socket_prober;
  };

  // Phase is used as a 1-bit timestamp.
  // Used to know whether a socket_prober has been recently used.
  bool current_phase_ = false;
  std::map<int, TaggedSocketProber> socket_probers_;
};

/**
 * SocketInfoManager is a caching manager of known sockets in the system.
 *
 * There is a primary Lookup interface to query for information on a socket, by inode number.
 *
 * SocketInfoManager manages its cache at the network namespace level. If a query is made to a new
 * network namespace, the information is gathered and then cached. Future queries will operate off
 * that snapshot of the known connections, for efficiency.
 *
 * Note that no attempt is made to check if new connections have been created after the snapshot is
 * established. This responsibility is on the user, who must explicitly call Flush(), so that new
 * connections can be discovered.
 */
class SocketInfoManager {
 public:
  /**
   * Create a new instance of SocketInfoManager.
   *
   * @param proc_path Path to the /proc filesystem
   * @param conn_states The connection states to probe for (established, listening, etc.).
   * @return unique_ptr to the SocketInfoManager, or error if there were not enough privileges to
   * initialize the SocketInfoManager.
   */
  static StatusOr<std::unique_ptr<SocketInfoManager>> Create(
      std::filesystem::path proc_path, int conn_states = kTCPEstablishedState);

  /**
   * Return all socket info for a given network namespace.
   *
   * @param pid The PID used to determine the network namespace.
   * @return A map with inode number as key, and socket information as value. Returns error if
   * information could not be queried.
   */
  StatusOr<std::map<int, system::SocketInfo>*> GetNamespaceConns(uint32_t pid);

  /**
   * Search for the socket info of a given inode number.
   *
   * @param pid The PID owning the connection. Used to determine the network namespace.
   * @param inode_num The inode number of the local socket.
   * @return Information for socket, including remote endpoint information. Returns error if
   * information could not be queried.
   */
  StatusOr<SocketInfo*> Lookup(uint32_t pid, uint32_t inode_num);

  /**
   * Flushes the cache so new connections can be discovered.
   */
  void Flush();

  /**
   * Number of socket prober queries made since the last Flush() (or init).
   * Cached responses are not included in this count.
   * Useful for performance optimization, since socket prober queries are expensive.
   */
  int num_socket_prober_calls() { return num_socket_prober_calls_; }

 private:
  SocketInfoManager(std::filesystem::path proc_path, int conn_states)
      : cfg_proc_path_(proc_path), cfg_conn_states_(conn_states) {}

  const std::filesystem::path cfg_proc_path_;

  // The connection states that are considered this SocketInfoManager.
  // For example, SocketInfoManager can hold only a list of Established connections.
  // See connection states at the top of this file.
  const int cfg_conn_states_;

  // Two-level to socket information:
  // First key is namespace inode; second key is socket inode.
  std::map<int, std::map<int, SocketInfo>> connections_;

  // Portal through which new connection information is gathered,
  // and populated into connections_.
  std::unique_ptr<SocketProberManager> socket_probers_;

  // Keep statistics of how many socket_prober calls were made.
  // Useful for performance optimization, since socket_prober calls are expensive.
  // This count does not include count of cached responses.
  int num_socket_prober_calls_ = 0;
};

};  // namespace system
}  // namespace px
