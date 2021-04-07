#pragma once

#include <memory>

#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/go_grpc_types.hpp"
#include "src/stirling/source_connectors/socket_tracer/testing/clock.h"

namespace pl {
namespace stirling {
namespace testing {

class StreamEventGenerator {
 public:
  StreamEventGenerator(Clock* clock, conn_id_t conn_id, uint32_t stream_id)
      : clock_(clock), conn_id_(conn_id), stream_id_(stream_id) {}

  template <DataFrameEventType TType>
  std::unique_ptr<HTTP2DataEvent> GenDataFrame(std::string_view body, bool end_stream = false) {
    auto frame = std::make_unique<HTTP2DataEvent>();
    frame->attr.conn_id = conn_id_;
    frame->attr.type = TType;
    frame->attr.timestamp_ns = clock_->now();
    frame->attr.stream_id = stream_id_;
    frame->attr.end_stream = end_stream;
    frame->attr.pos = pos;
    pos += body.length();
    frame->attr.data_size = body.length();
    frame->attr.data_buf_size = body.length();
    frame->payload = body;
    return frame;
  }

  template <HeaderEventType TType>
  std::unique_ptr<HTTP2HeaderEvent> GenHeader(std::string_view name, std::string_view value) {
    auto hdr = std::make_unique<HTTP2HeaderEvent>();
    hdr->attr.conn_id = conn_id_;
    hdr->attr.type = TType;
    hdr->attr.timestamp_ns = clock_->now();
    hdr->attr.stream_id = stream_id_;
    hdr->attr.end_stream = false;
    hdr->name = name;
    hdr->value = value;
    return hdr;
  }

  template <HeaderEventType TType>
  std::unique_ptr<HTTP2HeaderEvent> GenEndStreamHeader() {
    auto hdr = std::make_unique<HTTP2HeaderEvent>();
    hdr->attr.conn_id = conn_id_;
    hdr->attr.type = TType;
    hdr->attr.timestamp_ns = clock_->now();
    hdr->attr.stream_id = stream_id_;
    hdr->attr.end_stream = true;
    hdr->name = "";
    hdr->value = "";
    return hdr;
  }

 private:
  // Used to track the position for the next data frame.
  size_t pos = 0;
  Clock* clock_;
  conn_id_t conn_id_;
  uint32_t stream_id_;
};

}  // namespace testing
}  // namespace stirling
}  // namespace pl
