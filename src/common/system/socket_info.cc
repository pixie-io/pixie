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

#include "src/common/system/socket_info.h"

#include <arpa/inet.h>
#include <linux/inet_diag.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/sock_diag.h>
#include <linux/unix_diag.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

#include <absl/container/flat_hash_set.h>

#include "src/common/base/base.h"
#include "src/common/base/inet_utils.h"
#include "src/common/system/scoped_namespace.h"

namespace px {
namespace system {

//-----------------------------------------------------------------------------
// NetlinkSocketProber
//-----------------------------------------------------------------------------

Status NetlinkSocketProber::Connect() {
  fd_ = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_SOCK_DIAG);
  if (fd_ < 0) {
    return error::Internal("Could not create NETLINK_SOCK_DIAG connection. [errno=$0]", errno);
  }
  return Status::OK();
}

StatusOr<std::unique_ptr<NetlinkSocketProber>> NetlinkSocketProber::Create() {
  auto socket_prober_ptr = std::unique_ptr<NetlinkSocketProber>(new NetlinkSocketProber);
  PX_RETURN_IF_ERROR(socket_prober_ptr->Connect());
  return socket_prober_ptr;
}

StatusOr<std::unique_ptr<NetlinkSocketProber>> NetlinkSocketProber::Create(int net_ns_pid) {
  // Enter the network namespace of the provided pid. The namespace will automatically exit
  // on termination of this scope.
  PX_ASSIGN_OR_RETURN(std::unique_ptr<ScopedNamespace> scoped_namespace,
                      ScopedNamespace::Create(net_ns_pid, "net"));
  PX_ASSIGN_OR_RETURN(std::unique_ptr<NetlinkSocketProber> socket_prober_ptr, Create());
  return socket_prober_ptr;
}

NetlinkSocketProber::~NetlinkSocketProber() {
  if (fd_ >= 0) {
    close(fd_);
  }
}

template <typename TDiagReqType>
Status NetlinkSocketProber::SendDiagReq(const TDiagReqType& msg_req) {
  ssize_t msg_len = sizeof(struct nlmsghdr) + sizeof(TDiagReqType);

  struct nlmsghdr msg_header = {};
  msg_header.nlmsg_len = msg_len;
  msg_header.nlmsg_type = SOCK_DIAG_BY_FAMILY;
  msg_header.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;

  struct iovec iov[2];
  iov[0].iov_base = &msg_header;
  iov[0].iov_len = sizeof(msg_header);
  iov[1].iov_base = const_cast<TDiagReqType*>(&msg_req);
  iov[1].iov_len = sizeof(msg_req);

  struct sockaddr_nl nl_addr = {};
  nl_addr.nl_family = AF_NETLINK;

  struct msghdr msg = {};
  msg.msg_name = &nl_addr;
  msg.msg_namelen = sizeof(nl_addr);
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;

  ssize_t bytes_sent = 0;

  while (bytes_sent < msg_len) {
    ssize_t retval = sendmsg(fd_, &msg, 0);
    if (retval < 0) {
      return error::Internal("Failed to send NetLink messages [errno=$0]", errno);
    }
    bytes_sent += retval;
  }

  return Status::OK();
}

namespace {

Status ProcessDiagMsg(const struct inet_diag_msg& diag_msg, unsigned int len,
                      std::map<int, SocketInfo>* socket_info_entries) {
  if (len < NLMSG_LENGTH(sizeof(diag_msg))) {
    return error::Internal("Not enough bytes");
  }

  if (diag_msg.idiag_family != AF_INET && diag_msg.idiag_family != AF_INET6) {
    return error::Internal("Unsupported address family $0", diag_msg.idiag_family);
  }

  if (diag_msg.idiag_inode == 0) {
    // An inode of 0 is intermittently produced.
    // One case of this is cause by a server that has not yet called accept().
    // Between the client trying to establish a connection and the server calling accept(),
    // the connection appears as ESTABLISHED, but with an inode of 0.

    // Ingore it, since an inode of 0 is not useful.
    return Status::OK();
  }

  auto iter = socket_info_entries->find(diag_msg.idiag_inode);
  ECHECK(iter == socket_info_entries->end())
      << absl::Substitute("Clobbering socket info at inode=$0", diag_msg.idiag_inode);

  SocketInfo socket_info = {};
  socket_info.family = diag_msg.idiag_family;
  socket_info.local_port = diag_msg.id.idiag_sport;
  socket_info.remote_port = diag_msg.id.idiag_dport;
  socket_info.state =
      magic_enum::enum_cast<TCPConnState>(diag_msg.idiag_state).value_or(TCPConnState::kUnknown);
  if (socket_info.family == AF_INET) {
    socket_info.local_addr = *reinterpret_cast<const struct in_addr*>(&diag_msg.id.idiag_src);
    socket_info.remote_addr = *reinterpret_cast<const struct in_addr*>(&diag_msg.id.idiag_dst);
  } else {
    socket_info.local_addr = *reinterpret_cast<const struct in6_addr*>(&diag_msg.id.idiag_src);
    socket_info.remote_addr = *reinterpret_cast<const struct in6_addr*>(&diag_msg.id.idiag_dst);
  }

  socket_info_entries->insert({diag_msg.idiag_inode, std::move(socket_info)});

  return Status::OK();
}

Status ProcessDiagMsg(const struct unix_diag_msg& diag_msg, unsigned int len,
                      std::map<int, SocketInfo>* socket_info_entries) {
  if (len < NLMSG_LENGTH(sizeof(diag_msg))) {
    return error::Internal("Not enough bytes");
  }

  if (diag_msg.udiag_family != AF_UNIX) {
    return error::Internal("Unsupported address family $0", diag_msg.udiag_family);
  }

  // Since we asked for UDIAG_SHOW_PEER in the unix_diag_req,
  // The response has additional attributes, which we parse here.
  // In particular, we are looking for the peer socket's inode number.
  const struct rtattr* attr;
  unsigned int rta_len = len - NLMSG_LENGTH(sizeof(diag_msg));
  unsigned int peer = 0;

  for (attr = reinterpret_cast<const struct rtattr*>(&diag_msg + 1); RTA_OK(attr, rta_len);
       attr = RTA_NEXT(attr, rta_len)) {
    switch (attr->rta_type) {
      case UNIX_DIAG_NAME:
        // Nothing for now, but could extract path name, if needed.
        // For this to work, one should also add UDIAG_SHOW_NAME to the request.
        break;
      case UNIX_DIAG_PEER:
        if (RTA_PAYLOAD(attr) >= sizeof(peer)) {
          peer = *reinterpret_cast<unsigned int*>(RTA_DATA(attr));
        }
    }
  }

  auto iter = socket_info_entries->find(diag_msg.udiag_ino);
  ECHECK(iter == socket_info_entries->end())
      << absl::Substitute("Clobbering socket info at inode=$0", diag_msg.udiag_ino);

  SocketInfo socket_info = {};
  socket_info.family = diag_msg.udiag_family;
  socket_info.local_port = diag_msg.udiag_ino;
  socket_info.local_addr = un_path_t{};
  socket_info.remote_port = peer;
  socket_info.remote_addr = un_path_t{};
  socket_info.state =
      magic_enum::enum_cast<TCPConnState>(diag_msg.udiag_state).value_or(TCPConnState::kUnknown);

  socket_info_entries->insert({diag_msg.udiag_ino, std::move(socket_info)});

  return Status::OK();
}

}  // namespace

template <typename TDiagMsgType>
Status NetlinkSocketProber::RecvDiagResp(std::map<int, SocketInfo>* socket_info_entries) {
  static constexpr int kBufSize = 8192;
  uint8_t buf[kBufSize];

  bool done = false;
  while (!done) {
    ssize_t num_bytes = recv(fd_, &buf, sizeof(buf), 0);
    if (num_bytes < 0) {
      return error::Internal("Receive call failed");
    }

    struct nlmsghdr* msg_header = reinterpret_cast<struct nlmsghdr*>(buf);

    for (; NLMSG_OK(msg_header, num_bytes); msg_header = NLMSG_NEXT(msg_header, num_bytes)) {
      if (msg_header->nlmsg_type == NLMSG_DONE) {
        done = true;
        break;
      }

      if (msg_header->nlmsg_type == NLMSG_ERROR) {
        return error::Internal("Netlink error");
      }

      if (msg_header->nlmsg_type != SOCK_DIAG_BY_FAMILY) {
        return error::Internal("Unexpected message type");
      }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
      TDiagMsgType* diag_msg = reinterpret_cast<TDiagMsgType*>(NLMSG_DATA(msg_header));
#pragma GCC diagnostic pop
      PX_RETURN_IF_ERROR(ProcessDiagMsg(*diag_msg, msg_header->nlmsg_len, socket_info_entries));
    }
  }

  return Status::OK();
}

namespace {
void ClassifySocketRoles(std::map<int, SocketInfo>* socket_info_entries) {
  absl::flat_hash_set<SockAddrIPv4, SockAddrIPv4HashFn, SockAddrIPv4EqFn> ipv4_listening_sockets;
  absl::flat_hash_set<SockAddrIPv6, SockAddrIPv6HashFn, SockAddrIPv6EqFn> ipv6_listening_sockets;

  // Create a hash map of all listening sockets, for quick look-ups.
  for (const auto& [_, s] : *socket_info_entries) {
    uint16_t local_port = static_cast<uint16_t>(s.local_port);
    if (s.state == TCPConnState::kListening) {
      switch (s.family) {
        case AF_INET:
          ipv4_listening_sockets.insert({std::get<struct in_addr>(s.local_addr), local_port});
          break;
        case AF_INET6:
          ipv6_listening_sockets.insert({std::get<struct in6_addr>(s.local_addr), local_port});
          break;
        default:
          LOG(DFATAL) << absl::Substitute("Unexpected address family $0", s.family);
      }
    }
  }

  // Look for a match with a listening socket with the same IP and port.
  // Also consider the zero IP (e.g. 0.0.0.0) as a match.
  struct in_addr ipv4_any = {};
  struct in6_addr ipv6_any = {};
  for (auto& [_, s] : *socket_info_entries) {
    uint16_t local_port = static_cast<uint16_t>(s.local_port);
    switch (s.family) {
      case AF_INET:
        if (ipv4_listening_sockets.contains({ipv4_any, local_port}) ||
            ipv4_listening_sockets.contains({std::get<struct in_addr>(s.local_addr), local_port})) {
          s.role = ClientServerRole::kServer;
        } else {
          s.role = ClientServerRole::kClient;
        }
        break;
      case AF_INET6:
        if (ipv6_listening_sockets.contains({ipv6_any, local_port}) ||
            ipv6_listening_sockets.contains(
                {std::get<struct in6_addr>(s.local_addr), local_port})) {
          s.role = ClientServerRole::kServer;
        } else {
          s.role = ClientServerRole::kClient;
        }
        break;
      default:
        LOG(DFATAL) << absl::Substitute("Unexpected address family $0", s.family);
    }
  }
}
}  // namespace

Status NetlinkSocketProber::InetConnections(std::map<int, SocketInfo>* socket_info_entries,
                                            int conn_states) {
  struct inet_diag_req_v2 msg_req = {};
  msg_req.sdiag_protocol = IPPROTO_TCP;
  msg_req.idiag_states = conn_states;

  // Run once for IPv4.
  msg_req.sdiag_family = AF_INET;
  PX_RETURN_IF_ERROR(SendDiagReq(msg_req));
  PX_RETURN_IF_ERROR(RecvDiagResp<struct inet_diag_msg>(socket_info_entries));

  // Run again for IPv6.
  msg_req.sdiag_family = AF_INET6;
  PX_RETURN_IF_ERROR(SendDiagReq(msg_req));
  PX_RETURN_IF_ERROR(RecvDiagResp<struct inet_diag_msg>(socket_info_entries));

  // If Listening connections were queried, then also populate the role field of the connections.
  if (conn_states & kTCPListeningState) {
    ClassifySocketRoles(socket_info_entries);
  }

  return Status::OK();
}

Status NetlinkSocketProber::UnixConnections(std::map<int, SocketInfo>* socket_info_entries,
                                            int conn_states) {
  struct unix_diag_req msg_req = {};
  msg_req.sdiag_family = AF_UNIX;
  msg_req.udiag_states = conn_states;
  msg_req.udiag_show = UDIAG_SHOW_PEER;

  PX_RETURN_IF_ERROR(SendDiagReq(msg_req));
  PX_RETURN_IF_ERROR(RecvDiagResp<struct unix_diag_msg>(socket_info_entries));
  return Status::OK();
}

//-----------------------------------------------------------------------------
// PIDsByNetNamespace
//-----------------------------------------------------------------------------

StatusOr<uint32_t> NetNamespace(std::filesystem::path proc, uint32_t pid) {
  return NetNamespace(proc / std::to_string(pid));
}

StatusOr<uint32_t> NetNamespace(std::filesystem::path proc_pid) {
  std::filesystem::path net_ns_path = proc_pid / "ns/net";
  PX_ASSIGN_OR_RETURN(std::filesystem::path net_ns_link, fs::ReadSymlink(net_ns_path));
  PX_ASSIGN_OR_RETURN(uint32_t net_ns_inode_num,
                      fs::ExtractInodeNum(fs::kNetInodePrefix, net_ns_link.string()));
  return net_ns_inode_num;
}

std::map<uint32_t, std::vector<int>> PIDsByNetNamespace(std::filesystem::path proc) {
  std::map<uint32_t, std::vector<int>> result;

  for (const auto& p : std::filesystem::directory_iterator(proc)) {
    VLOG(1) << absl::Substitute("Directory: $0", p.path().string());
    int pid = 0;
    if (!absl::SimpleAtoi(p.path().filename().string(), &pid)) {
      VLOG(1) << absl::Substitute("Ignoring $0: Failed to parse pid.", p.path().string());
      continue;
    }

    StatusOr<uint32_t> net_ns_inode_num_status = NetNamespace(p);
    if (!net_ns_inode_num_status.ok()) {
      LOG(ERROR) << absl::Substitute(
          "Could not determine network namespace for pid $0. Message=$1.", pid,
          net_ns_inode_num_status.msg());
      continue;
    }

    result[net_ns_inode_num_status.ValueOrDie()].push_back(pid);
  }

  return result;
}

//-----------------------------------------------------------------------------
// SocketProberManager
//-----------------------------------------------------------------------------

NetlinkSocketProber* SocketProberManager::GetSocketProber(uint32_t ns) {
  auto iter = socket_probers_.find(ns);
  if (iter != socket_probers_.end()) {
    // Update the phase (similar to an LRU touch).
    iter->second.phase = current_phase_;

    VLOG(2) << absl::Substitute("SocketProberManager: Retrieving entry [ns=$0]", ns);
    return iter->second.socket_prober.get();
  }
  return nullptr;
}

StatusOr<NetlinkSocketProber*> SocketProberManager::CreateSocketProber(
    uint32_t ns, const std::vector<int>& pids) {
  StatusOr<std::unique_ptr<NetlinkSocketProber>> socket_prober_or = error::NotFound("");

  // Use any provided PID to create a socket into the network namespace.
  for (auto& pid : pids) {
    socket_prober_or = NetlinkSocketProber::Create(pid);
    if (socket_prober_or.ok()) {
      break;
    }
  }

  if (!socket_prober_or.ok()) {
    return error::Internal(
        "None of the provided PIDs for the provided namespace ($0) could be used to establish a "
        "netlink connection to the namespace. It is possible the namespace no longer exists. Error "
        "message for last attempt: $1",
        ns, socket_prober_or.msg());
  }

  VLOG(2) << absl::Substitute("SocketProberManager: Creating entry [ns=$0]", ns);

  // This socket prober will be in the network namespace defined by ns.
  std::unique_ptr<NetlinkSocketProber> socket_prober = socket_prober_or.ConsumeValueOrDie();
  NetlinkSocketProber* socket_prober_ptr = socket_prober.get();
  DCHECK_NE(socket_prober_ptr, nullptr);
  socket_probers_[ns] =
      TaggedSocketProber{.phase = current_phase_, .socket_prober = std::move(socket_prober)};
  return socket_prober_ptr;
}

StatusOr<NetlinkSocketProber*> SocketProberManager::GetOrCreateSocketProber(
    uint32_t ns, const std::vector<int>& pids) {
  // First check to see if an existing socket prober on the namespace exists.
  // If so, use that one.
  NetlinkSocketProber* socket_prober_ptr = GetSocketProber(ns);
  if (socket_prober_ptr != nullptr) {
    return socket_prober_ptr;
  }

  // Otherwise create a socket prober.
  return CreateSocketProber(ns, pids);
}

void SocketProberManager::Update() {
  // Toggle the phase.
  current_phase_ = current_phase_ ^ 1;

  // Remove socket probers that were not accessed in the last phase.
  auto iter = socket_probers_.begin();
  while (iter != socket_probers_.end()) {
    bool remove = (iter->second.phase == current_phase_);

    VLOG_IF(2, remove) << absl::Substitute("SocketProberManager: Removing entry [ns=$0]",
                                           iter->first);

    // Update iterator, deleting if necessary as we go.
    iter = remove ? socket_probers_.erase(iter) : ++iter;
  }
}

//-----------------------------------------------------------------------------
// SocketInfoManager
//-----------------------------------------------------------------------------

StatusOr<std::unique_ptr<SocketInfoManager>> SocketInfoManager::Create(
    std::filesystem::path proc_path, int conn_states) {
  std::unique_ptr<SocketInfoManager> socket_info_db_ptr(
      new SocketInfoManager(proc_path, conn_states));
  PX_ASSIGN_OR_RETURN(socket_info_db_ptr->socket_probers_, SocketProberManager::Create());
  return socket_info_db_ptr;
}

StatusOr<std::map<int, SocketInfo>*> SocketInfoManager::GetNamespaceConns(uint32_t pid) {
  PX_ASSIGN_OR_RETURN(uint32_t net_ns, NetNamespace(cfg_proc_path_, pid));

  // Step 1: Get the map of connections for this network namespace.
  // Create the map if it doesn't already exist.
  std::map<int, SocketInfo>* namespace_conns;

  auto ns_iter = connections_.find(net_ns);
  if (ns_iter != connections_.end()) {
    // Found a map of connections for this network namespace, so use it.
    namespace_conns = &ns_iter->second;
  } else {
    // No map of connections for this network namespace, so use a socker prober to populate one.
    PX_ASSIGN_OR_RETURN(NetlinkSocketProber * socket_prober,
                        socket_probers_->GetOrCreateSocketProber(net_ns, {static_cast<int>(pid)}));
    DCHECK(socket_prober != nullptr);

    ns_iter = connections_.insert(ns_iter, {static_cast<int>(net_ns), {}});
    namespace_conns = &ns_iter->second;

    Status s;

    s = socket_prober->InetConnections(namespace_conns, cfg_conn_states_);
    LOG_IF(ERROR, !s.ok()) << absl::Substitute("Failed to probe InetConnections [net_ns=$0 msg=$1]",
                                               net_ns, s.msg());

    s = socket_prober->UnixConnections(namespace_conns, cfg_conn_states_);
    LOG_IF(ERROR, !s.ok()) << absl::Substitute("Failed to probe UnixConnections [net_ns=$0 msg=$1]",
                                               net_ns, s.msg());

    ++num_socket_prober_calls_;
  }

  return namespace_conns;
}

StatusOr<SocketInfo*> SocketInfoManager::Lookup(uint32_t pid, uint32_t inode_num) {
  // Step 1: Get the map of connections for this network namespace.
  // Create the map if it doesn't already exist.
  std::map<int, SocketInfo>* namespace_conns;
  PX_ASSIGN_OR_RETURN(namespace_conns, GetNamespaceConns(pid));

  // Step 2: Lookup the inode.
  auto iter = namespace_conns->find(inode_num);
  if (iter == namespace_conns->end()) {
    return error::NotFound(
        "Likely not a TCP/Unix connection (might be some other socket type). Alternatively, might "
        "be looking in the wrong net namespace, which can happen if the target PID has connections "
        "in multiple namespaces.");
  }

  return &iter->second;
}

void SocketInfoManager::Flush() {
  socket_probers_->Update();
  connections_.clear();
  num_socket_prober_calls_ = 0;
}

}  // namespace system
}  // namespace px
