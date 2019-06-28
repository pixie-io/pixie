#include <vector>

#include "src/stirling/connection_tracker.h"

namespace pl {
namespace stirling {

void ConnectionTracker::AddConnOpenEvent(conn_info_t conn_info) {
  conn_.timestamp_ns = conn_info.timestamp_ns;
  conn_.tgid = conn_info.tgid;
  conn_.fd = conn_info.fd;
  auto ip_endpoint_or = ParseSockAddr(conn_info);
  if (ip_endpoint_or.ok()) {
    conn_.remote_addr = std::move(ip_endpoint_or.ValueOrDie().ip);
    conn_.remote_port = ip_endpoint_or.ValueOrDie().port;
  } else {
    LOG(WARNING) << "Could not parse IP address.";
  }
}

void ConnectionTracker::AddConnCloseEvent() { closed_ = true; }

void ConnectionTracker::AddDataEvent(socket_data_event_t event) {
  const uint64_t seq_num = event.attr.seq_num;

  switch (event.attr.event_type) {
    case kEventTypeSyscallWriteEvent:
    case kEventTypeSyscallSendEvent:
      send_data_.events.emplace(seq_num, std::move(event));
      break;
    case kEventTypeSyscallReadEvent:
    case kEventTypeSyscallRecvEvent:
      recv_data_.events.emplace(seq_num, std::move(event));
      break;
    default:
      LOG(ERROR) << absl::StrFormat("AddDataEvent() unexpected event type %d",
                                    event.attr.event_type);
  }
}

void DataStream::ExtractMessages(TrafficMessageType type) {
  // TODO(oazizi): Continue to propagate the templating through this function.
  EventParser<HTTPMessage> parser;

  const size_t orig_offset = offset;

  // Prepare all recorded events for parsing.
  std::vector<std::string_view> msgs;
  uint64_t next_seq_num = events.begin()->first;
  for (const auto& [seq_num, event] : events) {
    // Found a discontinuity in sequence numbers. Stop submitting events to parser.
    if (seq_num != next_seq_num) {
      break;
    }

    // The main message to submit to parser.
    std::string_view msg(event.msg, event.attr.msg_size);

    // First message may have been partially processed by a previous call to this function.
    // In such cases, the offset will be non-zero, and we need a sub-string of the first event.
    if (offset != 0) {
      CHECK(offset < event.attr.msg_size);
      msg = msg.substr(offset, event.attr.msg_size - offset);
      offset = 0;
    }

    parser.Append(msg, event.attr.timestamp_ns);
    msgs.push_back(msg);
    next_seq_num++;
  }

  // Now parse all the appended events.
  ParseResult<BufferPosition> parse_result = parser.ParseMessages(type, &messages);

  // If we weren't able to process anything new, then the offset should be the same as last time.
  if (offset != 0 && parse_result.end_position.seq_num == 0) {
    CHECK_EQ(parse_result.end_position.offset, orig_offset);
  }

  // Find and erase events that have been fully processed.
  auto erase_iter = events.begin();
  std::advance(erase_iter, parse_result.end_position.seq_num);
  events.erase(events.begin(), erase_iter);
  offset = parse_result.end_position.offset;
}

}  // namespace stirling
}  // namespace pl
