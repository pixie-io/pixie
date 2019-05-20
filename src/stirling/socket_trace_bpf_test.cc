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

  static constexpr std::string_view kMsg1 = R"(HTTP/1.1 200 OK
Content-Type: application/json; msg1

)";

  static constexpr std::string_view kMsg2 = R"(HTTP/1.1 200 OK
Content-Type: application/json; msg2

)";

  static constexpr std::string_view kMsg3 = R"(This is not an HTTP message)";

  std::unique_ptr<SourceConnector> source;
  std::thread client_thread;
};

TEST_F(HTTPTraceBPFTest, TestWriteCapturedData) {
  TCPSocket server;
  server.Bind();

  SpawnReaderClient(server);

  server.Accept();
  EXPECT_EQ(kMsg1.length(), server.Write(kMsg1));
  EXPECT_EQ(kMsg2.length(), server.Write(kMsg2));
  server.Close();

  JoinClient();

  const int table_num = 0;
  types::ColumnWrapperRecordBatch record_batch;
  EXPECT_OK(InitRecordBatch(SocketTraceConnector::kElements[table_num].elements(),
                            /*target_capacity*/ 2, &record_batch));
  source->TransferData(table_num, &record_batch);
  EXPECT_OK(source->Stop());

  for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
    ASSERT_EQ(2, col->Size());
  }

  // These 2 EXPECTs require docker container with --pid=host so that the container's PID and the
  // host machine are identical.
  // See https://stackoverflow.com/questions/33328841/pid-mapping-between-docker-and-host
  EXPECT_EQ(getpid(), record_batch[1]->Get<types::Int64Value>(0).val);
  EXPECT_EQ(getpid(), record_batch[1]->Get<types::Int64Value>(1).val);

  EXPECT_EQ(std::string_view("Content-Type: application/json; msg1"),
            record_batch[SocketTraceConnector::kHTTPHeaders]->Get<types::StringValue>(0));
  EXPECT_EQ(std::string_view("Content-Type: application/json; msg2"),
            record_batch[SocketTraceConnector::kHTTPHeaders]->Get<types::StringValue>(1));
}

TEST_F(HTTPTraceBPFTest, TestSendCapturedData) {
  TCPSocket server;
  server.Bind();

  SpawnReaderClient(server);

  server.Accept();
  EXPECT_EQ(kMsg1.length(), server.Send(kMsg1));
  EXPECT_EQ(kMsg2.length(), server.Send(kMsg2));
  server.Close();

  JoinClient();

  const int table_num = 0;
  types::ColumnWrapperRecordBatch record_batch;
  EXPECT_OK(InitRecordBatch(SocketTraceConnector::kElements[table_num].elements(),
                            /*target_capacity*/ 2, &record_batch));
  source->TransferData(table_num, &record_batch);
  EXPECT_OK(source->Stop());

  for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
    ASSERT_EQ(2, col->Size());
  }

  // These 2 EXPECTs require docker container with --pid=host so that the container's PID and the
  // host machine are identical.
  // See https://stackoverflow.com/questions/33328841/pid-mapping-between-docker-and-host
  EXPECT_EQ(getpid(), record_batch[1]->Get<types::Int64Value>(0).val);
  EXPECT_EQ(getpid(), record_batch[1]->Get<types::Int64Value>(1).val);

  EXPECT_EQ(std::string_view("Content-Type: application/json; msg1"),
            record_batch[SocketTraceConnector::kHTTPHeaders]->Get<types::StringValue>(0));
  EXPECT_EQ(std::string_view("Content-Type: application/json; msg2"),
            record_batch[SocketTraceConnector::kHTTPHeaders]->Get<types::StringValue>(1));
}

TEST_F(HTTPTraceBPFTest, TestNonHTTPWritesNotCaptured) {
  TCPSocket server;
  server.Bind();

  SpawnReaderClient(server);

  server.Accept();
  EXPECT_EQ(kMsg3.length(), server.Write(kMsg3));
  EXPECT_EQ(0, server.Write(""));
  EXPECT_EQ(kMsg3.length(), server.Send(kMsg3));
  EXPECT_EQ(0, server.Send(""));
  server.Close();

  JoinClient();

  const int table_num = 0;
  types::ColumnWrapperRecordBatch record_batch;
  EXPECT_OK(InitRecordBatch(SocketTraceConnector::kElements[table_num].elements(),
                            /*target_capacity*/ 2, &record_batch));
  source->TransferData(table_num, &record_batch);
  EXPECT_OK(source->Stop());

  // Should not have captured anything.
  for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
    ASSERT_EQ(0, col->Size());
  }
}

TEST_F(HTTPTraceBPFTest, TestReadCapturedData) {
  TCPSocket server;
  server.Bind();

  std::vector<std::string_view> write_data = {kMsg1, kMsg2};
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
  expected_msg.append(kMsg1);
  expected_msg.append(kMsg2);
  EXPECT_EQ(expected_msg, recv_msg);
  server.Close();

  JoinClient();

  // TODO(oazizi): Check that the probes actually captured the data. Coming in a future diff soon.
}

TEST_F(HTTPTraceBPFTest, TestRecvCapturedData) {
  TCPSocket server;
  server.Bind();

  std::vector<std::string_view> write_data = {kMsg1, kMsg2};
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
  expected_msg.append(kMsg1);
  expected_msg.append(kMsg2);
  EXPECT_EQ(expected_msg, recv_msg);
  server.Close();

  JoinClient();

  // TODO(oazizi): Check that the probes actually captured the data. Coming in a future diff soon.
}

TEST_F(HTTPTraceBPFTest, TestNonHTTPReadsNotCaptured) {
  TCPSocket server;
  server.Bind();

  std::vector<std::string_view> write_data = {kMsg3};
  SpawnWriterClient(server, write_data);

  server.Accept();
  std::string recv_msg;
  std::string recv_tmp;
  while (server.Read(&recv_tmp)) {
    recv_msg.append(recv_tmp);
  }
  EXPECT_EQ(kMsg3, recv_msg);
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
    EXPECT_EQ(kMsg1.length(), server.Write(kMsg1));
    server.Close();
    client_thread.join();
  }
  {
    // A new connection.
    TCPSocket server;
    server.Bind();
    SpawnReaderClient(server);
    server.Accept();
    EXPECT_EQ(kMsg2.length(), server.Write(kMsg2));
    server.Close();
    client_thread.join();
  }

  const int table_num = 0;  // HTTP Table
  auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(source.get());
  ASSERT_NE(nullptr, socket_trace_connector);
  socket_trace_connector->PollPerfBuffer(table_num);
  EXPECT_OK(source->Stop());

  ASSERT_THAT(socket_trace_connector->TestOnlyGetWriteStreamMap(),
              UnorderedElementsAre(Pair(_, SizeIs(1)), Pair(_, SizeIs(1))));

  auto get_message = [](const socket_data_event_t& event) -> std::string_view {
    return std::string_view(event.msg, std::min(event.attr.msg_bytes, event.attr.msg_buf_size));
  };
  std::vector<std::pair<uint64_t, std::string_view>> seq_msgs;
  for (const auto& stream : socket_trace_connector->TestOnlyGetWriteStreamMap()) {
    for (const auto& seq_event : stream.second) {
      seq_msgs.push_back(std::make_pair(seq_event.first, get_message(seq_event.second)));
    }
  }
  EXPECT_THAT(seq_msgs, UnorderedElementsAre(Pair(0, kMsg1), Pair(0, kMsg2)));
}

}  // namespace stirling
}  // namespace pl
