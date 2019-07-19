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
  }

  void TearDown() override { ASSERT_OK(source_->Stop()); }

  class ClientServerSystem {
   public:
    ClientServerSystem() { server_.Bind(); }

    void RunWriterReader(const std::vector<std::string_view>& write_data) {
      SpawnReaderClient();
      SpawnWriterServer(write_data);
      JoinThreads();
    }

    void RunSenderReceiver(const std::vector<std::string_view>& write_data) {
      SpawnReceiverClient();
      SpawnSenderServer(write_data);
      JoinThreads();
    }

    void RunSendMsgerReader(const std::vector<std::vector<std::string_view>>& write_data) {
      SpawnReaderClient();
      SpawnSendMsgerServer(write_data);
      JoinThreads();
    }

    void SpawnReaderClient() {
      client_thread_ = std::thread([this]() {
        client_.Connect(server_);
        std::string data;
        while (client_.Read(&data)) {
        }
        client_.Close();
      });
    }

    void SpawnReceiverClient() {
      client_thread_ = std::thread([this]() {
        client_.Connect(server_);
        std::string data;
        while (client_.Recv(&data)) {
        }
        client_.Close();
      });
    }

    void SpawnWriterServer(const std::vector<std::string_view>& write_data) {
      server_thread_ = std::thread([this, write_data]() {
        server_.Accept();
        for (auto data : write_data) {
          ASSERT_EQ(data.length(), server_.Write(data));
        }
        server_.Close();
      });
    }

    void SpawnSenderServer(const std::vector<std::string_view>& write_data) {
      server_thread_ = std::thread([this, write_data]() {
        server_.Accept();
        for (auto data : write_data) {
          ASSERT_EQ(data.length(), server_.Send(data));
        }
        server_.Close();
      });
    }

    void SpawnSendMsgerServer(const std::vector<std::vector<std::string_view>>& write_data) {
      server_thread_ = std::thread([this, write_data]() {
        server_.Accept();
        for (const auto& data : write_data) {
          server_.SendMsg(data);
        }
        server_.Close();
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
  ConfigureCapture(kProtocolHTTP, kSocketTraceSendResp);

  ClientServerSystem system;
  system.RunWriterReader({kHTTPRespMsg1, kHTTPRespMsg2});

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
  ConfigureCapture(TrafficProtocol::kProtocolHTTP, kSocketTraceSendResp);

  ClientServerSystem system;
  system.RunSenderReceiver({kHTTPRespMsg1, kHTTPRespMsg2});

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
  ConfigureCapture(TrafficProtocol::kProtocolHTTP, kSocketTraceRecvResp);

  ClientServerSystem system;
  system.RunWriterReader({kHTTPRespMsg1, kHTTPRespMsg2});

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
  ConfigureCapture(TrafficProtocol::kProtocolHTTP, kSocketTraceRecvResp);

  ClientServerSystem system;
  system.RunSenderReceiver({kHTTPRespMsg1, kHTTPRespMsg2});

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
  system.RunSenderReceiver({kMySQLMsg, kMySQLMsg});

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
  ConfigureCapture(TrafficProtocol::kProtocolHTTP, kSocketTraceSendReq | kSocketTraceRecvReq);
  ConfigureCapture(TrafficProtocol::kProtocolHTTP, kSocketTraceRecvResp | kSocketTraceSendResp);
  ConfigureCapture(TrafficProtocol::kProtocolMySQL, kSocketTraceSendReq | kSocketTraceRecvResp);

  ClientServerSystem system;
  system.RunWriterReader({kNoProtocolMsg, "", kNoProtocolMsg, ""});

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
  ConfigureCapture(TrafficProtocol::kProtocolHTTP, kSocketTraceRecvResp);

  // Two separate connections.
  ClientServerSystem system1;
  system1.RunWriterReader({kHTTPRespMsg1});

  ClientServerSystem system2;
  system2.RunWriterReader({kHTTPRespMsg2});

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
  ConfigureCapture(TrafficProtocol::kProtocolHTTP, kSocketTraceRecvResp);

  ClientServerSystem system;
  system.RunSenderReceiver({kHTTPRespMsg1, kHTTPRespMsg2});

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

TEST_F(SocketTraceBPFTest, SendMsgIsCapatured) {
  ConfigureCapture(kProtocolHTTP, kSocketTraceSendReq | kSocketTraceSendResp);
  ClientServerSystem system;
  system.RunSendMsgerReader(
      {{"HTTP/1.1 200 OK\r\n", "Content-Type: json\r\n", "Content-Length: 1\r\n\r\na"},
       {"HTTP/1.1 404 Not Found\r\n", "Content-Type: json\r\n", "Content-Length: 2\r\n\r\nbc"}});
  types::ColumnWrapperRecordBatch record_batch;
  InitRecordBatch(kHTTPTable.elements(), /*target_capacity*/ 4, &record_batch);
  source_->TransferData(kHTTPTableNum, &record_batch);
  for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
    ASSERT_EQ(2, col->Size());
  }
  EXPECT_THAT(std::string(record_batch[kHTTPHeaderIdx]->Get<types::StringValue>(0)),
              StrEq("Content-Length: 1\nContent-Type: json"));
  EXPECT_EQ(200, record_batch[kHTTPRespStatusIdx]->Get<types::Int64Value>(0).val);
  EXPECT_THAT(std::string(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(0)), StrEq("a"));
  EXPECT_THAT(std::string(record_batch[kHTTPRespMessageIdx]->Get<types::StringValue>(0)),
              StrEq("OK"));

  EXPECT_THAT(std::string(record_batch[kHTTPHeaderIdx]->Get<types::StringValue>(1)),
              StrEq("Content-Length: 2\nContent-Type: json"));
  EXPECT_EQ(404, record_batch[kHTTPRespStatusIdx]->Get<types::Int64Value>(1).val);
  EXPECT_THAT(std::string(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(1)), StrEq("bc"));
  EXPECT_THAT(std::string(record_batch[kHTTPRespMessageIdx]->Get<types::StringValue>(1)),
              StrEq("Not Found"));
}

}  // namespace stirling
}  // namespace pl
