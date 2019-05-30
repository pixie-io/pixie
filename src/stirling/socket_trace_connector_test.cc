#include "src/stirling/socket_trace_connector.h"

#include <gtest/gtest.h>

#include <sys/socket.h>
#include <memory>

#include "src/stirling/bcc_bpf/socket_trace.h"

namespace pl {
namespace stirling {

socket_data_event_t InitEvent(std::string_view msg) {
  socket_data_event_t event;
  event.attr.event_type = kEventTypeSyscallWriteEvent;
  event.attr.conn_info.addr.sin6_family = AF_INET;
  event.attr.conn_info.timestamp_ns = 0;
  event.attr.timestamp_ns = 1000000;
  event.attr.msg_size = msg.size();
  msg.copy(event.msg, msg.size());
  return event;
}

TEST(HandleProbeOutputTest, FilterMessages) {
  const std::string msg1 = R"(HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8
Content-Length: 0

)";
  socket_data_event_t event_json = InitEvent(msg1);

  const std::string msg2 = R"(HTTP/1.1 200 OK
Content-Type: text/plain; charset=utf-8
Content-Length: 0

)";
  socket_data_event_t event_text = InitEvent(msg2);
  const int table_num = 0;

  // FRIEND_TEST() does not grant std::make_unique() access to SocketTraceConnector's private ctor.
  // We choose this style over the SocketTraceConnector::Create() + dynamic_cast<>, as this is
  // clearer.
  std::unique_ptr<SourceConnector> connector = SocketTraceConnector::Create("bcc_http_trace");
  auto* source = dynamic_cast<SocketTraceConnector*>(connector.get());
  types::ColumnWrapperRecordBatch record_batch;
  EXPECT_OK(InitRecordBatch(SocketTraceConnector::kTables[table_num].elements(),
                            /*target_capacity*/ 1, &record_batch));

  event_json.attr.conn_info.seq_num = 0;
  // AcceptEvent() puts data into the internal buffer of SocketTraceConnector. And then
  // TransferData() polls perf buffer, which is no-op because we did not initialize probes, and the
  // data in the internal buffer is being processed and filtered.
  source->AcceptEvent(event_json);
  source->TransferData(table_num, &record_batch);
  for (const auto& column : record_batch) {
    EXPECT_EQ(1, column->Size())
        << "event_json Content-Type does have 'json', and will be selected by the default filter";
  }

  event_text.attr.conn_info.seq_num = 1;
  source->AcceptEvent(event_text);
  source->TransferData(table_num, &record_batch);
  for (const auto& column : record_batch) {
    EXPECT_EQ(1, column->Size())
        << "event_text Content-Type has no 'json', and won't be selected by the default filter";
  }

  SocketTraceConnector::TestOnlySetHTTPResponseHeaderFilter({
      {{"Content-Type", "text/plain"}},
      {{"Content-Encoding", "gzip"}},
  });
  event_text.attr.conn_info.seq_num = 2;
  source->AcceptEvent(event_text);
  source->TransferData(table_num, &record_batch);
  for (const auto& column : record_batch) {
    EXPECT_EQ(2, column->Size())
        << "The filter is changed to require 'text/plain' in Content-Type header, "
           "and event_json Content-Type does not match, and won't be selected";
  }

  SocketTraceConnector::TestOnlySetHTTPResponseHeaderFilter({
      {{"Content-Type", "application/json"}},
      {{"Content-Encoding", "gzip"}},
  });
  event_json.attr.conn_info.seq_num = 3;
  source->AcceptEvent(event_json);
  source->TransferData(table_num, &record_batch);
  for (const auto& column : record_batch) {
    EXPECT_EQ(3, column->Size())
        << "The filter is changed to require 'application/json' in Content-Type header, "
           "and event_json Content-Type matches, and is selected";
  }
}

}  // namespace stirling
}  // namespace pl
