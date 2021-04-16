#pragma once

#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/data_stream_buffer.h"

namespace px {
namespace stirling {
namespace protocols {

class DataStreamBufferTestWrapper {
 protected:
  static constexpr size_t kDataBufferSize = 128 * 1024;

  DataStreamBufferTestWrapper() : data_buffer_(kDataBufferSize) {}

  void AddEvent(const SocketDataEvent& event) {
    data_buffer_.Add(event.attr.pos, event.msg, event.attr.timestamp_ns);
  }

  void AddEvents(const std::vector<SocketDataEvent>& events) {
    for (const auto& e : events) {
      AddEvent(e);
    }
  }

  DataStreamBuffer data_buffer_;
};

template <typename TStrType>
std::vector<SocketDataEvent> CreateEvents(const std::vector<TStrType>& msgs) {
  std::vector<SocketDataEvent> events;
  size_t pos = 0;
  for (size_t i = 0; i < msgs.size(); ++i) {
    events.emplace_back();
    events.back().msg = msgs[i];
    events.back().attr.timestamp_ns = i;
    events.back().attr.pos = pos;
    events.back().attr.msg_size = msgs[i].size();
    pos += msgs[i].size();
  }
  return events;
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
