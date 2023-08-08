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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "src/common/base/base.h"
#include "src/common/exec/exec.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/stirling/core/data_tables.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

using ::px::stirling::testing::SocketTraceBPFTestFixture;
using ::px::testing::BazelRunfilePath;

using ::testing::Contains;
using ::testing::Key;
using ::testing::Not;

using ConnInfoMapT = bpf_tools::WrappedBCCMap<uint64_t, struct conn_info_t>;

class BPFMapLeakTest : public testing::SocketTraceBPFTestFixture<>,
                       public ::testing::WithParamInterface<std::string_view> {};

TEST_P(BPFMapLeakTest, UnclosedConnection) {
  // Disable the temporary safety net, since it might interfere.
  FLAGS_stirling_enable_periodic_bpf_map_cleanup = false;

  std::string_view server_path_param = GetParam();

  std::chrono::milliseconds kInactivityPeriod{100};
  ConnTracker::set_inactivity_duration(
      std::chrono::duration_cast<std::chrono::seconds>(kInactivityPeriod));

  // Create and run the server with a leaky FD.
  std::filesystem::path server_path = BazelRunfilePath(server_path_param);
  ASSERT_TRUE(fs::Exists(server_path));

  SubProcess server;
  ASSERT_OK(server.Start({server_path}, /* stderr_to_stdout */ true));

  // Wait for the server to come online.
  std::string server_stdout;
  bool server_started = false;
  for (int iters = 0; iters < 60; iters++) {
    ASSERT_OK(server.Stdout(&server_stdout));
    if (absl::StrContains(server_stdout, "Listening for connections on port")) {
      server_started = true;
      break;
    }
    // Sleep a bit to wait for the server to become ready.
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  ASSERT_TRUE(server_started);

  // Now connect to the server.
  SubProcess client;
  ASSERT_OK(client.Start({"curl", "-s", "localhost:8080"}));

  // Wait for the client connection to be made.
  bool matched = false;
  int32_t fd;
  for (int iters = 0; iters < 60; iters++) {
    ASSERT_OK(server.Stdout(&server_stdout));

    std::regex regex(".*Accepted connection on fd=(\\d+).*");
    std::smatch match;
    LOG(INFO) << server_stdout;
    if (std::regex_search(server_stdout, match, regex)) {
      // Get the FD of the connection from the server's logs.
      std::string fd_str = match[1];
      ASSERT_TRUE(absl::SimpleAtoi(fd_str, &fd));
      matched = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  ASSERT_TRUE(matched);

  // Now we know the bpf map key that should leak.
  uint64_t pid = server.child_pid();
  uint64_t server_bpf_map_key = (pid << 32) | fd;
  LOG(INFO) << absl::StrFormat("Server: pid=%d fd=%d key=%x", pid, fd, server_bpf_map_key);

  // Now kill the server, which should cause a BPF map entry to leak.
  LOG(INFO) << "Killing server";
  server.Signal(SIGINT);
  server.Wait();

  // At this point, server should have been traced.
  // And because it was killed, it should have leaked a BPF map entry.

  // For testing, make sure Stirling cleans up conn trackers right away.
  FLAGS_stirling_conn_tracker_cleanup_threshold = 0.0;

  // For testing, make sure Stirling cleans up BPF entries right away.
  // Without this flag, Stirling delays clean-up to accumulate a clean-up batch.
  FLAGS_stirling_conn_map_cleanup_threshold = 1;
  FLAGS_stirling_conn_stats_sampling_ratio = 1;

  DataTables data_tables(SocketTraceConnector::kTables);

  auto conn_info_map = ConnInfoMapT::Create(&source_->BCC(), "conn_info_map");

  // Confirm that the leaked BPF map entry exists.
  source_->TransferData(ctx_.get());
  EXPECT_THAT(conn_info_map->GetTableOffline(), Contains(Key(server_bpf_map_key)));

  std::this_thread::sleep_for(kInactivityPeriod);

  // This TransferData should cause the connection tracker to be marked for death.
  source_->TransferData(ctx_.get());

  // One iteration for ConnStats to approve the death.
  source_->TransferData(ctx_.get());

  // One more iteration for the tracker to be destroyed and to release the BPF map entry.
  source_->TransferData(ctx_.get());

  // Check that the leaked BPF map entry is removed.
  EXPECT_THAT(conn_info_map->GetTableOffline(), Not(Contains(Key(server_bpf_map_key))));
}

constexpr std::string_view kTCPServerPath =
    "src/stirling/testing/demo_apps/leaky_http_server/server";
// constexpr std::string_view kUnixServerPath =
// "src/stirling/testing/demo_apps/leaky_http_unix_socket_server/server";

INSTANTIATE_TEST_SUITE_P(SocketTypeSuite, BPFMapLeakTest, ::testing::Values(kTCPServerPath));

// TODO(oazizi): Add kUnixServerPath into the test suite after fixing the BPF map leak with Unix
//               domain sockets.

}  // namespace stirling
}  // namespace px
