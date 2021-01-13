#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "src/common/base/base.h"
#include "src/common/base/test_utils.h"
#include "src/common/exec/exec.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/testing/common.h"
#include "src/stirling/testing/socket_trace_bpf_test_fixture.h"

namespace pl {
namespace stirling {

using ::pl::stirling::testing::SocketTraceBPFTest;
using ::pl::testing::BazelBinTestFilePath;
using ::pl::testing::TestFilePath;

using ::testing::Contains;
using ::testing::Key;
using ::testing::Not;

constexpr std::string_view kServerPath =
    "src/stirling/source_connectors/socket_tracer/protocols/http/testing/leaky_cpp_http_server/"
    "leaky_http_server";

using BPFMapLeakTest = SocketTraceBPFTest<>;

TEST_F(BPFMapLeakTest, unclosed_connection) {
  const int kInactivitySeconds = 10;
  ConnectionTracker::set_inactivity_duration(std::chrono::seconds(kInactivitySeconds));

  // Create and run the server with a leaky FD.
  std::filesystem::path server_path = BazelBinTestFilePath(kServerPath);
  ASSERT_OK(fs::Exists(server_path));

  SubProcess server;
  ASSERT_OK(server.Start({server_path}));

  uint64_t pid = server.child_pid();
  uint32_t fd = 4;
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
  server.Kill();

  sleep(1);

  // At this point, server should have been traced.
  // And because it was killed, it should have leaked a BPF map entry.

  DataTable data_table(kHTTPTable);
  auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(source_.get());
  ebpf::BPFHashTable<uint64_t, struct conn_info_t> conn_info_map =
      socket_trace_connector->bpf().get_hash_table<uint64_t, struct conn_info_t>("conn_info_map");
  std::vector<std::pair<uint64_t, struct conn_info_t>> entries;

  // Confirm that the leaked BPF map entry exists.
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  entries = conn_info_map.get_table_offline();
  EXPECT_THAT(entries, Contains(Key(server_bpf_map_key)));

  sleep(kInactivitySeconds);

  // This TranfserData should cause the connection tracker to be marked for death.
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);

  // One more iteration for the tracker to be destroyed and to release the BPF map entry.
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);

  // Check that the leaked BPF map entry is removed.
  entries = conn_info_map.get_table_offline();
  EXPECT_THAT(entries, Not(Contains(Key(server_bpf_map_key))));
}

}  // namespace stirling
}  // namespace pl
