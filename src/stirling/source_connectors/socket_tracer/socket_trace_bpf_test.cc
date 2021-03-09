#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/types.h>
#include <unistd.h>
#include <string_view>
#include <thread>

#include <magic_enum.hpp>

#include "src/common/system/boot_clock.h"
#include "src/common/system/tcp_socket.h"
#include "src/common/system/udp_socket.h"
#include "src/shared/metadata/metadata.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/stirling/core/data_table.h"
#include "src/stirling/core/output.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/source_connectors/socket_tracer/testing/client_server_system.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace pl {
namespace stirling {

using ::pl::stirling::testing::ColWrapperIsEmpty;
using ::pl::stirling::testing::ColWrapperSizeIs;
using ::pl::stirling::testing::FindRecordIdxMatchesPID;
using ::pl::stirling::testing::FindRecordsMatchingPID;
using ::pl::system::TCPSocket;
using ::pl::types::ColumnWrapper;
using ::pl::types::ColumnWrapperRecordBatch;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

constexpr std::string_view kHTTPReqMsg1 = R"(GET /endpoint1 HTTP/1.1
User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:67.0) Gecko/20100101 Firefox/67.0

)";

constexpr std::string_view kHTTPReqMsg2 = R"(GET /endpoint2 HTTP/1.1
User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:67.0) Gecko/20100101 Firefox/67.0

)";

constexpr std::string_view kHTTPRespMsg1 = R"(HTTP/1.1 200 OK
Content-Type: application/json; msg1
Content-Length: 0

)";

constexpr std::string_view kHTTPRespMsg2 = R"(HTTP/1.1 200 OK
Content-Type: application/json; msg2
Content-Length: 0

)";

constexpr std::string_view kNoProtocolMsg = R"(This is not an HTTP message)";

// TODO(yzhao): We'd better rewrite the test to use BCCWrapper directly, instead of
// SocketTraceConnector, to avoid triggering the userland parsing code, so these tests do not need
// change if we alter output format.

// This test requires docker container with --pid=host so that the container's PID and the
// host machine are identical.

// TODO(yzhao): Apply this pattern to other syscall pairs. An issue is that other syscalls do not
// use scatter buffer. One approach would be to concatenate inner vector to a single string, and
// then feed to the syscall. Another caution is that value-parameterized tests actually discourage
// changing functions being tested according to test parameters. The canonical pattern is using test
// parameters as inputs, but keep the function being tested fixed.
enum class SyscallPair {
  kSendRecv,
  kWriteRead,
  kSendMsgRecvMsg,
  kWritevReadv,
};

struct SocketTraceBPFTestParams {
  SyscallPair syscall_pair;
  uint64_t trace_role;
};

// Returns a list of the messages of the input data events.
std::vector<std::string> GetMsgFromEvents(
    const std::map<size_t, std::unique_ptr<SocketDataEvent>>& events) {
  std::vector<std::string> data;
  data.reserve(events.size());

  for (const auto& [offset, event_uptr] : events) {
    data.push_back(event_uptr->msg);
  }

  return data;
}

class SocketTraceBPFTest : public testing::SocketTraceBPFTest</* TClientSideTracing */ true> {};

class NonVecSyscallTests : public SocketTraceBPFTest,
                           public ::testing::WithParamInterface<SocketTraceBPFTestParams> {};

TEST_P(NonVecSyscallTests, NonVecSyscalls) {
  SocketTraceBPFTestParams p = GetParam();
  LOG(INFO) << absl::Substitute("Test parameters: syscall_pair=$0 trace_role=$1",
                                magic_enum::enum_name(p.syscall_pair), p.trace_role);
  ConfigureBPFCapture(kProtocolHTTP, p.trace_role);

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
    default:
      LOG(FATAL) << absl::Substitute("$0 not supported by this test",
                                     magic_enum::enum_name(p.syscall_pair));
  }

  DataTable data_table(kHTTPTable);
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();
  ASSERT_FALSE(tablets.empty());

  if (p.trace_role & kRoleClient) {
    types::ColumnWrapperRecordBatch records =
        FindRecordsMatchingPID(tablets[0].records, kHTTPUPIDIdx, system.ClientPID());

    ASSERT_THAT(records, Each(ColWrapperSizeIs(2)));

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
    types::ColumnWrapperRecordBatch records =
        FindRecordsMatchingPID(tablets[0].records, kHTTPUPIDIdx, system.ServerPID());

    ASSERT_THAT(records, Each(ColWrapperSizeIs(2)));

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

  // TODO(oazizi): Check IPs.
}

INSTANTIATE_TEST_SUITE_P(
    NonVecSyscalls, NonVecSyscallTests,
    ::testing::Values(SocketTraceBPFTestParams{SyscallPair::kWriteRead, kRoleClient},
                      SocketTraceBPFTestParams{SyscallPair::kWriteRead, kRoleServer},
                      SocketTraceBPFTestParams{SyscallPair::kWriteRead, kRoleClient | kRoleServer},
                      SocketTraceBPFTestParams{SyscallPair::kSendRecv, kRoleClient},
                      SocketTraceBPFTestParams{SyscallPair::kSendRecv, kRoleServer},
                      SocketTraceBPFTestParams{SyscallPair::kSendRecv, kRoleClient | kRoleServer}));

class IOVecSyscallTests : public SocketTraceBPFTest,
                          public ::testing::WithParamInterface<SocketTraceBPFTestParams> {};

TEST_P(IOVecSyscallTests, IOVecSyscalls) {
  SocketTraceBPFTestParams p = GetParam();
  LOG(INFO) << absl::Substitute("$0 $1", magic_enum::enum_name(p.syscall_pair), p.trace_role);
  ConfigureBPFCapture(kProtocolHTTP, p.trace_role);

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

  DataTable data_table(kHTTPTable);
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();
  ASSERT_FALSE(tablets.empty());

  if (p.trace_role & kRoleServer) {
    types::ColumnWrapperRecordBatch records =
        FindRecordsMatchingPID(tablets[0].records, kHTTPUPIDIdx, system.ServerPID());

    ASSERT_THAT(records, Each(ColWrapperSizeIs(2)));

    EXPECT_EQ(200, records[kHTTPRespStatusIdx]->Get<types::Int64Value>(0).val);
    EXPECT_THAT(std::string(records[kHTTPRespBodyIdx]->Get<types::StringValue>(0)), StrEq("a"));
    EXPECT_THAT(std::string(records[kHTTPRespMessageIdx]->Get<types::StringValue>(0)), StrEq("OK"));

    EXPECT_EQ(404, records[kHTTPRespStatusIdx]->Get<types::Int64Value>(1).val);
    EXPECT_THAT(std::string(records[kHTTPRespBodyIdx]->Get<types::StringValue>(1)), StrEq("bc"));
    EXPECT_THAT(std::string(records[kHTTPRespMessageIdx]->Get<types::StringValue>(1)),
                StrEq("Not Found"));
  }

  if (p.trace_role & kRoleClient) {
    types::ColumnWrapperRecordBatch records =
        FindRecordsMatchingPID(tablets[0].records, kHTTPUPIDIdx, system.ClientPID());

    ASSERT_THAT(records, Each(ColWrapperSizeIs(2)));

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

TEST_F(SocketTraceBPFTest, NoProtocolWritesNotCaptured) {
  ConfigureBPFCapture(TrafficProtocol::kProtocolHTTP, kRoleClient | kRoleServer);
  ConfigureBPFCapture(TrafficProtocol::kProtocolMySQL, kRoleClient);

  testing::SendRecvScript script({
      {{kNoProtocolMsg}, {kNoProtocolMsg}},
      {{kNoProtocolMsg}, {kNoProtocolMsg}},
  });

  testing::ClientServerSystem system;
  system.RunClientServer<&TCPSocket::Read, &TCPSocket::Write>(script);

  // Check that no tables captured any data.
  std::vector<DataTableSchema> tables = {kHTTPTable, kMySQLTable, kCQLTable, kPGSQLTable};

  for (const auto& table : tables) {
    DataTable data_table(table);
    source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
    std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();
    if (!tablets.empty()) {
      types::ColumnWrapperRecordBatch& records = tablets[0].records;

      int table_upid_index = table.ColIndex("upid");

      ASSERT_THAT(FindRecordsMatchingPID(records, table_upid_index, system.ClientPID()),
                  Each(ColWrapperIsEmpty()));
      ASSERT_THAT(FindRecordsMatchingPID(records, table_upid_index, system.ServerPID()),
                  Each(ColWrapperIsEmpty()));
    }
  }
}

TEST_F(SocketTraceBPFTest, MultipleConnections) {
  ConfigureBPFCapture(TrafficProtocol::kProtocolHTTP, kRoleClient);

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

  {
    DataTable data_table(kHTTPTable);
    source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
    std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();
    ASSERT_FALSE(tablets.empty());

    {
      types::ColumnWrapperRecordBatch records =
          FindRecordsMatchingPID(tablets[0].records, kHTTPUPIDIdx, system1.ClientPID());

      ASSERT_THAT(records, Each(ColWrapperSizeIs(1)));
      EXPECT_THAT(records[kHTTPRespHeadersIdx]->Get<types::StringValue>(0), HasSubstr("msg1"));
    }

    {
      types::ColumnWrapperRecordBatch records =
          FindRecordsMatchingPID(tablets[0].records, kHTTPUPIDIdx, system2.ClientPID());

      ASSERT_THAT(records, Each(ColWrapperSizeIs(1)));
      EXPECT_THAT(records[kHTTPRespHeadersIdx]->Get<types::StringValue>(0), HasSubstr("msg2"));
    }
  }
}

TEST_F(SocketTraceBPFTest, StartTime) {
  ConfigureBPFCapture(TrafficProtocol::kProtocolHTTP, kRoleClient);

  testing::SendRecvScript script({
      {{kHTTPReqMsg1}, {kHTTPRespMsg1}},
      {{kHTTPReqMsg2}, {kHTTPRespMsg2}},
  });

  testing::ClientServerSystem system;
  system.RunClientServer<&TCPSocket::Recv, &TCPSocket::Send>(script);

  // Kernel uses a special monotonic clock as start_time, so we must do the same.
  auto now = pl::chrono::boot_clock::now();

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

  DataTable data_table(kHTTPTable);
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch records =
      FindRecordsMatchingPID(tablets[0].records, kHTTPUPIDIdx, system.ClientPID());

  ASSERT_THAT(records, Each(ColWrapperSizeIs(2)));

  md::UPID upid0(records[kHTTPUPIDIdx]->Get<types::UInt128Value>(0).val);
  EXPECT_EQ(system.ClientPID(), upid0.pid());
  EXPECT_LT(time_window_start, upid0.start_ts());
  EXPECT_GT(time_window_end, upid0.start_ts());

  md::UPID upid1(records[kHTTPUPIDIdx]->Get<types::UInt128Value>(1).val);
  EXPECT_EQ(system.ClientPID(), upid1.pid());
  EXPECT_LT(time_window_start, upid1.start_ts());
  EXPECT_GT(time_window_end, upid1.start_ts());
}

TEST_F(SocketTraceBPFTest, UDPSendToRecvFrom) {
  using ::pl::system::UDPSocket;

  ConfigureBPFCapture(TrafficProtocol::kProtocolHTTP, kRoleClient);

  // Run a UDP-based client-server system.
  {
    UDPSocket server;
    server.BindAndListen();

    UDPSocket client;
    std::string recv_data;

    ASSERT_EQ(client.SendTo(kHTTPReqMsg1, server.sockaddr()), kHTTPReqMsg1.size());
    struct sockaddr_in server_remote = server.RecvFrom(&recv_data);
    ASSERT_NE(server_remote.sin_addr.s_addr, 0);
    ASSERT_NE(server_remote.sin_port, 0);
    EXPECT_EQ(recv_data, kHTTPReqMsg1);

    ASSERT_EQ(server.SendTo(kHTTPRespMsg1, server_remote), kHTTPRespMsg1.size());
    struct sockaddr_in client_remote = client.RecvFrom(&recv_data);
    ASSERT_EQ(client_remote.sin_addr.s_addr, server.addr().s_addr);
    ASSERT_EQ(client_remote.sin_port, server.port());
    EXPECT_EQ(recv_data, kHTTPRespMsg1);

    client.Close();
    server.Close();
  }

  DataTable data_table(kHTTPTable);
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch records =
      FindRecordsMatchingPID(tablets[0].records, kHTTPUPIDIdx, getpid());

  ASSERT_THAT(records, Each(ColWrapperSizeIs(1)));

  EXPECT_EQ(200, records[kHTTPRespStatusIdx]->Get<types::Int64Value>(0).val);
  EXPECT_THAT(std::string(records[kHTTPRespBodyIdx]->Get<types::StringValue>(0)), StrEq(""));
  EXPECT_THAT(std::string(records[kHTTPRespMessageIdx]->Get<types::StringValue>(0)), StrEq("OK"));
}

TEST_F(SocketTraceBPFTest, UDPSendMsgRecvMsg) {
  using ::pl::system::UDPSocket;

  ConfigureBPFCapture(TrafficProtocol::kProtocolHTTP, kRoleClient | kRoleServer);

  // Run a UDP-based client-server system.
  UDPSocket server;
  server.BindAndListen();

  UDPSocket client;
  std::string recv_data;

  ASSERT_EQ(client.SendMsg(kHTTPReqMsg1, server.sockaddr()), kHTTPReqMsg1.size());
  struct sockaddr_in client_sockaddr = server.RecvMsg(&recv_data);
  ASSERT_NE(client_sockaddr.sin_addr.s_addr, 0);
  ASSERT_NE(client_sockaddr.sin_port, 0);
  EXPECT_EQ(recv_data, kHTTPReqMsg1);

  ASSERT_EQ(server.SendMsg(kHTTPRespMsg1, client_sockaddr), kHTTPRespMsg1.size());
  struct sockaddr_in client_remote = client.RecvMsg(&recv_data);
  ASSERT_EQ(client_remote.sin_addr.s_addr, server.addr().s_addr);
  ASSERT_EQ(client_remote.sin_port, server.port());
  EXPECT_EQ(recv_data, kHTTPRespMsg1);

  source_->PollPerfBuffers();

  ASSERT_OK_AND_ASSIGN(const ConnectionTracker* tracker,
                       source_->GetConnectionTracker(getpid(), client.sockfd()));
  ASSERT_NE(tracker, nullptr);
  EXPECT_THAT(GetMsgFromEvents(tracker->send_data().events()), ElementsAre(kHTTPReqMsg1));
  EXPECT_THAT(GetMsgFromEvents(tracker->recv_data().events()), ElementsAre(kHTTPRespMsg1));

  client.Close();
  server.Close();
}

TEST_F(SocketTraceBPFTest, UDPSendMMsgRecvMMsg) {
  using ::pl::system::UDPSocket;

  ConfigureBPFCapture(TrafficProtocol::kProtocolHTTP, kRoleClient);

  // Run a UDP-based client-server system.
  {
    UDPSocket server;
    server.BindAndListen();

    UDPSocket client;
    std::string recv_data;

    ASSERT_EQ(client.SendMMsg(kHTTPReqMsg1, server.sockaddr()), kHTTPReqMsg1.size());
    struct sockaddr_in server_remote = server.RecvMMsg(&recv_data);
    ASSERT_NE(server_remote.sin_addr.s_addr, 0);
    ASSERT_NE(server_remote.sin_port, 0);
    EXPECT_EQ(recv_data, kHTTPReqMsg1);

    ASSERT_EQ(server.SendMMsg(kHTTPRespMsg1, server_remote), kHTTPRespMsg1.size());
    struct sockaddr_in client_remote = client.RecvMMsg(&recv_data);
    ASSERT_EQ(client_remote.sin_addr.s_addr, server.addr().s_addr);
    ASSERT_EQ(client_remote.sin_port, server.port());
    EXPECT_EQ(recv_data, kHTTPRespMsg1);

    client.Close();
    server.Close();
  }

  DataTable data_table(kHTTPTable);
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch records =
      FindRecordsMatchingPID(tablets[0].records, kHTTPUPIDIdx, getpid());

  ASSERT_THAT(records, Each(ColWrapperSizeIs(1)));

  EXPECT_EQ(200, records[kHTTPRespStatusIdx]->Get<types::Int64Value>(0).val);
  EXPECT_THAT(std::string(records[kHTTPRespBodyIdx]->Get<types::StringValue>(0)), StrEq(""));
  EXPECT_THAT(std::string(records[kHTTPRespMessageIdx]->Get<types::StringValue>(0)), StrEq("OK"));
}

// A failed non-blocking receive call shouldn't interfere with tracing.
TEST_F(SocketTraceBPFTest, NonBlockingRecv) {
  using ::pl::system::UDPSocket;

  ConfigureBPFCapture(TrafficProtocol::kProtocolHTTP, kRoleClient);

  int server_port = 0;

  // Run a UDP-based client-server system.
  {
    UDPSocket server;
    server.BindAndListen();

    UDPSocket client;
    std::string recv_data;

    // This receive will fail with with EAGAIN, since there's no data to receive.
    struct sockaddr_in failed_recv_remote = client.RecvFrom(&recv_data, MSG_DONTWAIT);
    ASSERT_EQ(failed_recv_remote.sin_addr.s_addr, 0);
    ASSERT_EQ(failed_recv_remote.sin_port, 0);
    ASSERT_TRUE(recv_data.empty());

    ASSERT_EQ(client.SendTo(kHTTPReqMsg1, server.sockaddr()), kHTTPReqMsg1.size());
    struct sockaddr_in server_remote = server.RecvFrom(&recv_data);
    ASSERT_NE(server_remote.sin_addr.s_addr, 0);
    ASSERT_NE(server_remote.sin_port, 0);
    EXPECT_EQ(recv_data, kHTTPReqMsg1);

    ASSERT_EQ(server.SendTo(kHTTPRespMsg1, server_remote), kHTTPRespMsg1.size());
    struct sockaddr_in client_remote = client.RecvFrom(&recv_data);
    ASSERT_EQ(client_remote.sin_addr.s_addr, server.addr().s_addr);
    ASSERT_EQ(client_remote.sin_port, server.port());
    EXPECT_EQ(recv_data, kHTTPRespMsg1);

    server_port = htons(server.port());

    client.Close();
    server.Close();
  }

  DataTable data_table(kHTTPTable);
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);
  std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch records =
      FindRecordsMatchingPID(tablets[0].records, kHTTPUPIDIdx, getpid());

  ASSERT_THAT(records, Each(ColWrapperSizeIs(1)));

  int resp_status = records[kHTTPRespStatusIdx]->Get<types::Int64Value>(0).val;
  const std::string& resp_body = records[kHTTPRespBodyIdx]->Get<types::StringValue>(0);
  const std::string& resp_msg = records[kHTTPRespMessageIdx]->Get<types::StringValue>(0);
  const std::string& remote_addr = records[kHTTPRemoteAddrIdx]->Get<types::StringValue>(0);
  const int remote_port = records[kHTTPRemotePortIdx]->Get<types::Int64Value>(0).val;

  EXPECT_EQ(resp_status, 200);
  EXPECT_EQ(resp_body, "");
  EXPECT_EQ(resp_msg, "OK");
  EXPECT_EQ(remote_addr, "127.0.0.1");
  EXPECT_EQ(remote_port, server_port);
}

class SocketTraceServerSideBPFTest
    : public testing::SocketTraceBPFTest</* TClientSideTracing */ false> {};

// Returns the number of records that belong to the input pid.
int GetTargetRecordsCount(DataTable* data_table, int32_t pid) {
  int res = 0;
  std::vector<TaggedRecordBatch> tablets = data_table->ConsumeRecords();
  for (const auto& tablet : tablets) {
    res += FindRecordIdxMatchesPID(tablet.records, kHTTPUPIDIdx, pid).size();
  }
  return res;
}

uint64_t GetConnStats(const ConnectionTracker& tracker, ConnectionTracker::Stats::Key key) {
  return tracker.stats().Get(key);
}

uint64_t GetBytesSentTransferred(const ConnectionTracker& tracker) {
  return GetConnStats(tracker, ConnectionTracker::Stats::Key::kBytesSentTransferred);
}

uint64_t GetBytesRecvTransferred(const ConnectionTracker& tracker) {
  return GetConnStats(tracker, ConnectionTracker::Stats::Key::kBytesRecvTransferred);
}

uint64_t GetBytesSent(const ConnectionTracker& tracker) {
  return GetConnStats(tracker, ConnectionTracker::Stats::Key::kBytesSent);
}

uint64_t GetBytesRecv(const ConnectionTracker& tracker) {
  return GetConnStats(tracker, ConnectionTracker::Stats::Key::kBytesRecv);
}

uint64_t GetDataEventSent(const ConnectionTracker& tracker) {
  return GetConnStats(tracker, ConnectionTracker::Stats::Key::kDataEventSent);
}

uint64_t GetDataEventRecv(const ConnectionTracker& tracker) {
  return GetConnStats(tracker, ConnectionTracker::Stats::Key::kDataEventRecv);
}

uint64_t GetValidRecords(const ConnectionTracker& tracker) {
  return GetConnStats(tracker, ConnectionTracker::Stats::Key::kValidRecords);
}

uint64_t GetInvalidRecords(const ConnectionTracker& tracker) {
  return GetConnStats(tracker, ConnectionTracker::Stats::Key::kInvalidRecords);
}

// Tests that connection stats are updated after ConnectionTracker is disabled because of being
// a client.
TEST_F(SocketTraceServerSideBPFTest, ConnStatsUpdatedAfterConnectionTrackerDisabled) {
  auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(source_.get());

  ConfigureBPFCapture(kProtocolHTTP, kRoleClient | kRoleServer);
  DataTable data_table(kHTTPTable);

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
  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);

  ASSERT_OK_AND_ASSIGN(const ConnectionTracker* client_side_tracker,
                       socket_trace_connector->GetConnectionTracker(getpid(), client.sockfd()));
  ASSERT_OK_AND_ASSIGN(
      const ConnectionTracker* server_side_tracker,
      socket_trace_connector->GetConnectionTracker(getpid(), server_endpoint->sockfd()));

  EXPECT_EQ(GetBytesSent(*client_side_tracker), kHTTPReqMsg1.size());
  EXPECT_EQ(GetBytesSentTransferred(*client_side_tracker), kHTTPReqMsg1.size());
  EXPECT_EQ(GetDataEventSent(*client_side_tracker), 1);
  EXPECT_EQ(GetDataEventRecv(*client_side_tracker), 1);
  // No records are produced because client-side tracing is disabled in user-space.
  EXPECT_EQ(GetValidRecords(*client_side_tracker), 0);
  EXPECT_EQ(GetInvalidRecords(*client_side_tracker), 0);

  EXPECT_EQ(GetBytesRecv(*server_side_tracker), kHTTPReqMsg1.size());
  EXPECT_EQ(GetBytesRecvTransferred(*server_side_tracker), kHTTPReqMsg1.size());
  EXPECT_EQ(GetDataEventSent(*server_side_tracker), 1);
  EXPECT_EQ(GetDataEventRecv(*server_side_tracker), 1);
  EXPECT_EQ(GetValidRecords(*server_side_tracker), 1);
  EXPECT_EQ(GetInvalidRecords(*server_side_tracker), 0);

  // Second write.
  // BPF should not even trace the write, because it should have been disabled from user-space.
  ASSERT_TRUE(client.Write(kHTTPReqMsg2));
  ASSERT_TRUE(server_endpoint->Recv(&msg));

  ASSERT_TRUE(server_endpoint->Send(kHTTPRespMsg1));
  ASSERT_TRUE(client.Recv(&msg));
  sleep(1);

  source_->TransferData(ctx_.get(), kHTTPTableNum, &data_table);

  EXPECT_EQ(GetBytesSent(*client_side_tracker), kHTTPReqMsg1.size() + kHTTPReqMsg2.size())
      << "Data sent were increased.";
  EXPECT_EQ(GetBytesSentTransferred(*client_side_tracker), kHTTPReqMsg1.size())
      << "Data were not transferred to user space, so the counter should be the same.";
  EXPECT_EQ(GetDataEventSent(*client_side_tracker), 2);
  EXPECT_EQ(GetDataEventRecv(*client_side_tracker), 2);
  EXPECT_EQ(GetValidRecords(*client_side_tracker), 0);
  EXPECT_EQ(GetInvalidRecords(*client_side_tracker), 0);

  EXPECT_EQ(GetBytesRecv(*server_side_tracker), kHTTPReqMsg1.size() + kHTTPReqMsg2.size());
  EXPECT_EQ(GetBytesRecvTransferred(*server_side_tracker),
            kHTTPReqMsg1.size() + kHTTPReqMsg2.size());
  EXPECT_EQ(GetDataEventSent(*server_side_tracker), 2);
  EXPECT_EQ(GetDataEventRecv(*server_side_tracker), 2);
  EXPECT_EQ(GetValidRecords(*server_side_tracker), 2);
  EXPECT_EQ(GetInvalidRecords(*server_side_tracker), 0);
}

}  // namespace stirling
}  // namespace pl
