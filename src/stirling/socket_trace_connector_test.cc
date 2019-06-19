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

constexpr int kHTTPRespBodyIdx = SocketTraceConnector::kHTTPTable.ColIndex("http_resp_body");
constexpr int kTimeIdx = SocketTraceConnector::kHTTPTable.ColIndex("time_");

auto HTTPRespBodys(const RecordBatch& batch) {
  std::vector<std::string> result;
  const auto& col = batch[kHTTPRespBodyIdx];
  for (size_t i = 0; i < col->Size(); ++i) {
    result.push_back(col->Get<types::StringValue>(i));
  }
  return result;
}

auto Times(const RecordBatch& batch) {
  std::vector<uint64_t> result;
  const auto& col = batch[kTimeIdx];
  for (size_t i = 0; i < col->Size(); ++i) {
    result.push_back(col->Get<types::Time64NSValue>(i).val);
  }
  return result;
}

socket_data_event_t InitEvent(std::string_view msg, uint64_t ts_ns = 0) {
  socket_data_event_t event = {};
  event.attr.event_type = kEventTypeSyscallRecvEvent;
  event.attr.conn_info.addr.sin6_family = AF_INET;
  event.attr.conn_info.timestamp_ns = 0;
  event.attr.conn_info.traffic_class.protocol = kProtocolHTTP;
  event.attr.conn_info.traffic_class.message_type = kMessageTypeResponses;
  event.attr.timestamp_ns = ts_ns;
  event.attr.msg_size = msg.size();
  msg.copy(event.msg, msg.size());
  return event;
}

TEST(HandleProbeOutputTest, FilterMessages) {
  const std::string msg1 =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json; charset=utf-8\r\n"
      "Content-Length: 3\r\n"
      "\r\n"
      "foo";
  socket_data_event_t event_json = InitEvent(msg1, 100);

  const std::string msg2 =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain; charset=utf-8\r\n"
      "Content-Length: 3\r\n"
      "\r\n"
      "bar";
  socket_data_event_t event_text = InitEvent(msg2, 200);
  constexpr int kTableNum = SocketTraceConnector::kHTTPTableNum;

  // FRIEND_TEST() does not grant std::make_unique() access to SocketTraceConnector's private ctor.
  // We choose this style over the SocketTraceConnector::Create() + dynamic_cast<>, as this is
  // clearer.
  std::unique_ptr<SourceConnector> connector =
      SocketTraceConnector::Create("socket_trace_connector");
  auto* source = dynamic_cast<SocketTraceConnector*>(connector.get());
  source->TestOnlyConfigure(kProtocolHTTP, kSocketTraceSendReq | kSocketTraceRecvResp);
  types::ColumnWrapperRecordBatch record_batch;
  EXPECT_OK(InitRecordBatch(SocketTraceConnector::kHTTPTable.elements(),
                            /*target_capacity*/ 1, &record_batch));

  event_json.attr.seq_num = 0;
  // AcceptEvent() puts data into the internal buffer of SocketTraceConnector. And then
  // TransferData() polls perf buffer, which is no-op because we did not initialize probes, and the
  // data in the internal buffer is being processed and filtered.
  source->AcceptEvent(event_json);
  source->TransferData(kTableNum, &record_batch);
  for (const auto& column : record_batch) {
    EXPECT_EQ(1, column->Size())
        << "event_json Content-Type does have 'json', and will be selected by the default filter";
  }

  event_text.attr.seq_num = 1;
  source->AcceptEvent(event_text);
  source->TransferData(kTableNum, &record_batch);
  for (const auto& column : record_batch) {
    EXPECT_EQ(1, column->Size())
        << "event_text Content-Type has no 'json', and won't be selected by the default filter";
  }

  SocketTraceConnector::TestOnlySetHTTPResponseHeaderFilter({
      {{"Content-Type", "text/plain"}},
      {{"Content-Encoding", "gzip"}},
  });
  event_text.attr.seq_num = 2;
  source->AcceptEvent(event_text);
  source->TransferData(kTableNum, &record_batch);
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
  source->AcceptEvent(event_json);
  source->TransferData(kTableNum, &record_batch);
  for (const auto& column : record_batch) {
    EXPECT_EQ(3, column->Size())
        << "The filter is changed to require 'application/json' in Content-Type header, "
           "and event_json Content-Type matches, and is selected";
  }
  EXPECT_THAT(HTTPRespBodys(record_batch), ElementsAre("foo", "bar", "foo"));
  EXPECT_THAT(Times(record_batch), ElementsAre(100, 200, 100));
}

TEST(SocketTraceConnectorTest, AppendNonContiguousEvents) {
  std::string_view msg0 =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: json\r\n"
      "Content-Length: 3\r\n"
      "\r\n"
      "foo"
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: json\r\n";
  std::string_view msg1 =
      "Content-Length: 3\r\n"
      "\r\n"
      "bar";
  std::string_view msg2 =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: json\r\n"
      "Content-Length: 3\r\n"
      "\r\n"
      "doe";
  socket_data_event_t event0 = InitEvent(msg0);
  event0.attr.seq_num = 0;
  socket_data_event_t event1 = InitEvent(msg1);
  event1.attr.seq_num = 1;
  socket_data_event_t event2 = InitEvent(msg2);
  event2.attr.seq_num = 2;

  constexpr int kTableNum = SocketTraceConnector::kHTTPTableNum;

  std::unique_ptr<SourceConnector> connector =
      SocketTraceConnector::Create("socket_trace_connector");
  auto* source = dynamic_cast<SocketTraceConnector*>(connector.get());
  source->TestOnlyConfigure(kProtocolHTTP, kSocketTraceSendReq | kSocketTraceRecvResp);
  ASSERT_NE(nullptr, source);

  types::ColumnWrapperRecordBatch record_batch;
  EXPECT_OK(InitRecordBatch(SocketTraceConnector::kHTTPTable.elements(),
                            /*target_capacity*/ 1, &record_batch));

  source->AcceptEvent(event0);
  source->AcceptEvent(event2);
  source->TransferData(kTableNum, &record_batch);
  EXPECT_EQ(1, record_batch[0]->Size());

  source->AcceptEvent(event1);
  source->TransferData(kTableNum, &record_batch);
  EXPECT_EQ(3, record_batch[0]->Size()) << "Get 3 events after getting the missing one.";
}

}  // namespace stirling
}  // namespace pl
