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

  socket_data_event_t InitSendEvent(std::string_view msg, uint64_t ts_ns = 0) {
    socket_data_event_t event = InitEvent(kEventTypeSyscallSendEvent, msg, ts_ns);
    event.attr.seq_num = send_seq_num_;
    send_seq_num_++;
    return event;
  }

  socket_data_event_t InitRecvEvent(std::string_view msg, uint64_t ts_ns = 0) {
    socket_data_event_t event = InitEvent(kEventTypeSyscallRecvEvent, msg, ts_ns);
    event.attr.seq_num = recv_seq_num_;
    recv_seq_num_++;
    return event;
  }

  socket_data_event_t InitEvent(EventType event_type, std::string_view msg, uint64_t ts_ns = 0) {
    socket_data_event_t event = {};
    event.attr.event_type = event_type;
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

  uint64_t send_seq_num_ = 0;
  uint64_t recv_seq_num_ = 0;

  static constexpr int kTableNum = SocketTraceConnector::kHTTPTableNum;
  static constexpr DataTableSchema kHTTPTable = SocketTraceConnector::kHTTPTable;
  static constexpr int kHTTPRespBodyIdx = kHTTPTable.ColIndex("http_resp_body");
  static constexpr int kHTTPReqMethodIdx = kHTTPTable.ColIndex("http_req_method");
  static constexpr int kHTTPReqPathIdx = kHTTPTable.ColIndex("http_req_path");
  static constexpr int kTimeIdx = kHTTPTable.ColIndex("time_");

  std::unique_ptr<SourceConnector> connector_;
  SocketTraceConnector* source_;

  const std::string kReq0 =
      "GET /index.html HTTP/1.1\r\n"
      "Host: www.pixielabs.ai\r\n"
      "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
      "\r\n";

  const std::string kReq1 =
      "GET /data.html HTTP/1.1\r\n"
      "Host: www.pixielabs.ai\r\n"
      "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
      "\r\n";

  const std::string kReq2 =
      "GET /logs.html HTTP/1.1\r\n"
      "Host: www.pixielabs.ai\r\n"
      "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n"
      "\r\n";

  const std::string kJSONResp =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json; charset=utf-8\r\n"
      "Content-Length: 3\r\n"
      "\r\n"
      "foo";

  const std::string kTextResp =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "Content-Length: 3\r\n"
      "\r\n"
      "bar";

  const std::string_view kResp0 =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: json\r\n"
      "Content-Length: 3\r\n"
      "\r\n"
      "foo";

  const std::string_view kResp1 =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: json\r\n"
      "Content-Length: 3\r\n"
      "\r\n"
      "bar";

  std::string_view kResp2 =
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
  socket_data_event_t event0_json = InitRecvEvent(kJSONResp, 100);
  socket_data_event_t event1_text = InitRecvEvent(kTextResp, 200);
  socket_data_event_t event2_text = InitRecvEvent(kTextResp, 200);
  socket_data_event_t event3_json = InitRecvEvent(kJSONResp, 100);

  auto record_batch = GetRecordBatch(SocketTraceConnector::kHTTPTable);

  // Registers a new connection
  source_->OpenConn(conn);

  // AcceptEvent() puts data into the internal buffer of SocketTraceConnector. And then
  // TransferData() polls perf buffer, which is no-op because we did not initialize probes, and the
  // data in the internal buffer is being processed and filtered.
  source_->AcceptEvent(event0_json);
  source_->TransferData(kTableNum, &record_batch);
  for (const auto& column : record_batch) {
    EXPECT_EQ(1, column->Size())
        << "event_json Content-Type does have 'json', and will be selected by the default filter";
  }

  source_->AcceptEvent(event1_text);
  source_->TransferData(kTableNum, &record_batch);
  for (const auto& column : record_batch) {
    EXPECT_EQ(1, column->Size())
        << "event_text Content-Type has no 'json', and won't be selected by the default filter";
  }

  SocketTraceConnector::TestOnlySetHTTPResponseHeaderFilter({
      {{"Content-Type", "text/plain"}},
      {{"Content-Encoding", "gzip"}},
  });
  source_->AcceptEvent(event2_text);
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
  source_->AcceptEvent(event3_json);
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
  socket_data_event_t event0 =
      InitRecvEvent(absl::StrCat(kResp0, kResp1.substr(0, kResp1.length() / 2)));
  socket_data_event_t event1 = InitRecvEvent(kResp1.substr(kResp1.length() / 2));
  socket_data_event_t event2 = InitRecvEvent(kResp2);

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
  socket_data_event_t event0 = InitRecvEvent(kResp0);

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

TEST_F(SocketTraceConnectorTest, RequestResponseMatching) {
  conn_info_t conn = InitConn();
  socket_data_event_t req_event0 = InitSendEvent(kReq0);
  socket_data_event_t req_event1 = InitSendEvent(kReq1);
  socket_data_event_t req_event2 = InitSendEvent(kReq2);
  socket_data_event_t resp_event0 = InitRecvEvent(kResp0);
  socket_data_event_t resp_event1 = InitRecvEvent(kResp1);
  socket_data_event_t resp_event2 = InitRecvEvent(kResp2);

  auto record_batch = GetRecordBatch(SocketTraceConnector::kHTTPTable);

  source_->OpenConn(conn);
  source_->AcceptEvent(req_event0);
  source_->AcceptEvent(req_event1);
  source_->AcceptEvent(req_event2);
  source_->AcceptEvent(resp_event0);
  source_->AcceptEvent(resp_event1);
  source_->AcceptEvent(resp_event2);
  source_->TransferData(kTableNum, &record_batch);
  EXPECT_EQ(3, record_batch[0]->Size());

  EXPECT_THAT(ToStringVector(record_batch[kHTTPRespBodyIdx]), ElementsAre("foo", "bar", "doe"));
  EXPECT_THAT(ToStringVector(record_batch[kHTTPReqMethodIdx]), ElementsAre("GET", "GET", "GET"));
  EXPECT_THAT(ToStringVector(record_batch[kHTTPReqPathIdx]),
              ElementsAre("/index.html", "/data.html", "/logs.html"));
}

}  // namespace stirling
}  // namespace pl
