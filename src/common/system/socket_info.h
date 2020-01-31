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

namespace pl {
namespace system {

// See tcp_states.h for other states if we ever need them.
constexpr int kTCPEstablishedState = 1 << 1;
constexpr int kTCPListeningState = 1 << 10;

struct SocketInfo {
  sa_family_t family;
  std::variant<struct in6_addr, struct in_addr, struct un_path_t> local_addr;
  std::variant<struct in6_addr, struct in_addr, struct un_path_t> remote_addr;
  // Use uint32_t instead of in_port_t, since it represents an inode for unix domain sockets.
  uint32_t local_port;
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

}  // namespace system
}  // namespace pl
