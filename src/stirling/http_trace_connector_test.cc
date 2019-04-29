#include "src/stirling/http_trace_connector.h"

#include <gtest/gtest.h>

#include <memory>

#include "src/stirling/bcc_bpf/http_trace.h"

namespace pl {
namespace stirling {

TEST(HandleProbeOutputTest, FilterMessages) {
  const std::string msg = R"(HTTP/1.1 200 OK
Date: Wed, 24 Apr 2019 05:13:42 GMT
Content-Length: 1
Content-Type: application/json; charset=utf-8

X)";
  syscall_write_event_t event;
  event.attr.event_type = kEventTypeSyscallWriteEvent;
  event.attr.msg_size = msg.size();
  msg.copy(event.msg, msg.size());

  std::unique_ptr<SourceConnector> source = HTTPTraceConnector::Create("bcc_http_trace");
  types::ColumnWrapperRecordBatch record_batch;
  Status init_status =
      InitRecordBatch(HTTPTraceConnector::kElements, /*target_capacity*/ 1, &record_batch);
  EXPECT_EQ(0, init_status.code());

  HTTPTraceConnector::filter_substrs_ = {"text/plain"};
  HTTPTraceConnector::HandleProbeOutput(source.get(), &event, sizeof(event), &record_batch);
  for (const auto& column : record_batch) {
    EXPECT_EQ(0, column->Size());
  }

  HTTPTraceConnector::filter_substrs_ = {"application/json"};
  HTTPTraceConnector::HandleProbeOutput(source.get(), &event, sizeof(event), &record_batch);
  for (const auto& column : record_batch) {
    EXPECT_EQ(1, column->Size());
  }
}

}  // namespace stirling
}  // namespace pl
