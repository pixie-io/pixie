#pragma once

#include <memory>

namespace pl {
namespace stirling {
namespace testing {

class StreamEventGenerator {
 public:
  StreamEventGenerator(conn_id_t conn_id, uint32_t stream_id)
      : conn_id_(conn_id), stream_id_(stream_id) {}

  template <DataFrameEventType TType>
  std::unique_ptr<HTTP2DataEvent> GenDataFrame(std::string_view body) {
    auto frame = std::make_unique<HTTP2DataEvent>();
    frame->attr.conn_id = conn_id_;
    frame->attr.stream_id = stream_id_;
    frame->attr.ftype = TType;
    frame->attr.timestamp_ns = ++ts_;
    frame->attr.data_len = body.length();
    frame->payload = body;
    return frame;
  }

  template <HeaderEventType TType>
  std::unique_ptr<HTTP2HeaderEvent> GenHeader(std::string_view name, std::string_view value) {
    auto hdr = std::make_unique<HTTP2HeaderEvent>();
    hdr->attr.conn_id = conn_id_;
    hdr->attr.stream_id = stream_id_;
    hdr->attr.htype = TType;
    hdr->attr.timestamp_ns = ++ts_;
    hdr->name = name;
    hdr->value = value;
    return hdr;
  }

 private:
  conn_id_t conn_id_;
  uint32_t stream_id_;

  uint64_t ts_ = 0;
};

}  // namespace testing
}  // namespace stirling
}  // namespace pl
