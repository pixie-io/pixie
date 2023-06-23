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

#include "src/common/testing/testing.h"
#include "src/stirling/core/output.h"
#include "src/stirling/source_connectors/socket_tracer/testing/client_server_system.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/curl_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/nginx_openssl_1_1_1_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

using ::px::stirling::testing::AccessRecordBatch;
using ::px::stirling::testing::ClientServerSystem;
using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::px::stirling::testing::FindRecordsMatchingPID;
using ::px::stirling::testing::SendRecvScript;
using ::px::stirling::testing::TCPSocket;

using ::testing::IsEmpty;

constexpr int kUPIDIdx = conn_stats_idx::kUPID;
constexpr int kConnOpenIdx = conn_stats_idx::kConnOpen;
constexpr int kConnCloseIdx = conn_stats_idx::kConnClose;
constexpr int kBytesSentIdx = conn_stats_idx::kBytesSent;
constexpr int kBytesRecvIdx = conn_stats_idx::kBytesRecv;
constexpr int kAddrFamilyIdx = conn_stats_idx::kAddrFamily;
constexpr int kProtocolIdx = conn_stats_idx::kProtocol;
constexpr int kRoleIdx = conn_stats_idx::kRole;
constexpr int kSSLIdx = conn_stats_idx::kSSL;

// Turn off client-side tracing. Unlike data tracing, conn-stats should still trace client-side even
// with the client-side tracing turned off.
class ConnStatsBPFTest : public testing::SocketTraceBPFTestFixture</* TClientSideTracing */ false> {
 public:
  ConnStatsBPFTest() { FLAGS_stirling_conn_stats_sampling_ratio = 1; }
};

TEST_F(ConnStatsBPFTest, UnclassifiedEvents) {
  StartTransferDataThread();

  SendRecvScript script = {{{"req1"}, {"resp1"}}, {{"req2"}, {"resp2"}}};

  // The server needs to be slow, so that the client is alive long enough for it to be discovered
  // by the TransferDataThread.
  std::chrono::milliseconds server_response_latency{200};

  ClientServerSystem cs(server_response_latency);
  cs.RunClientServer<&TCPSocket::Read, &TCPSocket::Write>(script);

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kConnStatsTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& rb, tablets);
  // PX_LOG_VAR(PrintConnStatsTable(rb));

  // Check server-side stats.
  {
    auto indices = FindRecordIdxMatchesPID(rb, kUPIDIdx, cs.ServerPID());
    ASSERT_FALSE(indices.empty());

    // ConnStats may have produced various updates during the lifetime of the ClientServerSystem.
    // Grab the last record, which has the final information.
    int idx = indices.back();

    int conn_open = AccessRecordBatch<types::Int64Value>(rb, kConnOpenIdx, idx).val;
    int conn_close = AccessRecordBatch<types::Int64Value>(rb, kConnCloseIdx, idx).val;
    int bytes_sent = AccessRecordBatch<types::Int64Value>(rb, kBytesSentIdx, idx).val;
    int bytes_rcvd = AccessRecordBatch<types::Int64Value>(rb, kBytesRecvIdx, idx).val;
    int addr_family = AccessRecordBatch<types::Int64Value>(rb, kAddrFamilyIdx, idx).val;
    int protocol = AccessRecordBatch<types::Int64Value>(rb, kProtocolIdx, idx).val;
    int role = AccessRecordBatch<types::Int64Value>(rb, kRoleIdx, idx).val;

    EXPECT_THAT(conn_open, 1);
    EXPECT_THAT(conn_close, 1);
    EXPECT_THAT(bytes_sent, 10);
    EXPECT_THAT(bytes_rcvd, 8);
    EXPECT_THAT(addr_family, static_cast<int>(SockAddrFamily::kIPv4));
    EXPECT_THAT(protocol, kProtocolUnknown);
    EXPECT_THAT(role, kRoleServer);
  }

  // Check client-side stats.
  {
    auto indices = FindRecordIdxMatchesPID(rb, kUPIDIdx, cs.ClientPID());
    ASSERT_FALSE(indices.empty());

    // ConnStats may have produced various updates during the lifetime of the ClientServerSystem.
    // Grab the last record, which has the final information.
    int idx = indices.back();

    int conn_open = AccessRecordBatch<types::Int64Value>(rb, kConnOpenIdx, idx).val;
    int conn_close = AccessRecordBatch<types::Int64Value>(rb, kConnCloseIdx, idx).val;
    int bytes_sent = AccessRecordBatch<types::Int64Value>(rb, kBytesSentIdx, idx).val;
    int bytes_rcvd = AccessRecordBatch<types::Int64Value>(rb, kBytesRecvIdx, idx).val;
    int addr_family = AccessRecordBatch<types::Int64Value>(rb, kAddrFamilyIdx, idx).val;
    int protocol = AccessRecordBatch<types::Int64Value>(rb, kProtocolIdx, idx).val;
    int role = AccessRecordBatch<types::Int64Value>(rb, kRoleIdx, idx).val;
    int ssl = AccessRecordBatch<types::BoolValue>(rb, kSSLIdx, idx).val;

    EXPECT_THAT(conn_open, 1);
    EXPECT_THAT(conn_close, 1);
    EXPECT_THAT(bytes_sent, 8);
    EXPECT_THAT(bytes_rcvd, 10);
    EXPECT_THAT(addr_family, static_cast<int>(SockAddrFamily::kIPv4));
    EXPECT_THAT(protocol, kProtocolUnknown);
    EXPECT_THAT(role, kRoleClient);
    EXPECT_THAT(ssl, false);
  }
}

// This test checks that BPF is recording the role properly with only the accept/connect syscalls.
// Expectation is that summary stats are collected. The bytes transferred will be zero,
// but we should see the connect/accept for both client and server.
TEST_F(ConnStatsBPFTest, RoleFromConnectAccept) {
  StartTransferDataThread();

  // No data transfer, since we want to see if we can infer role from connect/accept syscalls.
  testing::SendRecvScript script({});

  // The server needs to be slow, so that the client is alive long enough for it to be discovered
  // by the TransferDataThread.
  std::chrono::milliseconds server_response_latency{200};

  ClientServerSystem system(server_response_latency);
  system.RunClientServer<&TCPSocket::Recv, &TCPSocket::Send>(script);

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kConnStatsTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& rb, tablets);

  // Check client-side.
  {
    auto indices = FindRecordIdxMatchesPID(rb, kUPIDIdx, system.ClientPID());
    ASSERT_FALSE(indices.empty());

    // ConnStats may have produced various updates during the lifetime of the ClientServerSystem.
    // Grab the last record, which has the final information.
    int idx = indices.back();

    int conn_open = AccessRecordBatch<types::Int64Value>(rb, kConnOpenIdx, idx).val;
    int conn_close = AccessRecordBatch<types::Int64Value>(rb, kConnCloseIdx, idx).val;
    int bytes_sent = AccessRecordBatch<types::Int64Value>(rb, kBytesSentIdx, idx).val;
    int bytes_rcvd = AccessRecordBatch<types::Int64Value>(rb, kBytesRecvIdx, idx).val;
    int addr_family = AccessRecordBatch<types::Int64Value>(rb, kAddrFamilyIdx, idx).val;
    int protocol = AccessRecordBatch<types::Int64Value>(rb, kProtocolIdx, idx).val;
    int role = AccessRecordBatch<types::Int64Value>(rb, kRoleIdx, idx).val;
    int ssl = AccessRecordBatch<types::BoolValue>(rb, kSSLIdx, idx).val;

    EXPECT_THAT(protocol, kProtocolUnknown);
    EXPECT_THAT(role, kRoleClient);
    EXPECT_THAT(addr_family, static_cast<int>(SockAddrFamily::kIPv4));
    EXPECT_THAT(conn_open, 1);
    EXPECT_THAT(conn_close, 1);
    EXPECT_THAT(bytes_sent, 0);
    EXPECT_THAT(bytes_rcvd, 0);
    EXPECT_THAT(ssl, false);
  }

  // Check server-side.
  {
    auto indices = FindRecordIdxMatchesPID(rb, kUPIDIdx, system.ServerPID());
    ASSERT_FALSE(indices.empty());

    // ConnStats may have produced various updates during the lifetime of the ClientServerSystem.
    // Grab the last record, which has the final information.
    int idx = indices.back();

    int conn_open = AccessRecordBatch<types::Int64Value>(rb, kConnOpenIdx, idx).val;
    int conn_close = AccessRecordBatch<types::Int64Value>(rb, kConnCloseIdx, idx).val;
    int bytes_sent = AccessRecordBatch<types::Int64Value>(rb, kBytesSentIdx, idx).val;
    int bytes_rcvd = AccessRecordBatch<types::Int64Value>(rb, kBytesRecvIdx, idx).val;
    int addr_family = AccessRecordBatch<types::Int64Value>(rb, kAddrFamilyIdx, idx).val;
    int protocol = AccessRecordBatch<types::Int64Value>(rb, kProtocolIdx, idx).val;
    int role = AccessRecordBatch<types::Int64Value>(rb, kRoleIdx, idx).val;
    int ssl = AccessRecordBatch<types::BoolValue>(rb, kSSLIdx, idx).val;

    EXPECT_THAT(protocol, kProtocolUnknown);
    EXPECT_THAT(role, kRoleServer);
    EXPECT_THAT(addr_family, static_cast<int>(SockAddrFamily::kIPv4));
    EXPECT_THAT(conn_open, 1);
    EXPECT_THAT(conn_close, 1);
    EXPECT_THAT(bytes_sent, 0);
    EXPECT_THAT(bytes_rcvd, 0);
    EXPECT_THAT(ssl, false);
  }
}

// Test fixture that starts SocketTraceConnector after the connection was already established.
class ConnStatsMidConnBPFTest
    : public testing::SocketTraceBPFTestFixture</* TClientSideTracing */ false> {
 protected:
  ConnStatsMidConnBPFTest() { FLAGS_stirling_conn_stats_sampling_ratio = 1; }

  void SetUp() override {
    LOG(INFO) << absl::Substitute("Test PID = $0", getpid());
    // Uncomment to enable tracing:
    // FLAGS_stirling_conn_trace_pid = getpid();

    server_listener_.BindAndListen();
    client_.Connect(server_listener_);
    server_ = server_listener_.Accept();

    // Now SocketTraceConnector is created after the connection was already established.
    SocketTraceBPFTestFixture::SetUp();
  }

  void TearDown() override {
    client_.Close();
    server_listener_.Close();
    if (server_ != nullptr) {
      server_->Close();
    }
    SocketTraceBPFTestFixture::TearDown();
  }

  TCPSocket server_listener_;
  std::unique_ptr<TCPSocket> server_;

  TCPSocket client_;
};

TEST_F(ConnStatsMidConnBPFTest, DidNotSeeConnEstablishment) {
  StartTransferDataThread();

  std::string_view test_msg = "Hello World!";
  EXPECT_EQ(test_msg.size(), client_.Send(test_msg));
  std::string text;
  while (!server_->Recv(&text)) {
  }

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kConnStatsTableNum);
  if (!tablets.empty()) {
    types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

    auto indices = FindRecordIdxMatchesPID(record_batch, conn_stats_idx::kUPID, getpid());
    ASSERT_THAT(indices, IsEmpty());
  }
}

TEST_F(ConnStatsMidConnBPFTest, InferRemoteEndpointAndReport) {
  StartTransferDataThread();

  // Uncomment to enable tracing:
  // FLAGS_stirling_conn_trace_pid = getpid();

  std::string_view test_msg = "Hello World!";
  EXPECT_EQ(test_msg.size(), client_.Send(test_msg));
  std::string text;
  while (!server_->Read(&text)) {
  }

  // Make sure traffic spans across multiple TransferData calls,
  // so that connection inference has a chance to kick in.
  // If this test becomes flaky, may want to try bumping this up to 4.
  for (int i = 0; i < 10000; ++i) {
    EXPECT_EQ(test_msg.size(), client_.Send(test_msg));
    while (!server_->Recv(&text)) {
    }
  }

  std::this_thread::sleep_for(2 * kTransferDataPeriod);

  for (int i = 0; i < 10000; ++i) {
    EXPECT_EQ(test_msg.size(), client_.Send(test_msg));
    while (!server_->Recv(&text)) {
    }
  }

  std::this_thread::sleep_for(2 * kTransferDataPeriod);

  for (int i = 0; i < 10000; ++i) {
    EXPECT_EQ(test_msg.size(), client_.Send(test_msg));
    while (!server_->Recv(&text)) {
    }
  }

  server_->Close();
  client_.Close();

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kConnStatsTableNum);

  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& rb, tablets);
  // PX_LOG_VAR(PrintConnStatsTable(rb));

  // Check client-side.
  {
    auto indices = FindRecordIdxMatchesPID(rb, conn_stats_idx::kUPID, getpid());

    // One of the records is the server and the other is the client.
    int s_idx = -1;
    int c_idx = -1;

    for (auto idx : indices) {
      if (rb[kRoleIdx]->Get<types::Int64Value>(idx).val == kRoleClient) {
        c_idx = idx;
      } else {
        s_idx = idx;
      }
    }

    // Expect to find at least one record in each direction.
    ASSERT_NE(c_idx, -1);
    ASSERT_NE(s_idx, -1);

    // Check that we properly found a client and server record.
    EXPECT_THAT(rb[kRoleIdx]->Get<types::Int64Value>(c_idx).val, kRoleClient);
    EXPECT_THAT(rb[kRoleIdx]->Get<types::Int64Value>(s_idx).val, kRoleServer);

    // Check client record.
    EXPECT_THAT(rb[kProtocolIdx]->Get<types::Int64Value>(c_idx), kProtocolUnknown);
    EXPECT_THAT(rb[kConnOpenIdx]->Get<types::Int64Value>(c_idx).val, 1);
    EXPECT_THAT(rb[kConnCloseIdx]->Get<types::Int64Value>(c_idx).val, 1);
    EXPECT_THAT(rb[kBytesSentIdx]->Get<types::Int64Value>(c_idx).val, 360012);
    EXPECT_THAT(rb[kBytesRecvIdx]->Get<types::Int64Value>(c_idx).val, 0);

    // Check server record.
    EXPECT_THAT(rb[kProtocolIdx]->Get<types::Int64Value>(s_idx), kProtocolUnknown);
    EXPECT_THAT(rb[kConnOpenIdx]->Get<types::Int64Value>(s_idx).val, 1);
    EXPECT_THAT(rb[kConnCloseIdx]->Get<types::Int64Value>(s_idx).val, 1);
    EXPECT_THAT(rb[kBytesSentIdx]->Get<types::Int64Value>(s_idx).val, 0);
    EXPECT_THAT(rb[kBytesRecvIdx]->Get<types::Int64Value>(s_idx).val, 360012);
  }
}

TEST_F(ConnStatsBPFTest, SSLConnections) {
  ::px::stirling::testing::NginxOpenSSL_1_1_1_Container server;
  ::px::stirling::testing::CurlContainer client;

  // Run the nginx HTTPS server.
  // The container runner will make sure it is in the ready state before unblocking.
  StatusOr<std::string> run_result = server.Run(std::chrono::seconds{60});
  PX_CHECK_OK(run_result);

  // Sleep an additional second, just to be safe.
  sleep(1);

  StartTransferDataThread();

  // Make an SSL request with curl.
  // Because the server uses a self-signed certificate, curl will normally refuse to connect.
  // This is similar to the warning pages that Firefox/Chrome would display.
  // To take an exception and make the SSL connection anyways, we use the --insecure flag.

  // Run the client in the network of the server, so they can connect to each other.
  PX_CHECK_OK(client.Run(std::chrono::seconds{60},
                         {absl::Substitute("--network=container:$0", server.container_name())},
                         {"--insecure", "-s", "-S", "https://127.0.0.1:443/index.html"}));
  client.Wait();

  StopTransferDataThread();

  int server_pid = server.NginxWorkerPID();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kConnStatsTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& rb, tablets);
  types::ColumnWrapperRecordBatch records = FindRecordsMatchingPID(rb, kUPIDIdx, server_pid);
  // PX_LOG_VAR(PrintConnStatsTable(records));

  // Check server-side stats.
  {
    auto indices = FindRecordIdxMatchesPID(tablets[0].records, kUPIDIdx, server_pid);
    ASSERT_FALSE(indices.empty());

    // ConnStats may have produced various updates during the lifetime of the ClientServerSystem.
    // Grab the last record, which has the final information.
    int idx = indices.back();

    int conn_open = AccessRecordBatch<types::Int64Value>(rb, kConnOpenIdx, idx).val;
    int conn_close = AccessRecordBatch<types::Int64Value>(rb, kConnCloseIdx, idx).val;
    int bytes_sent = AccessRecordBatch<types::Int64Value>(rb, kBytesSentIdx, idx).val;
    int bytes_rcvd = AccessRecordBatch<types::Int64Value>(rb, kBytesRecvIdx, idx).val;
    int addr_family = AccessRecordBatch<types::Int64Value>(rb, kAddrFamilyIdx, idx).val;
    int protocol = AccessRecordBatch<types::Int64Value>(rb, kProtocolIdx, idx).val;
    int role = AccessRecordBatch<types::Int64Value>(rb, kRoleIdx, idx).val;
    int ssl = AccessRecordBatch<types::BoolValue>(rb, kSSLIdx, idx).val;

    EXPECT_THAT(conn_open, 1);
    EXPECT_THAT(conn_close, 1);
    // TODO(oazizi): The number of encrypted bytes is different than the number of plaintext bytes.
    //               At the conn_stats level, the right number should be what's on the connection,
    //               so it should be encrypted bytes. But the way we do things today,
    //               we're counting the plaintext bytes (because that's what we trace).
    //               bytes_sent/bytes_rcvd should be 2730 and 1029 respectively
    //               once we perform our accounting on encrypted data.
    // The TLS handshake has 4 less bytes when using 127.0.0.1 instead of localhost.
    EXPECT_THAT(bytes_sent, 2486);
    EXPECT_THAT(bytes_rcvd, 698);
    EXPECT_THAT(addr_family, static_cast<int>(SockAddrFamily::kIPv4));
    EXPECT_THAT(protocol, kProtocolHTTP);
    EXPECT_THAT(role, kRoleServer);
    EXPECT_THAT(ssl, true);
  }
}

}  // namespace stirling
}  // namespace px
