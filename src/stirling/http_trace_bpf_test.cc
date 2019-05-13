#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/types.h>
#include <unistd.h>

#include <thread>

#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/stirling/http_trace_connector.h"
#include "src/stirling/testing/tcp_socket.h"

namespace pl {
namespace stirling {

using ::pl::stirling::testing::TCPSocket;
using ::pl::types::ColumnWrapper;
using ::pl::types::ColumnWrapperRecordBatch;
using ::pl::types::DataType;
using ::testing::SizeIs;

TEST(HTTPTraceBPFTest, TestCapaturedData) {
  const std::string msg1 = R"(HTTP/1.1 200 OK
Content-Type: application/json; msg1

)";

  const std::string msg2 = R"(HTTP/1.1 200 OK
Content-Type: application/json; msg2

)";

  std::unique_ptr<SourceConnector> source(HTTPTraceConnector::Create("bcc_http_trace"));
  EXPECT_OK(source->Init());

  TCPSocket server;
  server.Bind();

  TCPSocket client;
  std::thread client_thread([&server, &client]() {
    client.Connect(server);
    std::string data;
    while (client.Read(&data)) {
    }
  });

  server.Accept();
  EXPECT_EQ(msg1.length(), server.Write(msg1));
  EXPECT_EQ(msg2.length(), server.Write(msg2));

  // TODO(yzhao): The send() syscall somehow is not probed, i.e., with server.Send() the captured
  // data is not showing. Investigate and add test for send().

  server.Close();
  client_thread.join();

  const int table_num = 0;
  types::ColumnWrapperRecordBatch record_batch;
  Status init_status = InitRecordBatch(HTTPTraceConnector::kElements[table_num].elements(),
                                       /*target_capacity*/ 2, &record_batch);
  EXPECT_EQ(0, init_status.code());
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

  // TODO(yzhao): Add close() and reopen sequences to test the behavior across connection establish
  // and close.
}

}  // namespace stirling
}  // namespace pl
