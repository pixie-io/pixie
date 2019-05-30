#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/types.h>
#include <unistd.h>

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
using ::pl::types::DataType;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

class HTTPTraceBPFTest : public ::testing::Test {
 protected:
  HTTPTraceBPFTest() {}

  void SetUp() override {
    source = SocketTraceConnector::Create("bcc_http_trace");
    ASSERT_OK(source->Init());
  }

  void SpawnReaderClient(const TCPSocket& server) {
    client_thread = std::thread([&server]() {
      TCPSocket client;
      client.Connect(server);
      std::string data;
      while (client.Read(&data)) {
      }
    });
  }

  void SpawnWriterClient(const TCPSocket& server, const std::vector<std::string_view>& write_data) {
    client_thread = std::thread([&server, &write_data]() {
      TCPSocket client;
      client.Connect(server);
      for (auto data : write_data) {
        client.Write(data);
      }
      client.Close();
    });
  }

  void JoinClient() { client_thread.join(); }

  static constexpr std::string_view kHTTPMsg1 = R"(HTTP/1.1 200 OK
Content-Type: application/json; msg1
Content-Length: 0

)";

  static constexpr std::string_view kHTTPMsg2 = R"(HTTP/1.1 200 OK
Content-Type: application/json; msg2
Content-Length: 0

)";

  static constexpr std::string_view kNoProtocolMsg = R"(This is not an HTTP message)";

  static constexpr std::string_view kMySQLMsg = "\x16SELECT column FROM table";

  const int http_table_num = 0;
  const DataTableSchema& http_table_schema = SocketTraceConnector::kHTTPTable;
  const uint64_t kHTTPHeaderIdx = http_table_schema.KeyIndex("http_headers");
  const uint64_t kFdIdx = http_table_schema.KeyIndex("fd");

  const int mysql_table_num = 1;
  const DataTableSchema& mysql_table_schema = SocketTraceConnector::kMySQLTable;

  std::unique_ptr<SourceConnector> source;
  std::thread client_thread;
};

TEST_F(HTTPTraceBPFTest, TestWriteCapturedData) {
  TCPSocket server;
  server.Bind();

  SpawnReaderClient(server);

  server.Accept();
  EXPECT_EQ(kHTTPMsg1.length(), server.Write(kHTTPMsg1));
  EXPECT_EQ(kHTTPMsg2.length(), server.Write(kHTTPMsg2));
  server.Close();

  JoinClient();

  {
    types::ColumnWrapperRecordBatch record_batch;
    EXPECT_OK(InitRecordBatch(http_table_schema.elements(), /*target_capacity*/ 2, &record_batch));
    source->TransferData(http_table_num, &record_batch);

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(2, col->Size());
    }

    // These 2 EXPECTs require docker container with --pid=host so that the container's PID and the
    // host machine are identical.
    // See https://stackoverflow.com/questions/33328841/pid-mapping-between-docker-and-host
    EXPECT_EQ(getpid(), record_batch[1]->Get<types::Int64Value>(0).val);
    EXPECT_EQ(getpid(), record_batch[1]->Get<types::Int64Value>(1).val);

    EXPECT_EQ(std::string_view("Content-Length: 0\nContent-Type: application/json; msg1"),
              record_batch[kHTTPHeaderIdx]->Get<types::StringValue>(0));
    EXPECT_EQ(server.sockfd(), record_batch[kFdIdx]->Get<types::Int64Value>(0).val);
    EXPECT_EQ(std::string_view("Content-Length: 0\nContent-Type: application/json; msg2"),
              record_batch[kHTTPHeaderIdx]->Get<types::StringValue>(1));
    EXPECT_EQ(server.sockfd(), record_batch[kFdIdx]->Get<types::Int64Value>(1).val);
  }

  // Check that MySQL table did not capture any data.
  //  {
  //    types::ColumnWrapperRecordBatch record_batch;
  //    EXPECT_OK(InitRecordBatch(mysql_table_schema.elements(), /*target_capacity*/ 2,
  //    &record_batch)); source->TransferData(mysql_table_num, &record_batch);
  //
  //    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
  //      ASSERT_EQ(0, col->Size());
  //    }
  //  }

  EXPECT_OK(source->Stop());
}

TEST_F(HTTPTraceBPFTest, TestSendCapturedData) {
  TCPSocket server;
  server.Bind();

  SpawnReaderClient(server);

  server.Accept();
  EXPECT_EQ(kHTTPMsg1.length(), server.Send(kHTTPMsg1));
  EXPECT_EQ(kHTTPMsg2.length(), server.Send(kHTTPMsg2));
  server.Close();

  JoinClient();

  {
    types::ColumnWrapperRecordBatch record_batch;
    EXPECT_OK(InitRecordBatch(http_table_schema.elements(), /*target_capacity*/ 2, &record_batch));
    source->TransferData(http_table_num, &record_batch);

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(2, col->Size());
    }

    // These 2 EXPECTs require docker container with --pid=host so that the container's PID and the
    // host machine are identical.
    // See https://stackoverflow.com/questions/33328841/pid-mapping-between-docker-and-host
    EXPECT_EQ(getpid(), record_batch[1]->Get<types::Int64Value>(0).val);
    EXPECT_EQ(getpid(), record_batch[1]->Get<types::Int64Value>(1).val);

    EXPECT_EQ(std::string_view("Content-Length: 0\nContent-Type: application/json; msg1"),
              record_batch[kHTTPHeaderIdx]->Get<types::StringValue>(0));
    EXPECT_EQ(server.sockfd(), record_batch[kFdIdx]->Get<types::Int64Value>(0).val);
    EXPECT_EQ(std::string_view("Content-Length: 0\nContent-Type: application/json; msg2"),
              record_batch[kHTTPHeaderIdx]->Get<types::StringValue>(1));
    EXPECT_EQ(server.sockfd(), record_batch[kFdIdx]->Get<types::Int64Value>(1).val);
  }

  // Check that MySQL table did not capture any data.
  //  {
  //    types::ColumnWrapperRecordBatch record_batch;
  //    EXPECT_OK(InitRecordBatch(mysql_table_schema.elements(), /*target_capacity*/ 2,
  //    &record_batch)); source->TransferData(mysql_table_num, &record_batch);
  //
  //    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
  //      ASSERT_EQ(0, col->Size());
  //    }
  //  }

  EXPECT_OK(source->Stop());
}

TEST_F(HTTPTraceBPFTest, DISABLED_TestMySQLWriteCapturedData) {
  TCPSocket server;
  server.Bind();

  SpawnReaderClient(server);

  server.Accept();
  EXPECT_EQ(kMySQLMsg.length(), server.Write(kMySQLMsg));
  EXPECT_EQ(kMySQLMsg.length(), server.Send(kMySQLMsg));
  server.Close();

  JoinClient();

  // Check that HTTP table did not capture any data.
  {
    types::ColumnWrapperRecordBatch record_batch;
    EXPECT_OK(InitRecordBatch(http_table_schema.elements(), /*target_capacity*/ 2, &record_batch));
    source->TransferData(http_table_num, &record_batch);

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(0, col->Size());
    }
  }

  // Check that MySQL table did capture the appropriate data.
  {
    types::ColumnWrapperRecordBatch record_batch;
    EXPECT_OK(InitRecordBatch(mysql_table_schema.elements(), /*target_capacity*/ 2, &record_batch));
    source->TransferData(mysql_table_num, &record_batch);

    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(2, col->Size());
    }

    EXPECT_EQ(std::string_view("\x16SELECT column FROM table"),
              record_batch[8]->Get<types::StringValue>(0));
    EXPECT_EQ(std::string_view("\x16SELECT column FROM table"),
              record_batch[8]->Get<types::StringValue>(1));
  }

  EXPECT_OK(source->Stop());
}

TEST_F(HTTPTraceBPFTest, TestNoProtocolWritesNotCaptured) {
  TCPSocket server;
  server.Bind();

  SpawnReaderClient(server);

  server.Accept();
  EXPECT_EQ(kNoProtocolMsg.length(), server.Write(kNoProtocolMsg));
  EXPECT_EQ(0, server.Write(""));
  EXPECT_EQ(kNoProtocolMsg.length(), server.Send(kNoProtocolMsg));
  EXPECT_EQ(0, server.Send(""));
  server.Close();

  JoinClient();

  // Check that HTTP table did not capture any data.
  {
    types::ColumnWrapperRecordBatch record_batch;
    EXPECT_OK(InitRecordBatch(http_table_schema.elements(), /*target_capacity*/ 2, &record_batch));
    source->TransferData(http_table_num, &record_batch);

    // Should not have captured anything.
    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
      ASSERT_EQ(0, col->Size());
    }
  }

  // Check that MySQL table did not capture any data.
  //  {
  //    types::ColumnWrapperRecordBatch record_batch;
  //    EXPECT_OK(InitRecordBatch(mysql_table_schema.elements(), /*target_capacity*/ 2,
  //    &record_batch)); source->TransferData(mysql_table_num, &record_batch);
  //
  //    // Should not have captured anything.
  //    for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
  //      ASSERT_EQ(0, col->Size());
  //    }
  //  }

  EXPECT_OK(source->Stop());
}

TEST_F(HTTPTraceBPFTest, TestReadCapturedData) {
  TCPSocket server;
  server.Bind();

  std::vector<std::string_view> write_data = {kHTTPMsg1, kHTTPMsg2};
  SpawnWriterClient(server, write_data);

  server.Accept();
  std::string recv_msg;
  std::string recv_tmp;
  while (server.Read(&recv_tmp)) {
    recv_msg.append(recv_tmp);
  }

  // Unlike write/send syscalls, read probes may capture zero, one or multiple messages at a time.
  // For example, a single Read() call after two Write() calls may return the data from both Writes.
  // As a result, we can only only check that the aggregate was received correctly.
  std::string expected_msg;
  expected_msg.append(kHTTPMsg1);
  expected_msg.append(kHTTPMsg2);
  EXPECT_EQ(expected_msg, recv_msg);
  server.Close();

  JoinClient();

  // TODO(oazizi): Check that the probes actually captured the data. Coming in a future diff soon.
}

TEST_F(HTTPTraceBPFTest, TestRecvCapturedData) {
  TCPSocket server;
  server.Bind();

  std::vector<std::string_view> write_data = {kHTTPMsg1, kHTTPMsg2};
  SpawnWriterClient(server, write_data);

  server.Accept();
  std::string recv_msg;
  std::string recv_tmp;
  while (server.Recv(&recv_tmp)) {
    recv_msg.append(recv_tmp);
  }

  // Unlike write/send syscalls, read probes may capture zero, one or multiple messages at a time.
  // For example, a single Recv() call after two Write() calls may return the data from both Writes.
  // As a result, we can only only check that the aggregate was received correctly.
  std::string expected_msg;
  expected_msg.append(kHTTPMsg1);
  expected_msg.append(kHTTPMsg2);
  EXPECT_EQ(expected_msg, recv_msg);
  server.Close();

  JoinClient();

  // TODO(oazizi): Check that the probes actually captured the data. Coming in a future diff soon.
}

TEST_F(HTTPTraceBPFTest, TestNonHTTPReadsNotCaptured) {
  TCPSocket server;
  server.Bind();

  std::vector<std::string_view> write_data = {kNoProtocolMsg};
  SpawnWriterClient(server, write_data);

  server.Accept();
  std::string recv_msg;
  std::string recv_tmp;
  while (server.Read(&recv_tmp)) {
    recv_msg.append(recv_tmp);
  }
  EXPECT_EQ(kNoProtocolMsg, recv_msg);
  server.Close();

  JoinClient();

  // TODO(oazizi): Check that the probes did not capture the data. Coming in a future diff soon.
}

TEST_F(HTTPTraceBPFTest, TestConnectionCloseAndGenerationNumberAreInSync) {
  {
    TCPSocket server;
    server.Bind();
    SpawnReaderClient(server);
    server.Accept();
    EXPECT_EQ(kHTTPMsg1.length(), server.Write(kHTTPMsg1));
    server.Close();
    client_thread.join();
  }
  {
    // A new connection.
    TCPSocket server;
    server.Bind();
    SpawnReaderClient(server);
    server.Accept();
    EXPECT_EQ(kHTTPMsg2.length(), server.Write(kHTTPMsg2));
    server.Close();
    client_thread.join();
  }

  auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(source.get());
  ASSERT_NE(nullptr, socket_trace_connector);
  socket_trace_connector->ReadPerfBuffer(http_table_num);
  EXPECT_OK(source->Stop());

  // TODO(yzhao): Write a matcher for Stream.
  ASSERT_THAT(socket_trace_connector->TestOnlyHTTPStreams(), SizeIs(2));

  auto get_message = [](const socket_data_event_t& event) -> std::string_view {
    return std::string_view(event.msg, std::min<uint32_t>(event.attr.msg_size, MAX_MSG_SIZE));
  };
  std::vector<std::pair<uint64_t, std::string_view>> seq_msgs;
  for (const auto& [id, http_stream] : socket_trace_connector->TestOnlyHTTPStreams()) {
    PL_UNUSED(id);
    for (const auto& [seq_num, event] : http_stream.events) {
      seq_msgs.emplace_back(seq_num, get_message(event));
    }
  }
  EXPECT_THAT(seq_msgs, UnorderedElementsAre(Pair(0, kHTTPMsg1), Pair(0, kHTTPMsg2)));
}

}  // namespace stirling
}  // namespace pl
