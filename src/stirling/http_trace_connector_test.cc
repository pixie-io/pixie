#include "src/stirling/http_trace_connector.h"

#include <gtest/gtest.h>

#include <sys/socket.h>
#include <memory>

#include "src/stirling/bcc_bpf/http_trace.h"

namespace pl {
namespace stirling {

syscall_write_event_t InitEvent(std::string_view msg) {
  syscall_write_event_t event;
  event.attr.event_type = kEventTypeSyscallWriteEvent;
  event.attr.accept_info.addr.sin6_family = AF_INET;
  event.attr.accept_info.timestamp_ns = 0;
  event.attr.time_stamp_ns = 1000000;
  event.attr.msg_buf_size = sizeof(event.msg);
  event.attr.msg_bytes = msg.size();
  msg.copy(event.msg, msg.size());
  return event;
}

TEST(HandleProbeOutputTest, FilterMessages) {
  const std::string msg1 = R"(HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8

)";
  syscall_write_event_t event1 = InitEvent(msg1);

  const std::string msg2 = R"(HTTP/1.1 200 OK
Content-Type: text/plain; charset=utf-8

)";
  syscall_write_event_t event2 = InitEvent(msg2);

  // FRIEND_TEST() does not grant std::make_unique() access to HTTPTraceConnector's private ctor.
  // We choose this style over the HTTPTraceConnector::Create() + dynamic_cast<>, as this is
  // clearer.
  std::unique_ptr<HTTPTraceConnector> source(new HTTPTraceConnector("bcc_http_trace"));
  types::ColumnWrapperRecordBatch record_batch;
  EXPECT_OK(InitRecordBatch(HTTPTraceConnector::kElements[0].elements(),
                            /*target_capacity*/ 1, &record_batch));

  source->OutputEvent(event1, &record_batch);
  for (const auto& column : record_batch) {
    EXPECT_EQ(1, column->Size())
        << "event1 Content-Type does have 'json', and will be selected by the default filter";
  }

  source->OutputEvent(event2, &record_batch);
  for (const auto& column : record_batch) {
    EXPECT_EQ(1, column->Size())
        << "event2 Content-Type has no 'json', and won't be selected by the default filter";
  }

  HTTPTraceConnector::http_response_header_filter_ = {
      {{"Content-Type", "text/plain"}},
      {{"Content-Encoding", "gzip"}},
  };
  HTTPTraceConnector::HandleProbeOutput(source.get(), &event1, sizeof(event1));
  for (const auto& column : record_batch) {
    EXPECT_EQ(1, column->Size())
        << "The filter is changed to require 'text/plain' in Content-Type header, "
           "and event1 Content-Type does not match, and won't be selected";
  }

  // Duplicate headers are allowed in the filters.
  HTTPTraceConnector::http_response_header_filter_ = {
      {{"Content-Type", "application/json"}},
      {{"Content-Encoding", "gzip"}},
  };
  source->OutputEvent(event1, &record_batch);
  for (const auto& column : record_batch) {
    EXPECT_EQ(2, column->Size())
        << "The filter is changed to require 'application/json' in Content-Type header, "
           "and event1 Content-Type matches, and is selected";
  }
}

}  // namespace stirling
}  // namespace pl
