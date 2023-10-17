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

#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/types.h>
#include <unistd.h>
#include <string_view>
#include <thread>

#include <magic_enum.hpp>

#include "src/common/fs/temp_file.h"
#include "src/common/system/clock.h"
#include "src/common/system/tcp_socket.h"
#include "src/common/system/udp_socket.h"
#include "src/common/system/unix_socket.h"
#include "src/shared/metadata/metadata.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/stirling/core/data_table.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/source_connectors/socket_tracer/testing/client_server_system.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

using ::px::stirling::testing::FindRecordsMatchingPID;
using ::px::stirling::testing::RecordBatchSizeIs;
using ::px::system::TCPSocket;
using ::px::system::UDPSocket;
using ::px::system::UnixSocket;
using ::px::types::ColumnWrapperRecordBatch;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::StrEq;

constexpr std::string_view kHTTPReqMsg1 =
    "GET /endpoint1 HTTP/1.1\r\n"
    "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:67.0) Gecko/20100101 Firefox/67.0\r\n"
    "\r\n";

constexpr std::string_view kHTTPReqMsg2 =
    "GET /endpoint2 HTTP/1.1\r\n"
    "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:67.0) Gecko/20100101 Firefox/67.0\r\n"
    "\r\n";

constexpr std::string_view kHTTPRespMsg1 =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json; msg1\r\n"
    "Content-Length: 0\r\n"
    "\r\n";

constexpr std::string_view kHTTPRespMsg2 =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json; msg2\r\n"
    "Content-Length: 0\r\n"
    "\r\n";

// TODO(yzhao): Apply this pattern to other syscall pairs. An issue is that other syscalls do not
// use scatter buffer. One approach would be to concatenate inner vector to a single string, and
// then feed to the syscall. Another caution is that value-parameterized tests actually discourage
// changing functions being tested according to test parameters. The canonical pattern is using test
// parameters as inputs, but keep the function being tested fixed.
enum class SyscallPair {
  kSendRecv,
  kWriteRead,
  kSendMsgRecvMsg,
  kSendMMsgRecvMMsg,
  kWritevReadv,
};

struct SocketTraceBPFTestParams {
  SyscallPair syscall_pair;
  uint64_t trace_role;
};

class SocketTraceBPFTest
    : public testing::SocketTraceBPFTestFixture</* TClientSideTracing */ true> {
 protected:
  StatusOr<const ConnTracker*> GetConnTracker(int pid, int fd) {
    PX_ASSIGN_OR_RETURN(const ConnTracker* tracker, source_->GetConnTracker(pid, fd));
    if (tracker == nullptr) {
      return error::Internal("No ConnTracker found for pid=$0 fd=$1", pid, fd);
    }
    return tracker;
  }

  StatusOr<ConnTracker*> GetMutableConnTracker(int pid, int fd) {
    conn_id_t conn_id;
    conn_id.tsid = 0;
    for (const auto* conn_tracker : source_->conn_trackers_mgr_.active_trackers()) {
      if (conn_tracker->conn_id().upid.pid == static_cast<uint32_t>(pid) &&
          conn_tracker->conn_id().fd == fd) {
        conn_id = conn_tracker->conn_id();
        break;
      }
    }
    // If tsid=0 then the above loop didn't find any conn trackers with the same {pid, fd} pair.
    if (conn_id.tsid == 0) {
      return error::Internal("No ConnTracker found for pid=$0 fd=$1", pid, fd);
    }
    auto& conn_tracker = source_->GetOrCreateConnTracker(conn_id);
    return &conn_tracker;
  }
};

class NonVecSyscallTests : public SocketTraceBPFTest,
                           public ::testing::WithParamInterface<SocketTraceBPFTestParams> {};

TEST_P(NonVecSyscallTests, NonVecSyscalls) {
  SocketTraceBPFTestParams p = GetParam();
  LOG(INFO) << absl::Substitute("Test parameters: syscall_pair=$0 trace_role=$1",
                                magic_enum::enum_name(p.syscall_pair), p.trace_role);
  ConfigureBPFCapture(kProtocolHTTP, p.trace_role);

  StartTransferDataThread();

  testing::SendRecvScript script({
      {{kHTTPReqMsg1}, {kHTTPRespMsg1}},
      {{kHTTPReqMsg2}, {kHTTPRespMsg2}},
  });

  testing::ClientServerSystem system;
  switch (p.syscall_pair) {
    case SyscallPair::kWriteRead:
      system.RunClientServer<&TCPSocket::Read, &TCPSocket::Write>(script);
      break;
    case SyscallPair::kSendRecv:
      system.RunClientServer<&TCPSocket::Recv, &TCPSocket::Send>(script);
      break;
    case SyscallPair::kSendMMsgRecvMMsg:
      system.RunClientServer<&TCPSocket::RecvMMsg, &TCPSocket::SendMMsg>(script);
      break;
    default:
      LOG(FATAL) << absl::Substitute("$0 not supported by this test",
                                     magic_enum::enum_name(p.syscall_pair));
  }

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(kHTTPTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

  if (p.trace_role & kRoleClient) {
    ColumnWrapperRecordBatch records =
        FindRecordsMatchingPID(record_batch, kHTTPUPIDIdx, system.ClientPID());

    ASSERT_THAT(records, RecordBatchSizeIs(2));

    EXPECT_THAT(records[kHTTPRespHeadersIdx]->Get<types::StringValue>(0), HasSubstr("msg1"));
    EXPECT_THAT(records[kHTTPRespHeadersIdx]->Get<types::StringValue>(1), HasSubstr("msg2"));

    // Additional verifications. These are common to all HTTP1.x tracing, so we decide to not
    // duplicate them on all relevant tests.
    EXPECT_EQ(1, records[kHTTPMajorVersionIdx]->Get<types::Int64Value>(0).val);
    EXPECT_EQ(static_cast<uint64_t>(HTTPContentType::kJSON),
              records[kHTTPContentTypeIdx]->Get<types::Int64Value>(0).val);
    EXPECT_EQ(1, records[kHTTPMajorVersionIdx]->Get<types::Int64Value>(1).val);
    EXPECT_EQ(static_cast<uint64_t>(HTTPContentType::kJSON),
              records[kHTTPContentTypeIdx]->Get<types::Int64Value>(1).val);
  }

  if (p.trace_role & kRoleServer) {
    ColumnWrapperRecordBatch records =
        FindRecordsMatchingPID(record_batch, kHTTPUPIDIdx, system.ServerPID());

    ASSERT_THAT(records, RecordBatchSizeIs(2));

    EXPECT_THAT(records[kHTTPRespHeadersIdx]->Get<types::StringValue>(0), HasSubstr("msg1"));
    EXPECT_THAT(records[kHTTPRespHeadersIdx]->Get<types::StringValue>(1), HasSubstr("msg2"));

    // Additional verifications. These are common to all HTTP1.x tracing, so we decide to not
    // duplicate them on all relevant tests.
    EXPECT_EQ(1, records[kHTTPMajorVersionIdx]->Get<types::Int64Value>(0).val);
    EXPECT_EQ(static_cast<uint64_t>(HTTPContentType::kJSON),
              records[kHTTPContentTypeIdx]->Get<types::Int64Value>(0).val);
    EXPECT_EQ(1, records[kHTTPMajorVersionIdx]->Get<types::Int64Value>(1).val);
    EXPECT_EQ(static_cast<uint64_t>(HTTPContentType::kJSON),
              records[kHTTPContentTypeIdx]->Get<types::Int64Value>(1).val);
  }
}

INSTANTIATE_TEST_SUITE_P(
    NonVecSyscalls, NonVecSyscallTests,
    ::testing::Values(SocketTraceBPFTestParams{SyscallPair::kWriteRead, kRoleClient | kRoleServer},
                      SocketTraceBPFTestParams{SyscallPair::kSendRecv, kRoleClient | kRoleServer},
                      SocketTraceBPFTestParams{SyscallPair::kSendMMsgRecvMMsg,
                                               kRoleClient | kRoleServer}));

class IOVecSyscallTests : public SocketTraceBPFTest,
                          public ::testing::WithParamInterface<SocketTraceBPFTestParams> {};

TEST_P(IOVecSyscallTests, IOVecSyscalls) {
  SocketTraceBPFTestParams p = GetParam();
  LOG(INFO) << absl::Substitute("$0 $1", magic_enum::enum_name(p.syscall_pair), p.trace_role);
  ConfigureBPFCapture(kProtocolHTTP, p.trace_role);

  StartTransferDataThread();

  testing::SendRecvScript script({
      {{kHTTPReqMsg1},
       {"HTTP/1.1 200 OK\r\n", "Content-Type: json\r\n", "Content-Length: 1\r\n\r\na"}},
      {{kHTTPReqMsg2},
       {"HTTP/1.1 404 Not Found\r\n", "Content-Type: json\r\n", "Content-Length: 2\r\n\r\nbc"}},
  });

  testing::ClientServerSystem system;
  switch (p.syscall_pair) {
    case SyscallPair::kSendMsgRecvMsg:
      system.RunClientServer<&TCPSocket::RecvMsg, &TCPSocket::SendMsg>(script);
      break;
    case SyscallPair::kWritevReadv:
      system.RunClientServer<&TCPSocket::ReadV, &TCPSocket::WriteV>(script);
      break;
    default:
      LOG(FATAL) << absl::Substitute("$0 not supported by this test",
                                     magic_enum::enum_name(p.syscall_pair));
  }

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(kHTTPTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

  if (p.trace_role & kRoleServer) {
    ColumnWrapperRecordBatch records =
        FindRecordsMatchingPID(record_batch, kHTTPUPIDIdx, system.ServerPID());

    ASSERT_THAT(records, RecordBatchSizeIs(2));

    EXPECT_EQ(200, records[kHTTPRespStatusIdx]->Get<types::Int64Value>(0).val);
    EXPECT_THAT(std::string(records[kHTTPRespBodyIdx]->Get<types::StringValue>(0)), StrEq("a"));
    EXPECT_THAT(std::string(records[kHTTPRespMessageIdx]->Get<types::StringValue>(0)), StrEq("OK"));

    EXPECT_EQ(404, records[kHTTPRespStatusIdx]->Get<types::Int64Value>(1).val);
    EXPECT_THAT(std::string(records[kHTTPRespBodyIdx]->Get<types::StringValue>(1)), StrEq("bc"));
    EXPECT_THAT(std::string(records[kHTTPRespMessageIdx]->Get<types::StringValue>(1)),
                StrEq("Not Found"));
  }

  if (p.trace_role & kRoleClient) {
    ColumnWrapperRecordBatch records =
        FindRecordsMatchingPID(record_batch, kHTTPUPIDIdx, system.ClientPID());

    ASSERT_THAT(records, RecordBatchSizeIs(2));

    EXPECT_EQ(200, records[kHTTPRespStatusIdx]->Get<types::Int64Value>(0).val);
    EXPECT_THAT(std::string(records[kHTTPRespBodyIdx]->Get<types::StringValue>(0)), StrEq("a"));
    EXPECT_THAT(std::string(records[kHTTPRespMessageIdx]->Get<types::StringValue>(0)), StrEq("OK"));

    EXPECT_EQ(404, records[kHTTPRespStatusIdx]->Get<types::Int64Value>(1).val);
    EXPECT_THAT(std::string(records[kHTTPRespBodyIdx]->Get<types::StringValue>(1)), StrEq("bc"));
    EXPECT_THAT(std::string(records[kHTTPRespMessageIdx]->Get<types::StringValue>(1)),
                StrEq("Not Found"));
  }
}

INSTANTIATE_TEST_SUITE_P(IOVecSyscalls, IOVecSyscallTests,
                         ::testing::Values(SocketTraceBPFTestParams{SyscallPair::kSendMsgRecvMsg,
                                                                    kRoleClient | kRoleServer},
                                           SocketTraceBPFTestParams{SyscallPair::kWritevReadv,
                                                                    kRoleClient | kRoleServer}));

TEST_F(SocketTraceBPFTest, FileIONotTraced) {
  std::unique_ptr<fs::TempFile> tmpf = fs::TempFile::Create();
  std::filesystem::path fpath = tmpf->path();

  int fd1 = open(fpath.c_str(), O_RDWR);

  // Write an HTTP request using the primary FD.
  ASSERT_EQ(write(fd1, kHTTPReqMsg1.data(), kHTTPReqMsg1.size()), kHTTPReqMsg1.size());

  // Write an HTTP response using a secondary FD, so the primary FD can read a response.
  int fd2 = open(fpath.c_str(), O_WRONLY | O_APPEND);
  ASSERT_EQ(write(fd2, kHTTPRespMsg1.data(), kHTTPRespMsg1.size()), kHTTPRespMsg1.size());
  close(fd2);

  // Read the HTTP response using the primary FD.
  std::string rd_data;
  rd_data.resize(kHTTPReqMsg1.size());
  int len = read(fd1, rd_data.data(), rd_data.size());
  ASSERT_GT(len, 0);
  rd_data.resize(len);
  ASSERT_EQ(rd_data, kHTTPRespMsg1);

  close(fd1);

  // Finally drain all BPF events.
  source_->BCC().PollPerfBuffers();

  // Those to file I/O FDs should not have been reported.
  ASSERT_NOT_OK(GetConnTracker(getpid(), fd1));
}

TEST_F(SocketTraceBPFTest, NonInetTrafficNotTraced) {
  UnixSocket server;
  UnixSocket client;

  int server_fd;
  int client_fd;

  std::string unix_socket_path = absl::Substitute(
      "/tmp/unix_sock_$0.server", std::chrono::steady_clock::now().time_since_epoch().count());

  server.BindAndListen(unix_socket_path);

  std::thread client_thread([&server, &client, &client_fd]() {
    client.Connect(server);
    client_fd = client.sockfd();

    ASSERT_EQ(client.Send(kHTTPReqMsg1), kHTTPReqMsg1.size());

    std::string received_data;
    while (received_data.empty()) {
      client.Recv(&received_data);
    }

    ASSERT_EQ(received_data, kHTTPRespMsg1);

    client.Close();
  });

  std::thread server_thread([&server, &server_fd]() {
    std::unique_ptr<UnixSocket> conn = server.Accept(false);
    server_fd = conn->sockfd();

    std::string received_data;
    while (received_data.empty()) {
      conn->Recv(&received_data);
    }
    ASSERT_EQ(received_data, kHTTPReqMsg1);

    ASSERT_EQ(conn->Send(kHTTPRespMsg1), kHTTPRespMsg1.size());

    conn->Close();
  });

  client_thread.join();
  server_thread.join();

  // Finally drain all BPF events.
  source_->BCC().PollPerfBuffers();

  // Those to file I/O FDs should not have been reported.
  ASSERT_NOT_OK(GetConnTracker(getpid(), client_fd));
  ASSERT_NOT_OK(GetConnTracker(getpid(), server_fd));
}

// Tests that SocketTraceConnector won't send data from BPF to userspace if the data were not
// any of the supported protocols.
TEST_F(SocketTraceBPFTest, NoProtocolWritesNotCaptured) {
  constexpr std::string_view kNoProtocolMsg = "This is not an HTTP message";

  testing::SendRecvScript script({
      {{kNoProtocolMsg}, {kNoProtocolMsg}},
      {{kNoProtocolMsg}, {kNoProtocolMsg}},
  });

  testing::ClientServerSystem system;
  system.RunClientServer<&TCPSocket::Read, &TCPSocket::Write>(script);

  source_->BCC().PollPerfBuffers();

  // We expect to see a ConnTracker allocated for ConnStats, but the data buffers should be empty
  // for unknown or unsupported protocols.

  ASSERT_OK_AND_ASSIGN(const auto* tracker, GetConnTracker(system.ClientPID(), system.ClientFD()));
  EXPECT_TRUE(tracker->send_data().data_buffer().empty());
  EXPECT_TRUE(tracker->recv_data().data_buffer().empty());

  ASSERT_OK_AND_ASSIGN(tracker, GetConnTracker(system.ServerPID(), system.ServerFD()));
  EXPECT_TRUE(tracker->send_data().data_buffer().empty());
  EXPECT_TRUE(tracker->recv_data().data_buffer().empty());
}

TEST_F(SocketTraceBPFTest, MultipleConnections) {
  ConfigureBPFCapture(traffic_protocol_t::kProtocolHTTP, kRoleClient);

  StartTransferDataThread();

  // Two separate connections.

  testing::SendRecvScript script1({
      {{kHTTPReqMsg1}, {kHTTPRespMsg1}},
  });
  testing::ClientServerSystem system1;
  system1.RunClientServer<&TCPSocket::Read, &TCPSocket::Write>(script1);

  testing::SendRecvScript script2({
      {{kHTTPReqMsg2}, {kHTTPRespMsg2}},
  });
  testing::ClientServerSystem system2;
  system2.RunClientServer<&TCPSocket::Read, &TCPSocket::Write>(script2);

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(kHTTPTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

  {
    ColumnWrapperRecordBatch records =
        FindRecordsMatchingPID(record_batch, kHTTPUPIDIdx, system1.ClientPID());

    ASSERT_THAT(records, RecordBatchSizeIs(1));
    EXPECT_THAT(records[kHTTPRespHeadersIdx]->Get<types::StringValue>(0), HasSubstr("msg1"));
  }

  {
    ColumnWrapperRecordBatch records =
        FindRecordsMatchingPID(record_batch, kHTTPUPIDIdx, system2.ClientPID());

    ASSERT_THAT(records, RecordBatchSizeIs(1));
    EXPECT_THAT(records[kHTTPRespHeadersIdx]->Get<types::StringValue>(0), HasSubstr("msg2"));
  }
}

// Tests that the start time of UPIDs reported in data table are within a specified time window.
TEST_F(SocketTraceBPFTest, StartTime) {
  ConfigureBPFCapture(traffic_protocol_t::kProtocolHTTP, kRoleClient);

  StartTransferDataThread();

  testing::SendRecvScript script({
      {{kHTTPReqMsg1}, {kHTTPRespMsg1}},
      {{kHTTPReqMsg2}, {kHTTPRespMsg2}},
  });

  testing::ClientServerSystem system;
  system.RunClientServer<&TCPSocket::Recv, &TCPSocket::Send>(script);

  // Kernel uses a special monotonic clock as start_time, so we must do the same.
  auto now = px::chrono::boot_clock::now();

  // Use a time window to make sure the recorded PID start_time is right.
  // Being super generous with the window, just in case test runs slow.
  auto time_window_start_tp = now - std::chrono::minutes(30);
  auto time_window_end_tp = now + std::chrono::minutes(5);

  // Start times are reported by Linux in what is essentially 10 ms units.
  constexpr int64_t kNsecondsPerSecond = 1000 * 1000 * 1000;
  constexpr int64_t kClockTicks = 100;
  constexpr int64_t kDivFactor = kNsecondsPerSecond / kClockTicks;

  auto time_window_start = time_window_start_tp.time_since_epoch().count() / kDivFactor;
  auto time_window_end = time_window_end_tp.time_since_epoch().count() / kDivFactor;

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(kHTTPTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  ColumnWrapperRecordBatch records =
      FindRecordsMatchingPID(record_batch, kHTTPUPIDIdx, system.ClientPID());

  ASSERT_THAT(records, RecordBatchSizeIs(2));

  md::UPID upid0(records[kHTTPUPIDIdx]->Get<types::UInt128Value>(0).val);
  EXPECT_EQ(system.ClientPID(), upid0.pid());
  EXPECT_LT(time_window_start, upid0.start_ts());
  EXPECT_GT(time_window_end, upid0.start_ts());

  md::UPID upid1(records[kHTTPUPIDIdx]->Get<types::UInt128Value>(1).val);
  EXPECT_EQ(system.ClientPID(), upid1.pid());
  EXPECT_LT(time_window_start, upid1.start_ts());
  EXPECT_GT(time_window_end, upid1.start_ts());
}

TEST_F(SocketTraceBPFTest, LargeMessages) {
  ConfigureBPFCapture(traffic_protocol_t::kProtocolHTTP, kRoleClient | kRoleServer);

  std::string large_response =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json; msg2\r\n"
      "Content-Length: 131072\r\n"
      "\r\n";
  large_response += std::string(131072, '+');

  testing::SendRecvScript script({
      {{kHTTPReqMsg1}, {large_response}},
  });

  testing::ClientServerSystem system;
  system.RunClientServer<&TCPSocket::Recv, &TCPSocket::Send>(script);

  source_->BCC().PollPerfBuffers();

  ASSERT_OK_AND_ASSIGN(auto* client_tracker,
                       GetMutableConnTracker(system.ClientPID(), system.ClientFD()));
  EXPECT_EQ(client_tracker->send_data().data_buffer().Head(), kHTTPReqMsg1);
  std::string client_recv_data(client_tracker->recv_data().data_buffer().Head());
  EXPECT_THAT(client_recv_data.size(), 131153);
  EXPECT_THAT(client_recv_data, HasSubstr("+++++"));
  EXPECT_EQ(client_recv_data.substr(client_recv_data.size() - 5, 5), "+++++");

  // The server's send syscall transmits all 131153 bytes in one shot.
  // This is over the limit that we can transmit through BPF, and so we expect
  // filler bytes on this side of the connection. Note that the client doesn't have the
  // same behavior, because the recv syscall provides the data in chunks.
  ASSERT_OK_AND_ASSIGN(auto* server_tracker,
                       GetMutableConnTracker(system.ServerPID(), system.ServerFD()));
  EXPECT_EQ(server_tracker->recv_data().data_buffer().Head(), kHTTPReqMsg1);
  std::string server_send_data(server_tracker->send_data().data_buffer().Head());
  EXPECT_THAT(server_send_data.size(), 131153);
  EXPECT_THAT(server_send_data, HasSubstr("+++++"));
  // We expect filling with \0 bytes.
  EXPECT_EQ(server_send_data.substr(server_send_data.size() - 5, 5), ConstStringView("\0\0\0\0\0"));
}

constexpr std::string_view kHTTPRespMsgHeader =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json; msg1\r\n"
    "Content-Length: 22\r\n"
    "\r\n";

constexpr std::string_view kHTTPRespMsgContent = "Pixie labs is awesome!";

TEST_F(SocketTraceBPFTest, SendFile) {
  // FLAGS_stirling_conn_trace_pid = getpid();

  StartTransferDataThread();

  TCPSocket client;
  TCPSocket server;
  // TODO(yzhao/oazizi): Consider std::condition_variable.
  std::atomic<bool> ready = false;

  std::thread server_thread([&server, &ready]() {
    server.BindAndListen();
    ready = true;
    auto conn = server.Accept();

    std::string data;

    conn->Recv(&data);

    std::unique_ptr<fs::TempFile> tmpf = fs::TempFile::Create();
    std::filesystem::path fpath = tmpf->path();
    ASSERT_OK(WriteFileFromString(fpath, kHTTPRespMsgContent));

    conn->Send(kHTTPRespMsgHeader);
    conn->SendFile(fpath);
  });

  // Wait for server thread to start listening.
  while (!ready) {
  }

  std::thread client_thread([&client, &server]() {
    client.Connect(server);

    std::string data;

    client.Send(kHTTPReqMsg1);
    while (data.size() != kHTTPRespMsgHeader.size() + kHTTPRespMsgContent.size()) {
      client.Recv(&data);
    }
  });

  server_thread.join();
  client_thread.join();

  client.Close();

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(kHTTPTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

  ColumnWrapperRecordBatch records = FindRecordsMatchingPID(record_batch, kHTTPUPIDIdx, getpid());

  ASSERT_THAT(records, RecordBatchSizeIs(2));

  const absl::flat_hash_set<std::string> responses{
      records[kHTTPRespBodyIdx]->Get<types::StringValue>(0),
      records[kHTTPRespBodyIdx]->Get<types::StringValue>(1)};

  const std::string kHTTPRespMsgContentAsFiller(kHTTPRespMsgContent.size(), 0);
  EXPECT_THAT(responses, Contains(kHTTPRespMsgContentAsFiller));
  EXPECT_THAT(responses, Contains(kHTTPRespMsgContent));
}

using NullRemoteAddrTest = testing::SocketTraceBPFTestFixture</* TClientSideTracing */ false>;

// Tests that accept4() with a NULL sock_addr result argument.
TEST_F(NullRemoteAddrTest, Accept4WithNullRemoteAddr) {
  StartTransferDataThread();

  TCPSocket client;
  TCPSocket server;

  std::atomic<bool> server_ready = true;

  std::thread server_thread([&server, &server_ready]() {
    server.BindAndListen();
    server_ready = true;
    auto conn = server.Accept(/* populate_remote_addr */ false);

    std::string data;

    conn->Read(&data);
    conn->Write(kHTTPRespMsg1);
  });

  // Wait for server thread to start listening.
  while (!server_ready) {
  }
  // After server_ready, server.Accept() needs to enter the accepting state, before the client
  // connection can succeed below. We don't have a simple and robust way to signal that from inside
  // the server thread, so we just use sleep to avoid the race condition.
  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::thread client_thread([&client, &server]() {
    client.Connect(server);

    std::string data;

    client.Write(kHTTPReqMsg1);
    client.Read(&data);
  });

  server_thread.join();
  client_thread.join();

  // Get the remote port seen by server from client's local port.
  struct sockaddr_in client_sockaddr = {};
  socklen_t client_sockaddr_len = sizeof(client_sockaddr);
  struct sockaddr* client_sockaddr_ptr = reinterpret_cast<struct sockaddr*>(&client_sockaddr);
  ASSERT_EQ(getsockname(client.sockfd(), client_sockaddr_ptr, &client_sockaddr_len), 0);

  // Close after getting the sockaddr from fd, otherwise getsockname() wont work.
  client.Close();
  server.Close();

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(kHTTPTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

  ColumnWrapperRecordBatch records = FindRecordsMatchingPID(record_batch, kHTTPUPIDIdx, getpid());

  ASSERT_THAT(records, RecordBatchSizeIs(1));

  EXPECT_THAT(std::string(records[kHTTPRespHeadersIdx]->Get<types::StringValue>(0)),
              HasSubstr(R"(Content-Type":"application/json; msg1)"));

  // Make sure that the socket info resolution works.
  ASSERT_OK_AND_ASSIGN(std::string remote_addr, IPv4AddrToString(client_sockaddr.sin_addr));
  EXPECT_EQ(std::string(records[kHTTPRemoteAddrIdx]->Get<types::StringValue>(0)), remote_addr);
  EXPECT_EQ(remote_addr, "127.0.0.1");

  uint16_t port = ntohs(client_sockaddr.sin_port);
  EXPECT_EQ(records[kHTTPRemotePortIdx]->Get<types::Int64Value>(0), port);
}

// Tests that accept4() with a NULL sock_addr result argument (IPv6 version).
TEST_F(NullRemoteAddrTest, IPv6Accept4WithNullRemoteAddr) {
  StartTransferDataThread();

  TCPSocket client(AF_INET6);
  TCPSocket server(AF_INET6);

  std::atomic<bool> server_ready = false;

  std::thread server_thread([&server, &server_ready]() {
    server.BindAndListen();
    server_ready = true;
    auto conn = server.Accept(/* populate_remote_addr */ false);

    std::string data;

    conn->Read(&data);
    conn->Write(kHTTPRespMsg1);
  });

  while (!server_ready) {
  }

  std::thread client_thread([&client, &server]() {
    client.Connect(server);

    std::string data;

    client.Write(kHTTPReqMsg1);
    client.Read(&data);
  });

  server_thread.join();
  client_thread.join();

  // Get the remote port seen by server from client's local port.
  struct sockaddr_in6 client_sockaddr = {};
  socklen_t client_sockaddr_len = sizeof(client_sockaddr);
  struct sockaddr* client_sockaddr_ptr = reinterpret_cast<struct sockaddr*>(&client_sockaddr);
  ASSERT_EQ(getsockname(client.sockfd(), client_sockaddr_ptr, &client_sockaddr_len), 0);

  // Close after getting the sockaddr from fd, otherwise getsockname() wont work.
  client.Close();
  server.Close();

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(kHTTPTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

  ColumnWrapperRecordBatch records = FindRecordsMatchingPID(record_batch, kHTTPUPIDIdx, getpid());
  ASSERT_THAT(records, RecordBatchSizeIs(1));

  EXPECT_THAT(std::string(records[kHTTPRespHeadersIdx]->Get<types::StringValue>(0)),
              HasSubstr(R"(Content-Type":"application/json; msg1)"));

  // Make sure that the socket info resolution works.
  ASSERT_OK_AND_ASSIGN(std::string remote_addr, IPv6AddrToString(client_sockaddr.sin6_addr));
  EXPECT_EQ(std::string(records[kHTTPRemoteAddrIdx]->Get<types::StringValue>(0)), remote_addr);
  EXPECT_EQ(remote_addr, "::1");

  uint16_t port = ntohs(client_sockaddr.sin6_port);
  EXPECT_EQ(records[kHTTPRemotePortIdx]->Get<types::Int64Value>(0), port);
}

// Run a UDP-based client-server system.
class UDPSocketTraceBPFTest : public SocketTraceBPFTest {
 protected:
  void SetUp() override {
    SocketTraceBPFTest::SetUp();
    ConfigureBPFCapture(traffic_protocol_t::kProtocolHTTP, kRoleClient | kRoleServer);
    server_.BindAndListen();

    pid_ = getpid();
    LOG(INFO) << absl::Substitute("PID=$0", pid_);

    // Drain the perf buffers before beginning the test to make sure perf buffers are empty.
    // Otherwise, the test may flake due to events not being received in user-space.
    source_->BCC().PollPerfBuffers();

    // Uncomment to enable tracing:
    // FLAGS_stirling_conn_trace_pid = pid_;
  }

  UDPSocket client_;
  UDPSocket server_;
  int pid_ = 0;
};

TEST_F(UDPSocketTraceBPFTest, UDPSendToRecvFrom) {
  std::string server_recv_data;
  std::string client_recv_data;

  ASSERT_EQ(client_.SendTo(kHTTPReqMsg1, server_.sockaddr()), kHTTPReqMsg1.size());
  struct sockaddr_in server_remote = server_.RecvFrom(&server_recv_data);
  ASSERT_NE(server_remote.sin_addr.s_addr, 0);
  ASSERT_NE(server_remote.sin_port, 0);
  EXPECT_EQ(server_recv_data, kHTTPReqMsg1);

  ASSERT_EQ(server_.SendTo(kHTTPRespMsg1, server_remote), kHTTPRespMsg1.size());
  struct sockaddr_in client_remote = client_.RecvFrom(&client_recv_data);
  ASSERT_EQ(client_remote.sin_addr.s_addr, server_.addr().s_addr);
  ASSERT_EQ(client_remote.sin_port, server_.port());
  EXPECT_EQ(client_recv_data, kHTTPRespMsg1);

  source_->BCC().PollPerfBuffers();

  ASSERT_OK_AND_ASSIGN(auto* tracker, GetMutableConnTracker(pid_, client_.sockfd()));
  EXPECT_EQ(tracker->send_data().data_buffer().Head(), kHTTPReqMsg1);
  EXPECT_EQ(tracker->recv_data().data_buffer().Head(), kHTTPRespMsg1);

  ASSERT_OK_AND_ASSIGN(tracker, GetMutableConnTracker(pid_, server_.sockfd()));
  EXPECT_EQ(tracker->send_data().data_buffer().Head(), kHTTPRespMsg1);
  EXPECT_EQ(tracker->recv_data().data_buffer().Head(), kHTTPReqMsg1);
}

TEST_F(UDPSocketTraceBPFTest, UDPSendMsgRecvMsg) {
  std::string server_recv_data;
  std::string client_recv_data;

  ASSERT_EQ(client_.SendMsg(kHTTPReqMsg1, server_.sockaddr()), kHTTPReqMsg1.size());
  struct sockaddr_in client_sockaddr = server_.RecvMsg(&server_recv_data);
  ASSERT_NE(client_sockaddr.sin_addr.s_addr, 0);
  ASSERT_NE(client_sockaddr.sin_port, 0);
  EXPECT_EQ(server_recv_data, kHTTPReqMsg1);

  ASSERT_EQ(server_.SendMsg(kHTTPRespMsg1, client_sockaddr), kHTTPRespMsg1.size());
  struct sockaddr_in client_remote = client_.RecvMsg(&client_recv_data);
  ASSERT_EQ(client_remote.sin_addr.s_addr, server_.addr().s_addr);
  ASSERT_EQ(client_remote.sin_port, server_.port());
  EXPECT_EQ(client_recv_data, kHTTPRespMsg1);

  source_->BCC().PollPerfBuffers();

  ASSERT_OK_AND_ASSIGN(auto* tracker, GetMutableConnTracker(pid_, client_.sockfd()));
  EXPECT_EQ(tracker->send_data().data_buffer().Head(), kHTTPReqMsg1);
  EXPECT_EQ(tracker->recv_data().data_buffer().Head(), kHTTPRespMsg1);

  ASSERT_OK_AND_ASSIGN(tracker, GetMutableConnTracker(pid_, server_.sockfd()));
  EXPECT_EQ(tracker->send_data().data_buffer().Head(), kHTTPRespMsg1);
  EXPECT_EQ(tracker->recv_data().data_buffer().Head(), kHTTPReqMsg1);
}

TEST_F(UDPSocketTraceBPFTest, UDPSendMMsgRecvMMsg) {
  std::string server_recv_data;
  std::string client_recv_data;

  ASSERT_EQ(client_.SendMMsg(kHTTPReqMsg1, server_.sockaddr()), kHTTPReqMsg1.size());
  struct sockaddr_in server_remote = server_.RecvMMsg(&server_recv_data);
  ASSERT_NE(server_remote.sin_addr.s_addr, 0);
  ASSERT_NE(server_remote.sin_port, 0);
  EXPECT_EQ(server_recv_data, kHTTPReqMsg1);

  ASSERT_EQ(server_.SendMMsg(kHTTPRespMsg1, server_remote), kHTTPRespMsg1.size());
  struct sockaddr_in client_remote = client_.RecvMMsg(&client_recv_data);
  ASSERT_EQ(client_remote.sin_addr.s_addr, server_.addr().s_addr);
  ASSERT_EQ(client_remote.sin_port, server_.port());
  EXPECT_EQ(client_recv_data, kHTTPRespMsg1);

  source_->BCC().PollPerfBuffers();

  ASSERT_OK_AND_ASSIGN(auto* tracker, GetMutableConnTracker(pid_, client_.sockfd()));
  EXPECT_EQ(tracker->send_data().data_buffer().Head(), kHTTPReqMsg1);
  EXPECT_EQ(tracker->recv_data().data_buffer().Head(), kHTTPRespMsg1);

  ASSERT_OK_AND_ASSIGN(tracker, GetMutableConnTracker(pid_, server_.sockfd()));
  EXPECT_EQ(tracker->send_data().data_buffer().Head(), kHTTPRespMsg1);
  EXPECT_EQ(tracker->recv_data().data_buffer().Head(), kHTTPReqMsg1);
}

// A failed non-blocking receive call shouldn't interfere with tracing.
TEST_F(UDPSocketTraceBPFTest, NonBlockingRecv) {
  std::string recv_data;

  // This receive will fail with with EAGAIN, since there's no data to receive.
  struct sockaddr_in failed_recv_remote = client_.RecvFrom(&recv_data, MSG_DONTWAIT);
  ASSERT_EQ(failed_recv_remote.sin_addr.s_addr, 0);
  ASSERT_EQ(failed_recv_remote.sin_port, 0);
  ASSERT_TRUE(recv_data.empty());

  recv_data.clear();
  ASSERT_EQ(client_.SendTo(kHTTPReqMsg1, server_.sockaddr()), kHTTPReqMsg1.size());
  struct sockaddr_in server_remote = server_.RecvFrom(&recv_data);
  ASSERT_NE(server_remote.sin_addr.s_addr, 0);
  ASSERT_NE(server_remote.sin_port, 0);
  EXPECT_EQ(recv_data, kHTTPReqMsg1);

  recv_data.clear();
  ASSERT_EQ(server_.SendTo(kHTTPRespMsg1, server_remote), kHTTPRespMsg1.size());
  struct sockaddr_in client_remote = client_.RecvFrom(&recv_data);
  ASSERT_EQ(client_remote.sin_addr.s_addr, server_.addr().s_addr);
  ASSERT_EQ(client_remote.sin_port, server_.port());
  EXPECT_EQ(recv_data, kHTTPRespMsg1);

  source_->BCC().PollPerfBuffers();

  ASSERT_OK_AND_ASSIGN(auto* tracker, GetMutableConnTracker(pid_, client_.sockfd()));
  EXPECT_EQ(tracker->send_data().data_buffer().Head(), kHTTPReqMsg1);
  EXPECT_EQ(tracker->recv_data().data_buffer().Head(), kHTTPRespMsg1);
  EXPECT_EQ(tracker->remote_endpoint().port(), ntohs(server_.sockaddr().sin_port));
  EXPECT_EQ(tracker->remote_endpoint().AddrStr(), "127.0.0.1");

  ASSERT_OK_AND_ASSIGN(tracker, GetMutableConnTracker(pid_, server_.sockfd()));
  EXPECT_EQ(tracker->send_data().data_buffer().Head(), kHTTPRespMsg1);
  EXPECT_EQ(tracker->recv_data().data_buffer().Head(), kHTTPReqMsg1);
  EXPECT_EQ(tracker->remote_endpoint().port(), ntohs(server_remote.sin_port));
  EXPECT_EQ(tracker->remote_endpoint().AddrStr(), "127.0.0.1");
}

class SocketTraceServerSideBPFTest
    : public testing::SocketTraceBPFTestFixture</* TClientSideTracing */ false> {};

// Tests stats for a disabled ConnTracker.
// Now that ConnStats is tracked independently, these stats are expected to stop
// updating after the tracker is disabled.
TEST_F(SocketTraceServerSideBPFTest, StatsDisabledTracker) {
  using Stat = ConnTracker::StatKey;

  auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(source_.get());

  ConfigureBPFCapture(kProtocolHTTP, kRoleClient | kRoleServer);

  TCPSocket client;
  TCPSocket server;

  server.BindAndListen();
  client.Connect(server);
  auto server_endpoint = server.Accept();

  // First write.
  // BPF should trace the write due to `ConfigureBPFCapture(kProtocolHTTP, kRoleClient |
  // kRoleServer)` above. Then TransferData should disable the client-side tracing in user-space,
  // and propagate the information back to BPF.
  ASSERT_TRUE(client.Write(kHTTPReqMsg1));
  std::string msg;
  ASSERT_TRUE(server_endpoint->Recv(&msg));

  ASSERT_TRUE(server_endpoint->Send(kHTTPRespMsg1));
  ASSERT_TRUE(client.Recv(&msg));

  sleep(1);
  source_->TransferData(ctx_.get());

  ASSERT_OK_AND_ASSIGN(const ConnTracker* client_side_tracker,
                       socket_trace_connector->GetConnTracker(getpid(), client.sockfd()));
  ASSERT_OK_AND_ASSIGN(const ConnTracker* server_side_tracker,
                       socket_trace_connector->GetConnTracker(getpid(), server_endpoint->sockfd()));

  EXPECT_EQ(client_side_tracker->GetStat(Stat::kBytesSent), kHTTPReqMsg1.size());
  EXPECT_EQ(client_side_tracker->GetStat(Stat::kBytesSentTransferred), kHTTPReqMsg1.size());
  EXPECT_EQ(client_side_tracker->GetStat(Stat::kDataEventSent), 1);
  EXPECT_EQ(client_side_tracker->GetStat(Stat::kDataEventRecv), 1);
  // No records are produced because client-side tracing is disabled in user-space.
  EXPECT_EQ(client_side_tracker->GetStat(Stat::kValidRecords), 0);
  EXPECT_EQ(client_side_tracker->GetStat(Stat::kInvalidRecords), 0);

  EXPECT_EQ(server_side_tracker->GetStat(Stat::kBytesRecv), kHTTPReqMsg1.size());
  EXPECT_EQ(server_side_tracker->GetStat(Stat::kBytesRecvTransferred), kHTTPReqMsg1.size());
  EXPECT_EQ(server_side_tracker->GetStat(Stat::kDataEventSent), 1);
  EXPECT_EQ(server_side_tracker->GetStat(Stat::kDataEventRecv), 1);
  EXPECT_EQ(server_side_tracker->GetStat(Stat::kValidRecords), 1);
  EXPECT_EQ(server_side_tracker->GetStat(Stat::kInvalidRecords), 0);

  // Second write.
  // BPF should not even trace the write, because it should have been disabled from user-space.
  ASSERT_TRUE(client.Write(kHTTPReqMsg2));
  ASSERT_TRUE(server_endpoint->Recv(&msg));

  ASSERT_TRUE(server_endpoint->Send(kHTTPRespMsg1));
  ASSERT_TRUE(client.Recv(&msg));
  sleep(1);

  source_->TransferData(ctx_.get());

  EXPECT_EQ(client_side_tracker->GetStat(Stat::kBytesSent), kHTTPReqMsg1.size());
  EXPECT_EQ(client_side_tracker->GetStat(Stat::kBytesSentTransferred), kHTTPReqMsg1.size())
      << "Data transfer was disabled, so the counter should be the same.";
  EXPECT_EQ(client_side_tracker->GetStat(Stat::kDataEventSent), 1);
  EXPECT_EQ(client_side_tracker->GetStat(Stat::kDataEventRecv), 1);
  EXPECT_EQ(client_side_tracker->GetStat(Stat::kValidRecords), 0);
  EXPECT_EQ(client_side_tracker->GetStat(Stat::kInvalidRecords), 0);

  EXPECT_EQ(server_side_tracker->GetStat(Stat::kBytesRecv),
            kHTTPReqMsg1.size() + kHTTPReqMsg2.size());
  EXPECT_EQ(server_side_tracker->GetStat(Stat::kBytesRecvTransferred),
            kHTTPReqMsg1.size() + kHTTPReqMsg2.size());
  EXPECT_EQ(server_side_tracker->GetStat(Stat::kDataEventSent), 2);
  EXPECT_EQ(server_side_tracker->GetStat(Stat::kDataEventRecv), 2);
  EXPECT_EQ(server_side_tracker->GetStat(Stat::kValidRecords), 2);
  EXPECT_EQ(server_side_tracker->GetStat(Stat::kInvalidRecords), 0);
}

}  // namespace stirling
}  // namespace px
