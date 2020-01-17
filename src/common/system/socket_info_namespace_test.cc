#include <unistd.h>

#include <gtest/gtest.h>
#include <string>

#include <absl/strings/numbers.h>

#include "src/common/base/base.h"
#include "src/common/system/socket_info.h"
#include "src/common/testing/testing.h"

#include "src/common/exec/exec.h"
#include "src/common/exec/subprocess.h"

namespace pl {
namespace system {

class NetlinkSocketProberNamespaceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a process in with its own network namespace.
    // Creating a docker container server is an easy way to do this.

    // First pull the image.
    // Do this separately from running the container, so we can timeout on the true runtime.
    pl::Exec("docker pull " + kImage);

    // Now run the server.
    // Run with timeout, as a backup in case we don't clean things up properly.
    std::string name = absl::StrCat("namespace_dummy_",
                                    std::chrono::steady_clock::now().time_since_epoch().count());
    ASSERT_OK(
        container_.Start({"timeout", "300", "docker", "run", "--rm", "--name", name, kImage}));

    sleep(2);

    // Wait for container's server to be running.
    const int kAttempts = 10;
    for (int i = 0; i < kAttempts; ++i) {
      // Get the pid of the container, which is the part that has runs in its own network namespace.
      std::string pid_str =
          pl::Exec(absl::Substitute("docker inspect -f '{{.State.Pid}}' $0", name)).ValueOrDie();

      if (absl::SimpleAtoi(pid_str, &target_pid_)) {
        break;
      }
      target_pid_ = -1;

      // Delay before trying again.
      LOG(INFO) << "Test Setup: Server not ready, will try again.";
      sleep(2);
    }

    ASSERT_NE(target_pid_, -1);
  }

  void TearDown() override {
    // Clean-up the container.
    container_.Signal(SIGINT);
    container_.Wait();
  }

  // Use the email service from hipster-shop. Really any service will do.
  inline static const std::string kImage =
      "gcr.io/google-samples/microservices-demo/emailservice:v0.1.3";

  SubProcess container_;
  int target_pid_;
};

TEST_F(NetlinkSocketProberNamespaceTest, Basic) {
  {
    StatusOr<std::unique_ptr<NetlinkSocketProber>> socket_prober_or = NetlinkSocketProber::Create();
    ASSERT_OK(socket_prober_or);
    std::unique_ptr<NetlinkSocketProber> socket_prober = socket_prober_or.ConsumeValueOrDie();

    std::map<int, SocketInfo> socket_info_entries;
    ASSERT_OK(socket_prober->InetConnections(&socket_info_entries,
                                             kTCPEstablishedState | kTCPListeningState));
    int num_conns = socket_info_entries.size();

    // Assume that on any reasonable host, there will be more than 1 connection in the default
    // network namespace.
    EXPECT_GT(num_conns, 1);
  }

  {
    StatusOr<std::unique_ptr<NetlinkSocketProber>> socket_prober_or =
        NetlinkSocketProber::Create(target_pid_);
    ASSERT_OK(socket_prober_or);
    std::unique_ptr<NetlinkSocketProber> socket_prober = socket_prober_or.ConsumeValueOrDie();

    std::map<int, SocketInfo> socket_info_entries;
    ASSERT_OK(socket_prober->InetConnections(&socket_info_entries,
                                             kTCPEstablishedState | kTCPListeningState));
    int num_conns = socket_info_entries.size();

    EXPECT_EQ(num_conns, 1);
  }

  {
    StatusOr<std::unique_ptr<NetlinkSocketProber>> socket_prober_or = NetlinkSocketProber::Create();
    ASSERT_OK(socket_prober_or);
    std::unique_ptr<NetlinkSocketProber> socket_prober = socket_prober_or.ConsumeValueOrDie();

    std::map<int, SocketInfo> socket_info_entries;
    ASSERT_OK(socket_prober->InetConnections(&socket_info_entries,
                                             kTCPEstablishedState | kTCPListeningState));
    int num_conns = socket_info_entries.size();

    // Assume that on any reasonable host, there will be more than 1 connection in the default
    // network namespace.
    EXPECT_GT(num_conns, 1);
  }
}

TEST_F(NetlinkSocketProberNamespaceTest, SocketProberManager) {
  std::map<uint32_t, std::vector<int>> pids_by_net_ns =
      PIDsByNetNamespace(system::Config::GetInstance().proc_path());

  // At least two net namespaces: default, and the container we made during SetUp().
  EXPECT_GE(pids_by_net_ns.size(), 2);

  SocketProberManager socket_probers;

  // First round: map should be empty.
  for (auto& [ns, pids] : pids_by_net_ns) {
    PL_UNUSED(pids);
    NetlinkSocketProber* socket_prober_ptr = socket_probers.GetSocketProber(ns);
    EXPECT_EQ(socket_prober_ptr, nullptr);
  }

  // Second round: map should become populated.
  for (auto& [ns, pids] : pids_by_net_ns) {
    // This might be flaky, if a network namespace was destroyed from the initial query to now.
    // Could cause false test failures.
    StatusOr<NetlinkSocketProber*> socket_prober_ptr_or =
        socket_probers.GetOrCreateSocketProber(ns, pids);
    ASSERT_OK(socket_prober_ptr_or);
    NetlinkSocketProber* socket_prober_ptr = socket_prober_ptr_or.ValueOrDie();
    EXPECT_NE(socket_prober_ptr, nullptr);
  }

  // Third round: map should be populated.
  for (auto& [ns, pids] : pids_by_net_ns) {
    PL_UNUSED(pids);
    NetlinkSocketProber* socket_prober_ptr = socket_probers.GetSocketProber(ns);
    EXPECT_NE(socket_prober_ptr, nullptr);
  }

  // Fourth round: A call to RemovedUnused() should not remove any sockets yet.
  socket_probers.RemoveUnused();
  for (auto& [ns, pids] : pids_by_net_ns) {
    PL_UNUSED(pids);
    NetlinkSocketProber* socket_prober_ptr = socket_probers.GetSocketProber(ns);
    EXPECT_NE(socket_prober_ptr, nullptr);
  }

  // Fifth round: A call to RemoveUnused(), followed by no accesses.
  socket_probers.RemoveUnused();
  // Don't access any socket probers.

  // Sixth round: If socket probers are not accessed, then they should have all been removed.
  socket_probers.RemoveUnused();
  for (auto& [ns, pids] : pids_by_net_ns) {
    PL_UNUSED(pids);
    NetlinkSocketProber* socket_prober_ptr = socket_probers.GetSocketProber(ns);
    EXPECT_EQ(socket_prober_ptr, nullptr);
  }
}

}  // namespace system
}  // namespace pl
