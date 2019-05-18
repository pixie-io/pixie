#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/types.h>
#include <unistd.h>

#include <string_view>
#include <thread>

#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/stirling/bcc_bpf/socket_trace.h"
#include "src/stirling/http_trace_connector.h"
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
    source = HTTPTraceConnector::Create("bcc_http_trace");
    ASSERT_OK(source->Init());
  }

  void SpawnClient(const TCPSocket& server) {
    client_thread = std::thread([&server]() {
      TCPSocket client;
      client.Connect(server);
      std::string data;
      while (client.Read(&data)) {
      }
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

  SpawnClient(server);

  server.Accept();
  EXPECT_EQ(kMsg1.length(), server.Write(kMsg1));
  EXPECT_EQ(kMsg2.length(), server.Write(kMsg2));
  server.Close();

  JoinClient();

  const int table_num = 0;
  types::ColumnWrapperRecordBatch record_batch;
  EXPECT_OK(InitRecordBatch(HTTPTraceConnector::kElements[table_num].elements(),
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
            record_batch[HTTPTraceConnector::kHTTPHeaders]->Get<types::StringValue>(0));
  EXPECT_EQ(std::string_view("Content-Type: application/json; msg2"),
            record_batch[HTTPTraceConnector::kHTTPHeaders]->Get<types::StringValue>(1));
}

TEST_F(HTTPTraceBPFTest, TestSendCapturedData) {
  TCPSocket server;
  server.Bind();

  SpawnClient(server);

  server.Accept();
  EXPECT_EQ(kMsg1.length(), server.Send(kMsg1));
  EXPECT_EQ(kMsg2.length(), server.Send(kMsg2));
  server.Close();

  JoinClient();

  const int table_num = 0;
  types::ColumnWrapperRecordBatch record_batch;
  EXPECT_OK(InitRecordBatch(HTTPTraceConnector::kElements[table_num].elements(),
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
            record_batch[HTTPTraceConnector::kHTTPHeaders]->Get<types::StringValue>(0));
  EXPECT_EQ(std::string_view("Content-Type: application/json; msg2"),
            record_batch[HTTPTraceConnector::kHTTPHeaders]->Get<types::StringValue>(1));
}

TEST_F(HTTPTraceBPFTest, TestNonHTTPWritesNotCaptured) {
  TCPSocket server;
  server.Bind();

  SpawnClient(server);

  server.Accept();
  EXPECT_EQ(kMsg3.length(), server.Write(kMsg3));
  EXPECT_EQ(0, server.Write(""));
  EXPECT_EQ(kMsg3.length(), server.Send(kMsg3));
  EXPECT_EQ(0, server.Send(""));
  server.Close();

  JoinClient();

  const int table_num = 0;
  types::ColumnWrapperRecordBatch record_batch;
  EXPECT_OK(InitRecordBatch(HTTPTraceConnector::kElements[table_num].elements(),
                            /*target_capacity*/ 2, &record_batch));
  source->TransferData(table_num, &record_batch);
  EXPECT_OK(source->Stop());

  // Should not have captured anything.
  for (const std::shared_ptr<ColumnWrapper>& col : record_batch) {
    ASSERT_EQ(0, col->Size());
  }
}

TEST_F(HTTPTraceBPFTest, TestConnectionCloseAndGenerationNumberAreInSync) {
  {
    TCPSocket server;
    server.Bind();
    SpawnClient(server);
    server.Accept();
    EXPECT_EQ(kMsg1.length(), server.Write(kMsg1));
    server.Close();
    client_thread.join();
  }
  {
    // A new connection.
    TCPSocket server;
    server.Bind();
    SpawnClient(server);
    server.Accept();
    EXPECT_EQ(kMsg2.length(), server.Write(kMsg2));
    server.Close();
    client_thread.join();
  }

  const int table_num = 0;  // HTTP Table
  auto* http_trace_connector = dynamic_cast<HTTPTraceConnector*>(source.get());
  ASSERT_NE(nullptr, http_trace_connector);
  http_trace_connector->PollPerfBuffer(table_num);
  EXPECT_OK(source->Stop());

  ASSERT_THAT(http_trace_connector->TestOnlyGetWriteStreamMap(),
              UnorderedElementsAre(Pair(_, SizeIs(1)), Pair(_, SizeIs(1))));

  auto get_message = [](const socket_data_event_t& event) -> std::string_view {
    return std::string_view(event.msg, std::min(event.attr.msg_bytes, event.attr.msg_buf_size));
  };
  std::vector<std::pair<uint64_t, std::string_view>> seq_msgs;
  for (const auto& stream : http_trace_connector->TestOnlyGetWriteStreamMap()) {
    for (const auto& seq_event : stream.second) {
      seq_msgs.push_back(std::make_pair(seq_event.first, get_message(seq_event.second)));
    }
  }
  EXPECT_THAT(seq_msgs, UnorderedElementsAre(Pair(0, kMsg1), Pair(0, kMsg2)));
}

}  // namespace stirling
}  // namespace pl
