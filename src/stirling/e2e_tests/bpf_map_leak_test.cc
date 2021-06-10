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
#include "src/common/base/test_utils.h"
#include "src/common/exec/exec.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

using ::px::stirling::testing::SocketTraceBPFTest;
using ::px::testing::BazelBinTestFilePath;
using ::px::testing::TestFilePath;

using ::testing::Contains;
using ::testing::Key;
using ::testing::Not;

class BPFMapLeakTest : public SocketTraceBPFTest<>,
                       public ::testing::WithParamInterface<std::string_view> {};

TEST_P(BPFMapLeakTest, UnclosedConnection) {
  // Disable the temporary safety net, since it might interfere.
  FLAGS_stirling_enable_periodic_bpf_map_cleanup = false;

  std::string_view server_path_param = GetParam();

  const int kInactivitySeconds = 10;
  ConnTracker::set_inactivity_duration(std::chrono::seconds(kInactivitySeconds));

  // Create and run the server with a leaky FD.
  std::filesystem::path server_path = BazelBinTestFilePath(server_path_param);
  ASSERT_OK(fs::Exists(server_path));

  SubProcess server;
  ASSERT_OK(server.Start({server_path}));

  uint64_t pid = server.child_pid();
  int32_t fd = 4;
  uint64_t server_bpf_map_key = (pid << 32) | fd;
  LOG(INFO) << absl::StrFormat("Server: pid=%d fd=%d key=%x", pid, fd, server_bpf_map_key);

  // Sleep a bit, just to make sure server is ready.
  sleep(1);

  // Now connect to the server.
  SubProcess client;
  ASSERT_OK(client.Start({"curl", "-s", "localhost:8080"}));

  // Sleep a little, to make sure connection is made.
  sleep(1);

  // Now kill the server, which should cause a BPF map entry to leak.
  LOG(INFO) << "Killing server";
  server.Signal(SIGINT);

  sleep(1);

  // At this point, server should have been traced.
  // And because it was killed, it should have leaked a BPF map entry.

  // For testing, make sure Stirling cleans up conn trackers right away.
  FLAGS_stirling_conn_tracker_cleanup_threshold = 0.0;

  // For testing, make sure Stirling cleans up BPF entries right away.
  // Without this flag, Stirling delays clean-up to accumulate a clean-up batch.
  FLAGS_stirling_conn_map_cleanup_threshold = 1;
  FLAGS_stirling_conn_stats_sampling_ratio = 1;

  testing::DataTables data_tables(SocketTraceConnector::kTables);

  auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(source_.get());
  ebpf::BPFHashTable<uint64_t, struct conn_info_t> conn_info_map =
      socket_trace_connector->GetHashTable<uint64_t, struct conn_info_t>("conn_info_map");
  std::vector<std::pair<uint64_t, struct conn_info_t>> entries;

  // Confirm that the leaked BPF map entry exists.
  source_->TransferData(ctx_.get(), data_tables.tables());
  entries = conn_info_map.get_table_offline();
  EXPECT_THAT(entries, Contains(Key(server_bpf_map_key)));

  sleep(kInactivitySeconds);

  // This TransferData should cause the connection tracker to be marked for death.
  source_->TransferData(ctx_.get(), data_tables.tables());

  // One iteration for ConnStats to approve the death.
  source_->TransferData(ctx_.get(), data_tables.tables());

  // One more iteration for the tracker to be destroyed and to release the BPF map entry.
  source_->TransferData(ctx_.get(), data_tables.tables());

  // Check that the leaked BPF map entry is removed.
  entries = conn_info_map.get_table_offline();
  EXPECT_THAT(entries, Not(Contains(Key(server_bpf_map_key))));
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
