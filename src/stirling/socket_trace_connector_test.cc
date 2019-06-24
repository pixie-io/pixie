#include "src/stirling/socket_trace_connector.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <sys/socket.h>
#include <memory>

#include "src/stirling/bcc_bpf/socket_trace.h"

namespace pl {
namespace stirling {

using ::testing::ElementsAre;
using RecordBatch = types::ColumnWrapperRecordBatch;

class SocketTraceConnectorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create and configure the connector.
    connector_ = SocketTraceConnector::Create("socket_trace_connector");
    source_ = dynamic_cast<SocketTraceConnector*>(connector_.get());
    ASSERT_NE(nullptr, source_);
    source_->TestOnlyConfigure(kProtocolHTTP, kSocketTraceSendReq | kSocketTraceRecvResp);
  }

  conn_info_t InitConn() {
    conn_info_t conn_info{};
    conn_info.addr.sin6_family = AF_INET;
    conn_info.timestamp_ns = 0;
    conn_info.tgid = 12345;
    conn_info.conn_id = 2;
    conn_info.fd = 3;
    conn_info.traffic_class.protocol = kProtocolHTTP;
    conn_info.traffic_class.message_type = kMessageTypeResponses;
    return conn_info;
  }

  socket_data_event_t InitEvent(std::string_view msg, uint64_t ts_ns = 0) {
    socket_data_event_t event = {};
    event.attr.event_type = kEventTypeSyscallRecvEvent;
    event.attr.protocol = kProtocolHTTP;
    event.attr.timestamp_ns = ts_ns;
    event.attr.tgid = 12345;
    event.attr.conn_id = 2;
    event.attr.msg_size = msg.size();
    msg.copy(event.msg, msg.size());
    return event;
  }

  types::ColumnWrapperRecordBatch GetRecordBatch(DataTableSchema schema) {
    types::ColumnWrapperRecordBatch record_batch;
    InitRecordBatch(schema.elements(), /*target_capacity*/ 1, &record_batch);
    return record_batch;
  }

  static constexpr int kTableNum = SocketTraceConnector::kHTTPTableNum;
  static constexpr int kHTTPRespBodyIdx =
      SocketTraceConnector::kHTTPTable.ColIndex("http_resp_body");
  static constexpr int kTimeIdx = SocketTraceConnector::kHTTPTable.ColIndex("time_");

  std::unique_ptr<SourceConnector> connector_;
  SocketTraceConnector* source_;

  const std::string kJSONMsg =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json; charset=utf-8\r\n"
      "Content-Length: 3\r\n"
      "\r\n"
      "foo";

  const std::string kTextMsg =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "Content-Length: 3\r\n"
      "\r\n"
      "bar";

  const std::string_view kMsg0 =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: json\r\n"
      "Content-Length: 3\r\n"
      "\r\n"
      "foo"
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: json\r\n";

  const std::string_view kMsg1 =
      "Content-Length: 3\r\n"
      "\r\n"
      "bar";

  std::string_view kMsg2 =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: json\r\n"
      "Content-Length: 3\r\n"
      "\r\n"
      "doe";
};

auto ToStringVector(const types::SharedColumnWrapper& col) {
  std::vector<std::string> result;
  for (size_t i = 0; i < col->Size(); ++i) {
    result.push_back(col->Get<types::StringValue>(i));
  }
  return result;
}

template <class TValueType>
auto ToIntVector(const types::SharedColumnWrapper& col) {
  std::vector<int64_t> result;
  for (size_t i = 0; i < col->Size(); ++i) {
    result.push_back(col->Get<TValueType>(i).val);
  }
  return result;
}

TEST_F(SocketTraceConnectorTest, FilterMessages) {
  conn_info_t conn = InitConn();
  socket_data_event_t event_json = InitEvent(kJSONMsg, 100);
  socket_data_event_t event_text = InitEvent(kTextMsg, 200);

  auto record_batch = GetRecordBatch(SocketTraceConnector::kHTTPTable);

  // Registers a new connection
  source_->OpenConn(conn);

  event_json.attr.seq_num = 0;
  // AcceptEvent() puts data into the internal buffer of SocketTraceConnector. And then
  // TransferData() polls perf buffer, which is no-op because we did not initialize probes, and the
  // data in the internal buffer is being processed and filtered.
  source_->AcceptEvent(event_json);
  source_->TransferData(kTableNum, &record_batch);
  for (const auto& column : record_batch) {
    EXPECT_EQ(1, column->Size())
        << "event_json Content-Type does have 'json', and will be selected by the default filter";
  }

  event_text.attr.seq_num = 1;
  source_->AcceptEvent(event_text);
  source_->TransferData(kTableNum, &record_batch);
  for (const auto& column : record_batch) {
    EXPECT_EQ(1, column->Size())
        << "event_text Content-Type has no 'json', and won't be selected by the default filter";
  }

  SocketTraceConnector::TestOnlySetHTTPResponseHeaderFilter({
      {{"Content-Type", "text/plain"}},
      {{"Content-Encoding", "gzip"}},
  });
  event_text.attr.seq_num = 2;
  source_->AcceptEvent(event_text);
  source_->TransferData(kTableNum, &record_batch);
  for (const auto& column : record_batch) {
    EXPECT_EQ(2, column->Size())
        << "The filter is changed to require 'text/plain' in Content-Type header, "
           "and event_json Content-Type does not match, and won't be selected";
  }

  SocketTraceConnector::TestOnlySetHTTPResponseHeaderFilter({
      {{"Content-Type", "application/json"}},
      {{"Content-Encoding", "gzip"}},
  });
  event_json.attr.seq_num = 3;
  source_->AcceptEvent(event_json);
  source_->TransferData(kTableNum, &record_batch);
  for (const auto& column : record_batch) {
    EXPECT_EQ(3, column->Size())
        << "The filter is changed to require 'application/json' in Content-Type header, "
           "and event_json Content-Type matches, and is selected";
  }
  EXPECT_THAT(ToStringVector(record_batch[kHTTPRespBodyIdx]), ElementsAre("foo", "bar", "foo"));
  EXPECT_THAT(ToIntVector<types::Time64NSValue>(record_batch[kTimeIdx]),
              ElementsAre(100, 200, 100));
}

TEST_F(SocketTraceConnectorTest, AppendNonContiguousEvents) {
  conn_info_t conn = InitConn();
  socket_data_event_t event0 = InitEvent(kMsg0);
  event0.attr.seq_num = 0;
  socket_data_event_t event1 = InitEvent(kMsg1);
  event1.attr.seq_num = 1;
  socket_data_event_t event2 = InitEvent(kMsg2);
  event2.attr.seq_num = 2;

  auto record_batch = GetRecordBatch(SocketTraceConnector::kHTTPTable);

  source_->OpenConn(conn);
  source_->AcceptEvent(event0);
  source_->AcceptEvent(event2);
  source_->TransferData(kTableNum, &record_batch);
  EXPECT_EQ(1, record_batch[0]->Size());

  source_->AcceptEvent(event1);
  source_->TransferData(kTableNum, &record_batch);
  EXPECT_EQ(3, record_batch[0]->Size()) << "Get 3 events after getting the missing one.";
}

TEST_F(SocketTraceConnectorTest, NoEvents) {
  conn_info_t conn = InitConn();
  socket_data_event_t event0 = InitEvent(kMsg0);

  auto record_batch = GetRecordBatch(SocketTraceConnector::kHTTPTable);

  source_->OpenConn(conn);

  // Check empty transfer.
  source_->TransferData(kTableNum, &record_batch);
  EXPECT_EQ(0, record_batch[0]->Size());

  // Check empty transfer following a successful transfer.
  source_->AcceptEvent(event0);
  source_->TransferData(kTableNum, &record_batch);
  EXPECT_EQ(1, record_batch[0]->Size());
  source_->TransferData(kTableNum, &record_batch);
  EXPECT_EQ(1, record_batch[0]->Size());
}

}  // namespace stirling
}  // namespace pl
