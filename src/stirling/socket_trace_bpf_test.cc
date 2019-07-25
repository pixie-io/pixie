#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>
#include <string_view>
#include <thread>

#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/stirling/bcc_bpf/socket_trace.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/testing/tcp_socket.h"

namespace pl {
namespace stirling {

using ::pl::stirling::testing::TCPSocket;
using ::pl::types::ColumnWrapper;
using ::pl::types::ColumnWrapperRecordBatch;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

class SocketTraceBPFTest : public ::testing::Test {
 protected:
  void SetUp() override {
    source_ = SocketTraceConnector::Create("socket_trace_connector");
    ASSERT_OK(source_->Init());
    TestOnlySetTargetPID(getpid());
  }

  void TearDown() override { ASSERT_OK(source_->Stop()); }

  class ClientServerSystem {
   public:
    ClientServerSystem() { server_.Bind(); }

    void RunWriteRead(const std::vector<std::string_view>& write_data) {
      SpawnReadClient();
      SpawnWriteServer(write_data);
      JoinThreads();
    }

    void RunSendRecv(const std::vector<std::string_view>& write_data) {
      SpawnRecvClient();
      SpawnSendServer(write_data);
      JoinThreads();
    }

    void RunSendMsgRecvMsg(const std::vector<std::vector<std::string_view>>& write_data) {
      SpawnRecvMsgClient();
      SpawnSendMsgServer(write_data);
      JoinThreads();
    }

    void RunWriteVReadV(const std::vector<std::vector<std::string_view>>& write_data) {
      SpawnReadVClient();
      SpawnWriteVServer(write_data);
      JoinThreads();
    }

    void SpawnReadClient() {
      client_thread_ = std::thread([this]() {
        client_.Connect(server_);
        std::string data;
        while (client_.Read(&data)) {
        }
        client_.Close();
      });
    }

    void SpawnRecvClient() {
      client_thread_ = std::thread([this]() {
        client_.Connect(server_);
        std::string data;
        while (client_.Recv(&data)) {
        }
        client_.Close();
      });
    }

    void SpawnWriteServer(const std::vector<std::string_view>& write_data) {
      server_thread_ = std::thread([this, write_data]() {
        server_.Accept();
        for (auto data : write_data) {
          ASSERT_EQ(data.length(), server_.Write(data));
        }
        server_.Close();
      });
    }

    void SpawnSendServer(const std::vector<std::string_view>& write_data) {
      server_thread_ = std::thread([this, write_data]() {
        server_.Accept();
        for (auto data : write_data) {
          ASSERT_EQ(data.length(), server_.Send(data));
        }
        server_.Close();
      });
    }

    void SpawnSendMsgServer(const std::vector<std::vector<std::string_view>>& write_data) {
      server_thread_ = std::thread([this, write_data]() {
        server_.Accept();
        for (const auto& data : write_data) {
          server_.SendMsg(data);
        }
        server_.Close();
      });
    }

    void SpawnRecvMsgClient() {
      client_thread_ = std::thread([this]() {
        client_.Connect(server_);
        std::vector<std::string> msgs;
        while (client_.RecvMsg(&msgs) > 0) {
        }
        client_.Close();
      });
    }

    void SpawnWriteVServer(const std::vector<std::vector<std::string_view>>& write_data) {
      server_thread_ = std::thread([this, write_data]() {
        server_.Accept();
        for (const auto& data : write_data) {
          server_.WriteV(data);
        }
        server_.Close();
      });
    }

    void SpawnReadVClient() {
      client_thread_ = std::thread([this]() {
        client_.Connect(server_);
        std::string msg;
        while (client_.ReadV(&msg) > 0) {
        }
        client_.Close();
      });
    }

    void JoinThreads() {
      server_thread_.join();
      client_thread_.join();
    }

    TCPSocket& Server() { return server_; }
    TCPSocket& Client() { return client_; }

   private:
    TCPSocket client_;
    TCPSocket server_;

    std::thread client_thread_;
    std::thread server_thread_;
  };

  void ConfigureCapture(TrafficProtocol protocol, uint64_t mask) {
    auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(source_.get());
    ASSERT_OK(socket_trace_connector->Configure(protocol, mask));
  }

  void TestOnlySetTargetPID(int64_t pid) {
    auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(source_.get());
    ASSERT_OK(socket_trace_connector->TestOnlySetTargetPID(pid));
  }

  static constexpr std::string_view kHTTPReqMsg1 = R"(GET /endpoint1 HTTP/1.1
User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:67.0) Gecko/20100101 Firefox/67.0

)";

  static constexpr std::string_view kHTTPReqMsg2 = R"(GET /endpoint2 HTTP/1.1
User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:67.0) Gecko/20100101 Firefox/67.0

)";

  static constexpr std::string_view kHTTPRespMsg1 = R"(HTTP/1.1 200 OK
Content-Type: application/json; msg1
Content-Length: 0

)";

  static constexpr std::string_view kHTTPRespMsg2 = R"(HTTP/1.1 200 OK
Content-Type: application/json; msg2
Content-Length: 0

)";

  static constexpr std::string_view kNoProtocolMsg = R"(This is not an HTTP message)";

  static constexpr std::string_view kMySQLMsg = "\x16SELECT column FROM table";

  static constexpr int kHTTPTableNum = SocketTraceConnector::kHTTPTableNum;
  static constexpr DataTableSchema kHTTPTable = SocketTraceConnector::kHTTPTable;
  static constexpr uint32_t kHTTPMajorVersionIdx = kHTTPTable.ColIndex("http_major_version");
  static constexpr uint32_t kHTTPContentTypeIdx = kHTTPTable.ColIndex("http_content_type");
  static constexpr uint32_t kHTTPRespStatusIdx = kHTTPTable.ColIndex("http_resp_status");
  static constexpr uint32_t kHTTPRespBodyIdx = kHTTPTable.ColIndex("http_resp_body");
  static constexpr uint32_t kHTTPRespMessageIdx = kHTTPTable.ColIndex("http_resp_message");
  static constexpr uint32_t kHTTPHeaderIdx = kHTTPTable.ColIndex("http_headers");
  static constexpr uint32_t kHTTPPIDIdx = kHTTPTable.ColIndex("pid");
  static constexpr uint32_t kHTTPRemoteAddrIdx = kHTTPTable.ColIndex("remote_addr");
  static constexpr uint32_t kHTTPStartTimeIdx = kHTTPTable.ColIndex("pid_start_time");

  static constexpr int kMySQLTableNum = SocketTraceConnector::kMySQLTableNum;
  static constexpr DataTableSchema kMySQLTable = SocketTraceConnector::kMySQLTable;
  static constexpr uint32_t kMySQLBodyIdx = kMySQLTable.ColIndex("body");

  std::unique_ptr<SourceConnector> source_;
};

TEST_F(SocketTraceBPFTest, TestWriteRespCapture) {
  ConfigureCapture(kProtocolHTTP, kRoleResponder);

  ClientServerSystem system;
  system.RunWriteRead({kHTTPRespMsg1, kHTTPRespMsg2});

  {
    types::ColumnWrapperRecordBatch record_batch;
    InitRecordBatch(kHTTPTable.elements(), /*target_capacity*/ 4, &record_batch);
    source_->TransferData(kHTTPTableNum, &record_batch);

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(2, col->Size());
    }

    // These getpid() EXPECTs require docker container with --pid=host so that the container's PID
    // and the host machine are identical. See
    // https://stackoverflow.com/questions/33328841/pid-mapping-between-docker-and-host

    EXPECT_EQ(getpid(), record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(0).val);
    EXPECT_EQ(std::string_view("Content-Length: 0\nContent-Type: application/json; msg1"),
              record_batch[kHTTPHeaderIdx]->Get<types::StringValue>(0));
    EXPECT_EQ("127.0.0.1", record_batch[kHTTPRemoteAddrIdx]->Get<types::StringValue>(0));

    EXPECT_EQ(getpid(), record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(1).val);
    EXPECT_EQ(std::string_view("Content-Length: 0\nContent-Type: application/json; msg2"),
              record_batch[kHTTPHeaderIdx]->Get<types::StringValue>(1));
    EXPECT_EQ("127.0.0.1", record_batch[kHTTPRemoteAddrIdx]->Get<types::StringValue>(1));

    // Additional verifications. These are common to all HTTP1.x tracing, so we decide to not
    // duplicate them on all relevant tests.
    EXPECT_EQ(1, record_batch[kHTTPMajorVersionIdx]->Get<types::Int64Value>(0).val);
    EXPECT_EQ(static_cast<uint64_t>(HTTPContentType::kJSON),
              record_batch[kHTTPContentTypeIdx]->Get<types::Int64Value>(0).val);
    EXPECT_EQ(1, record_batch[kHTTPMajorVersionIdx]->Get<types::Int64Value>(1).val);
    EXPECT_EQ(static_cast<uint64_t>(HTTPContentType::kJSON),
              record_batch[kHTTPContentTypeIdx]->Get<types::Int64Value>(1).val);
  }

  // Check that MySQL table did not capture any data.
  {
    types::ColumnWrapperRecordBatch record_batch;
    InitRecordBatch(kMySQLTable.elements(), /*target_capacity*/ 2, &record_batch);
    source_->TransferData(kMySQLTableNum, &record_batch);

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(0, col->Size());
    }
  }
}

TEST_F(SocketTraceBPFTest, TestSendRespCapture) {
  ConfigureCapture(TrafficProtocol::kProtocolHTTP, kRoleResponder);

  ClientServerSystem system;
  system.RunSendRecv({kHTTPRespMsg1, kHTTPRespMsg2});

  {
    types::ColumnWrapperRecordBatch record_batch;
    InitRecordBatch(kHTTPTable.elements(), /*target_capacity*/ 2, &record_batch);
    source_->TransferData(kHTTPTableNum, &record_batch);

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(2, col->Size());
    }

    // These 2 EXPECTs require docker container with --pid=host so that the container's PID and the
    // host machine are identical.
    // See https://stackoverflow.com/questions/33328841/pid-mapping-between-docker-and-host

    EXPECT_EQ(getpid(), record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(0).val);
    EXPECT_EQ(std::string_view("Content-Length: 0\nContent-Type: application/json; msg1"),
              record_batch[kHTTPHeaderIdx]->Get<types::StringValue>(0));

    EXPECT_EQ(getpid(), record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(1).val);
    EXPECT_EQ(std::string_view("Content-Length: 0\nContent-Type: application/json; msg2"),
              record_batch[kHTTPHeaderIdx]->Get<types::StringValue>(1));
  }

  // Check that MySQL table did not capture any data.
  {
    types::ColumnWrapperRecordBatch record_batch;
    InitRecordBatch(kMySQLTable.elements(), /*target_capacity*/ 2, &record_batch);
    source_->TransferData(kMySQLTableNum, &record_batch);

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(0, col->Size());
    }
  }
}

TEST_F(SocketTraceBPFTest, TestReadRespCapture) {
  ConfigureCapture(TrafficProtocol::kProtocolHTTP, kRoleRequestor);

  ClientServerSystem system;
  system.RunWriteRead({kHTTPRespMsg1, kHTTPRespMsg2});

  {
    types::ColumnWrapperRecordBatch record_batch;
    InitRecordBatch(kHTTPTable.elements(), /*target_capacity*/ 4, &record_batch);
    source_->TransferData(kHTTPTableNum, &record_batch);

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(2, col->Size());
    }

    // These 2 EXPECTs require docker container with --pid=host so that the container's PID and the
    // host machine are identical.
    // See https://stackoverflow.com/questions/33328841/pid-mapping-between-docker-and-host

    EXPECT_EQ(getpid(), record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(0).val);
    EXPECT_EQ(std::string_view("Content-Length: 0\nContent-Type: application/json; msg1"),
              record_batch[kHTTPHeaderIdx]->Get<types::StringValue>(0));

    EXPECT_EQ(getpid(), record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(1).val);
    EXPECT_EQ(std::string_view("Content-Length: 0\nContent-Type: application/json; msg2"),
              record_batch[kHTTPHeaderIdx]->Get<types::StringValue>(1));
  }

  // Check that MySQL table did not capture any data.
  {
    types::ColumnWrapperRecordBatch record_batch;
    InitRecordBatch(kMySQLTable.elements(), /*target_capacity*/ 2, &record_batch);
    source_->TransferData(kMySQLTableNum, &record_batch);

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(0, col->Size());
    }
  }
}

TEST_F(SocketTraceBPFTest, TestRecvRespCapture) {
  ConfigureCapture(TrafficProtocol::kProtocolHTTP, kRoleRequestor);

  ClientServerSystem system;
  system.RunSendRecv({kHTTPRespMsg1, kHTTPRespMsg2});

  {
    types::ColumnWrapperRecordBatch record_batch;
    InitRecordBatch(kHTTPTable.elements(), /*target_capacity*/ 4, &record_batch);
    source_->TransferData(kHTTPTableNum, &record_batch);

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(2, col->Size());
    }

    // These 2 EXPECTs require docker container with --pid=host so that the container's PID and the
    // host machine are identical.
    // See https://stackoverflow.com/questions/33328841/pid-mapping-between-docker-and-host

    EXPECT_EQ(getpid(), record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(0).val);
    EXPECT_EQ(std::string_view("Content-Length: 0\nContent-Type: application/json; msg1"),
              record_batch[kHTTPHeaderIdx]->Get<types::StringValue>(0));

    EXPECT_EQ(getpid(), record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(1).val);
    EXPECT_EQ(std::string_view("Content-Length: 0\nContent-Type: application/json; msg2"),
              record_batch[kHTTPHeaderIdx]->Get<types::StringValue>(1));
  }

  // Check that MySQL table did not capture any data.
  {
    types::ColumnWrapperRecordBatch record_batch;
    InitRecordBatch(kMySQLTable.elements(), /*target_capacity*/ 2, &record_batch);
    source_->TransferData(kMySQLTableNum, &record_batch);

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(0, col->Size());
    }
  }
}

TEST_F(SocketTraceBPFTest, TestMySQLWriteCapture) {
  ClientServerSystem system;
  system.RunSendRecv({kMySQLMsg, kMySQLMsg});

  // Check that HTTP table did not capture any data.
  {
    types::ColumnWrapperRecordBatch record_batch;
    InitRecordBatch(kHTTPTable.elements(), /*target_capacity*/ 2, &record_batch);
    source_->TransferData(kHTTPTableNum, &record_batch);

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(0, col->Size());
    }
  }

  // Check that MySQL table did capture the appropriate data.
  {
    types::ColumnWrapperRecordBatch record_batch;
    InitRecordBatch(kMySQLTable.elements(), /*target_capacity*/ 2, &record_batch);
    source_->TransferData(kMySQLTableNum, &record_batch);

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(2, col->Size());
    }

    EXPECT_EQ(std::string_view("\x16SELECT column FROM table"),
              record_batch[kMySQLBodyIdx]->Get<types::StringValue>(0));
    EXPECT_EQ(std::string_view("\x16SELECT column FROM table"),
              record_batch[kMySQLBodyIdx]->Get<types::StringValue>(1));
  }
}

TEST_F(SocketTraceBPFTest, TestNoProtocolWritesNotCaptured) {
  ConfigureCapture(TrafficProtocol::kProtocolHTTP, kRoleRequestor | kRoleResponder);
  ConfigureCapture(TrafficProtocol::kProtocolMySQL, kRoleRequestor);

  ClientServerSystem system;
  system.RunWriteRead({kNoProtocolMsg, "", kNoProtocolMsg, ""});

  // Check that HTTP table did not capture any data.
  {
    types::ColumnWrapperRecordBatch record_batch;
    InitRecordBatch(kHTTPTable.elements(), /*target_capacity*/ 2, &record_batch);
    source_->TransferData(kHTTPTableNum, &record_batch);

    // Should not have captured anything.
    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(0, col->Size());
    }
  }

  // Check that MySQL table did not capture any data.
  {
    types::ColumnWrapperRecordBatch record_batch;
    InitRecordBatch(kMySQLTable.elements(), /*target_capacity*/ 2, &record_batch);
    source_->TransferData(kMySQLTableNum, &record_batch);

    // Should not have captured anything.
    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(0, col->Size());
    }
  }
}

TEST_F(SocketTraceBPFTest, TestMultipleConnections) {
  ConfigureCapture(TrafficProtocol::kProtocolHTTP, kRoleRequestor);

  // Two separate connections.
  ClientServerSystem system1;
  system1.RunWriteRead({kHTTPRespMsg1});

  ClientServerSystem system2;
  system2.RunWriteRead({kHTTPRespMsg2});

  {
    types::ColumnWrapperRecordBatch record_batch;
    InitRecordBatch(kHTTPTable.elements(), /*target_capacity*/ 4, &record_batch);
    source_->TransferData(kHTTPTableNum, &record_batch);

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(2, col->Size());
    }

    std::vector<std::tuple<int64_t, std::string>> results;
    for (int i = 0; i < 2; ++i) {
      results.emplace_back(
          std::make_tuple(record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(i).val,
                          record_batch[kHTTPHeaderIdx]->Get<types::StringValue>(i)));
    }

    EXPECT_THAT(
        results,
        UnorderedElementsAre(
            std::make_tuple(getpid(), "Content-Length: 0\nContent-Type: application/json; msg1"),
            std::make_tuple(getpid(), "Content-Length: 0\nContent-Type: application/json; msg2")));
  }
}

TEST_F(SocketTraceBPFTest, TestStartTime) {
  ConfigureCapture(TrafficProtocol::kProtocolHTTP, kRoleRequestor);

  ClientServerSystem system;
  system.RunSendRecv({kHTTPRespMsg1, kHTTPRespMsg2});

  // Kernel uses monotonic clock as start_time, so we must do the same.
  auto now = std::chrono::steady_clock::now();

  // Use a time window to make sure the recorded PID start_time is right.
  // Being super generous with the window, just in case test runs slow.
  auto time_window_start = now - std::chrono::minutes(30);
  auto time_window_end = now + std::chrono::minutes(5);

  types::ColumnWrapperRecordBatch record_batch;
  InitRecordBatch(kHTTPTable.elements(), /*target_capacity*/ 4, &record_batch);
  source_->TransferData(kHTTPTableNum, &record_batch);

  ASSERT_EQ(2, record_batch[0]->Size());

  EXPECT_EQ(getpid(), record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(0).val);
  EXPECT_LT(time_window_start.time_since_epoch().count(),
            record_batch[kHTTPStartTimeIdx]->Get<types::Int64Value>(0).val);
  EXPECT_GT(time_window_end.time_since_epoch().count(),
            record_batch[kHTTPStartTimeIdx]->Get<types::Int64Value>(0).val);

  EXPECT_EQ(getpid(), record_batch[kHTTPPIDIdx]->Get<types::Int64Value>(1).val);
  EXPECT_LT(time_window_start.time_since_epoch().count(),
            record_batch[kHTTPStartTimeIdx]->Get<types::Int64Value>(1).val);
  EXPECT_GT(time_window_end.time_since_epoch().count(),
            record_batch[kHTTPStartTimeIdx]->Get<types::Int64Value>(1).val);
}

// TODO(yzhao): Apply this pattern to other syscall pairs. An issue is that other syscalls do not
// use scatter buffer. One approach would be to concatenate inner vector to a single string, and
// then feed to the syscall. Another caution is that value-parameterized tests actually discourage
// changing functions being tested according to test parameters. The canonical pattern is using test
// parameters as inputs, but keep the function being tested fixed.
enum class SyscallPair {
  kSendRecvMsg,
  kWriteReadv,
};

class SyscallPairBPFTest : public SocketTraceBPFTest,
                           public ::testing::WithParamInterface<SyscallPair> {};

TEST_P(SyscallPairBPFTest, EventsAreCaptured) {
  ConfigureCapture(kProtocolHTTP, kRoleRequestor | kRoleResponder);
  ClientServerSystem system;
  const std::vector<std::vector<std::string_view>> data = {
      {"HTTP/1.1 200 OK\r\n", "Content-Type: json\r\n", "Content-Length: 1\r\n\r\na"},
      {"HTTP/1.1 404 Not Found\r\n", "Content-Type: json\r\n", "Content-Length: 2\r\n\r\nbc"}};
  switch (GetParam()) {
    case SyscallPair::kSendRecvMsg:
      system.RunSendMsgRecvMsg(data);
      break;
    case SyscallPair::kWriteReadv:
      system.RunWriteVReadV(data);
      break;
  }
  types::ColumnWrapperRecordBatch record_batch;
  InitRecordBatch(kHTTPTable.elements(), /*target_capacity*/ 4, &record_batch);
  source_->TransferData(kHTTPTableNum, &record_batch);
  for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
    // 2 for sendmsg() and 2 for recvmsg().
    ASSERT_EQ(4, col->Size());
  }
  for (int i : {0, 2}) {
    EXPECT_THAT(std::string(record_batch[kHTTPHeaderIdx]->Get<types::StringValue>(i)),
                StrEq("Content-Length: 1\nContent-Type: json"));
    EXPECT_EQ(200, record_batch[kHTTPRespStatusIdx]->Get<types::Int64Value>(i).val);
    EXPECT_THAT(std::string(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(i)),
                StrEq("a"));
    EXPECT_THAT(std::string(record_batch[kHTTPRespMessageIdx]->Get<types::StringValue>(i)),
                StrEq("OK"));
  }
  for (int i : {1, 3}) {
    EXPECT_THAT(std::string(record_batch[kHTTPHeaderIdx]->Get<types::StringValue>(i)),
                StrEq("Content-Length: 2\nContent-Type: json"));
    EXPECT_EQ(404, record_batch[kHTTPRespStatusIdx]->Get<types::Int64Value>(i).val);
    EXPECT_THAT(std::string(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(i)),
                StrEq("bc"));
    EXPECT_THAT(std::string(record_batch[kHTTPRespMessageIdx]->Get<types::StringValue>(i)),
                StrEq("Not Found"));
  }
}

INSTANTIATE_TEST_CASE_P(IOVecSyscalls, SyscallPairBPFTest,
                        ::testing::Values(SyscallPair::kSendRecvMsg, SyscallPair::kWriteReadv));

}  // namespace stirling
}  // namespace pl
