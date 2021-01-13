#pragma once

#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"

namespace pl {
namespace stirling {
namespace protocols {

class EventParserTestWrapper {
 protected:
  void AppendEvent(const SocketDataEvent& event) { parser_.Append(event); }

  void AppendEvents(const std::vector<SocketDataEvent>& events) {
    for (const auto& e : events) {
      AppendEvent(e);
    }
  }

  EventParser parser_;
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
}  // namespace pl
