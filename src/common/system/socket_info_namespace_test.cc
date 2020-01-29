#include <unistd.h>

#include <gtest/gtest.h>
#include <string>

#include <absl/strings/numbers.h>

#include "src/common/base/base.h"
#include "src/common/system/config.h"
#include "src/common/system/socket_info.h"
#include "src/common/testing/test_utils/test_container.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace system {

class NetNamespaceTest : public ::testing::Test {
 protected:
  void SetUp() override { container_.Run(); }
  void TearDown() override { container_.Stop(); }

  DummyTestContainer container_;
};

TEST_F(NetNamespaceTest, NetNamespace) {
  ASSERT_OK_AND_ASSIGN(uint32_t net_ns, NetNamespace(system::Config::GetInstance().proc_path(),
                                                     container_.process_pid()));
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
  std::map<uint32_t, std::vector<int>> pids_by_net_ns =
      PIDsByNetNamespace(system::Config::GetInstance().proc_path());

  // At least two net namespaces: default, and the container we made during SetUp().
  EXPECT_GE(pids_by_net_ns.size(), 2);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SocketProberManager> socket_probers,
                       SocketProberManager::Create());

  // First round: map should be empty.
  for (auto& [ns, pids] : pids_by_net_ns) {
    PL_UNUSED(pids);
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
    PL_UNUSED(pids);
    EXPECT_NE(socket_probers->GetSocketProber(ns), nullptr);
  }

  // Fourth round: A call to Update() should not remove any sockets yet.
  socket_probers->Update();
  for (auto& [ns, pids] : pids_by_net_ns) {
    PL_UNUSED(pids);
    EXPECT_NE(socket_probers->GetSocketProber(ns), nullptr);
  }

  // Fifth round: A call to Update(), followed by no accesses.
  socket_probers->Update();
  // Don't access any socket probers.

  // Sixth round: If socket probers are not accessed, then they should have all been removed.
  socket_probers->Update();
  for (auto& [ns, pids] : pids_by_net_ns) {
    PL_UNUSED(pids);
    EXPECT_EQ(socket_probers->GetSocketProber(ns), nullptr);
  }
}

}  // namespace system
}  // namespace pl
