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

#include "src/common/base/base.h"
#include "src/common/base/inet_utils.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/proc_pid_path.h"
#include "src/common/system/socket_info.h"

DEFINE_int32(pid, -1,
             "The network namespace to report, specified by any PID belonging to the namespace. If "
             "-1, current PID is used.");
DEFINE_int32(fd, -1, "The FD to report. If -1, all sockets are reported.");

constexpr char kProgramDescription[] =
    "A tool that probes for the linux kernel for socket connections.\n"
    "\n"
    " - This program will probe the Linux kernel for all TCP and Unix domain sockets in a network "
    "namespace, and show information about the connections. The target network namespace can be"
    "specified via a PID that belongs to a different network namespace.\n"
    " - By specifying a file descriptor that refers to a socket, only the information for "
    "that socket will be shown.\n"
    "\n"
    "Note that this program must be run as root.";

using ::px::Status;
using ::px::system::kTCPEstablishedState;
using ::px::system::kTCPListeningState;
using ::px::system::SocketInfo;
using ::px::system::SocketInfoManager;

std::string IPv4AddrToString(struct in_addr addr, in_port_t port) {
  return absl::StrCat(px::IPv4AddrToString(addr).ValueOr("<error>"), ":", port);
}

std::string IPv6AddrToString(struct in6_addr addr, in_port_t port) {
  return absl::StrCat(px::IPv6AddrToString(addr).ValueOr("<error>"), ":", port);
}

std::string UnixAddrToString(struct un_path_t path, uint32_t inode) {
  return absl::StrCat(path.path, ":", inode);
}

std::string ToString(const SocketInfo& socket_info) {
  std::string family;
  std::string local_addr;
  std::string remote_addr;

  switch (socket_info.family) {
    case AF_INET:
      family = "TCP";
      local_addr = IPv4AddrToString(std::get<struct in_addr>(socket_info.local_addr),
                                    socket_info.local_port);
      remote_addr = IPv4AddrToString(std::get<struct in_addr>(socket_info.remote_addr),
                                     socket_info.remote_port);
      break;
    case AF_INET6:
      family = "TCP6";
      local_addr = IPv6AddrToString(std::get<struct in6_addr>(socket_info.local_addr),
                                    socket_info.local_port);
      remote_addr = IPv6AddrToString(std::get<struct in6_addr>(socket_info.remote_addr),
                                     socket_info.remote_port);
      break;
    case AF_UNIX:
      family = "Unix-socket";
      local_addr = UnixAddrToString(std::get<struct un_path_t>(socket_info.local_addr),
                                    socket_info.local_port);
      remote_addr = UnixAddrToString(std::get<struct un_path_t>(socket_info.remote_addr),
                                     socket_info.remote_port);
      break;
    default:
      family = std::to_string(socket_info.family);
      local_addr = "<unknown>";
      remote_addr = "<unknown>";
  }
  return absl::StrCat("family=", family, " local_addr=", local_addr, " remote_addr=", remote_addr);
}

int main(int argc, char** argv) {
  gflags::SetUsageMessage(kProgramDescription);
  px::EnvironmentGuard env_guard(&argc, argv);

  int32_t pid = FLAGS_pid;
  int32_t fd = FLAGS_fd;

  if (pid == -1) {
    pid = getpid();
    std::cout << "No PID specified. Assuming network namespace of current PID." << std::endl;
  }

  PX_ASSIGN_OR_EXIT(std::unique_ptr<SocketInfoManager> socket_info_db,
                    SocketInfoManager::Create(px::system::proc_path(), 0xfff));

  if (fd == -1) {
    std::cout << absl::Substitute("Querying network namespace of pid=$0 (all connections):", pid)
              << std::endl;
    std::map<int, SocketInfo>* namespace_conns;
    PX_ASSIGN_OR_EXIT(namespace_conns, socket_info_db->GetNamespaceConns(pid));

    int i = 0;
    for (const auto& x : *namespace_conns) {
      std::cout << absl::Substitute(" $0: inode=$1 $2", i, x.first, ToString(x.second))
                << std::endl;
      ++i;
    }

    if (i == 0) {
      std::cout << "No data" << std::endl;
    }
  } else {
    std::cout << absl::Substitute("Querying pid=$0 fd=$1:", pid, fd) << std::endl;
    const auto fd_path = px::system::ProcPidPath(pid, "fd", std::to_string(fd));
    PX_ASSIGN_OR_EXIT(std::filesystem::path fd_link, px::fs::ReadSymlink(fd_path));
    PX_ASSIGN_OR_EXIT(uint32_t inode_num,
                      px::fs::ExtractInodeNum(px::fs::kSocketInodePrefix, fd_link.string()));

    PX_ASSIGN_OR_EXIT(SocketInfo * socket_info, socket_info_db->Lookup(pid, inode_num));
    if (socket_info == nullptr) {
      std::cout << "No data" << std::endl;
      return 1;
    }

    std::cout << ToString(*socket_info) << std::endl;
  }

  return 0;
}
