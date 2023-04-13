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

#include <gtest/gtest.h>
#include <string>

#include <absl/strings/numbers.h>

#include "src/common/base/base.h"
#include "src/common/system/config.h"
#include "src/common/system/proc_pid_path.h"
#include "src/common/system/socket_info.h"
#include "src/common/testing/test_utils/test_container.h"
#include "src/common/testing/testing.h"

namespace px {
namespace system {

class NetNamespaceTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_OK(container_.Run()); }

  EmailServiceContainer container_;
};

TEST_F(NetNamespaceTest, NetNamespace) {
  ASSERT_OK_AND_ASSIGN(uint32_t net_ns, NetNamespace(proc_path(), container_.process_pid()));
  EXPECT_NE(net_ns, 0);
  EXPECT_NE(net_ns, -1);
}

TEST_F(NetNamespaceTest, NetlinkSocketProber) {
  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<NetlinkSocketProber> socket_prober,
                         NetlinkSocketProber::Create());

    std::map<int, SocketInfo> socket_info_entries;
    ASSERT_OK(socket_prober->InetConnections(&socket_info_entries,
                                             kTCPEstablishedState | kTCPListeningState));
    int num_conns = socket_info_entries.size();

    // Assume that on any reasonable host, there will be more than 1 connection in the default
    // network namespace.
    EXPECT_GT(num_conns, 1);
  }

  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<NetlinkSocketProber> socket_prober,
                         NetlinkSocketProber::Create(container_.process_pid()));

    std::map<int, SocketInfo> socket_info_entries;
    ASSERT_OK(socket_prober->InetConnections(&socket_info_entries,
                                             kTCPEstablishedState | kTCPListeningState));
    int num_conns = socket_info_entries.size();

    EXPECT_EQ(num_conns, 1);
  }

  {
    ASSERT_OK_AND_ASSIGN(std::unique_ptr<NetlinkSocketProber> socket_prober,
                         NetlinkSocketProber::Create());

    std::map<int, SocketInfo> socket_info_entries;
    ASSERT_OK(socket_prober->InetConnections(&socket_info_entries,
                                             kTCPEstablishedState | kTCPListeningState));
    int num_conns = socket_info_entries.size();

    // Assume that on any reasonable host, there will be more than 1 connection in the default
    // network namespace.
    EXPECT_GT(num_conns, 1);
  }
}

TEST_F(NetNamespaceTest, SocketProberManager) {
  std::map<uint32_t, std::vector<int>> pids_by_net_ns = PIDsByNetNamespace(proc_path());

  // At least two net namespaces: default, and the container we made during SetUp().
  EXPECT_GE(pids_by_net_ns.size(), 2);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SocketProberManager> socket_probers,
                       SocketProberManager::Create());

  // First round: map should be empty.
  for (auto& [ns, pids] : pids_by_net_ns) {
    PX_UNUSED(pids);
    EXPECT_EQ(socket_probers->GetSocketProber(ns), nullptr);
  }

  // Second round: map should become populated.
  for (auto& [ns, pids] : pids_by_net_ns) {
    // This might be flaky, if a network namespace was destroyed from the initial query to now.
    // Could cause false test failures.
    EXPECT_OK_AND_NE(socket_probers->GetOrCreateSocketProber(ns, pids), nullptr);
  }

  // Third round: map should be populated.
  for (auto& [ns, pids] : pids_by_net_ns) {
    PX_UNUSED(pids);
    EXPECT_NE(socket_probers->GetSocketProber(ns), nullptr);
  }

  // Fourth round: A call to Update() should not remove any sockets yet.
  socket_probers->Update();
  for (auto& [ns, pids] : pids_by_net_ns) {
    PX_UNUSED(pids);
    EXPECT_NE(socket_probers->GetSocketProber(ns), nullptr);
  }

  // Fifth round: A call to Update(), followed by no accesses.
  socket_probers->Update();
  // Don't access any socket probers.

  // Sixth round: If socket probers are not accessed, then they should have all been removed.
  socket_probers->Update();
  for (auto& [ns, pids] : pids_by_net_ns) {
    PX_UNUSED(pids);
    EXPECT_EQ(socket_probers->GetSocketProber(ns), nullptr);
  }
}

TEST_F(NetNamespaceTest, SocketInfoManager) {
  const int kPID = container_.process_pid();

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<SocketInfoManager> socket_info_db,
      SocketInfoManager::Create(proc_path(), kTCPEstablishedState | kTCPListeningState));
  ASSERT_NE(socket_info_db.get(), nullptr);

  {
    // Non-existent inode should return nullptr.
    // 3 is very unlikely to be used as an inode number.
    const uint32_t kUnusedInode = 3;
    ASSERT_NOT_OK(socket_info_db->Lookup(kPID, kUnusedInode));
    EXPECT_EQ(socket_info_db->num_socket_prober_calls(), 1);
  }

  {
    // Hacky: For the container in question, FD 6 is a valid socket FD.
    // If container is changed, or if the container is found to have races, this needs to be
    // updated. TOOD(oazizi): Make this more programmatic.
    constexpr pid_t kFD = 6;
    const auto fd_path = ProcPidPath(kPID, "fd", std::to_string(kFD));
    ASSERT_OK_AND_ASSIGN(std::filesystem::path fd_link, fs::ReadSymlink(fd_path));
    ASSERT_OK_AND_ASSIGN(uint32_t inode_num,
                         fs::ExtractInodeNum(fs::kSocketInodePrefix, fd_link.string()));
    ASSERT_OK_AND_ASSIGN(system::SocketInfo * socket_info, socket_info_db->Lookup(kPID, inode_num));
    ASSERT_NE(socket_info, nullptr);
    EXPECT_THAT(socket_info->family, ::testing::AnyOf(AF_INET, AF_INET6));

    // Expecting caching to be in effect.
    EXPECT_EQ(socket_info_db->num_socket_prober_calls(), 1);

    socket_info_db->Flush();

    // Flush resets counter
    EXPECT_EQ(socket_info_db->num_socket_prober_calls(), 0);
    ASSERT_OK_AND_ASSIGN(socket_info, socket_info_db->Lookup(kPID, inode_num));
    ASSERT_NE(socket_info, nullptr);
    EXPECT_THAT(socket_info->family, ::testing::AnyOf(AF_INET, AF_INET6));

    // After flush, we should have made one more call.
    EXPECT_EQ(socket_info_db->num_socket_prober_calls(), 1);
  }
}

}  // namespace system
}  // namespace px
