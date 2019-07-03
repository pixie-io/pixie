#include <vector>

#include "src/stirling/connection_tracker.h"
#include "src/stirling/http2.h"

namespace pl {
namespace stirling {

void ConnectionTracker::AddConnOpenEvent(conn_info_t conn_info) {
  SetTrafficClass(conn_info.traffic_class);

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

void ConnectionTracker::AddConnCloseEvent(conn_info_t conn_info) {
  close_info_.closed = true;
  close_info_.timestamp_ns = conn_info.timestamp_ns;
  close_info_.send_seq_num = conn_info.wr_seq_num;
  close_info_.recv_seq_num = conn_info.rd_seq_num;
}

void ConnectionTracker::AddDataEvent(SocketDataEvent event) {
  SetTrafficClass(event.attr.traffic_class);

  const uint64_t seq_num = event.attr.seq_num;

  switch (event.attr.event_type) {
    case kEventTypeSyscallWriteEvent:
    case kEventTypeSyscallSendEvent:
      send_data_.events.emplace(seq_num, event);
      ++num_send_events_;
      break;
    case kEventTypeSyscallReadEvent:
    case kEventTypeSyscallRecvEvent:
      recv_data_.events.emplace(seq_num, event);
      ++num_recv_events_;
      break;
    default:
      LOG(ERROR) << absl::StrFormat("AddDataEvent() unexpected event type %d",
                                    event.attr.event_type);
  }
}

bool ConnectionTracker::AllEventsReceived() const {
  return (close_info_.closed) && (num_send_events_ == close_info_.send_seq_num) &&
         (num_recv_events_ == close_info_.recv_seq_num);
}

void ConnectionTracker::SetTrafficClass(struct traffic_class_t traffic_class) {
  DCHECK((traffic_class_.protocol == kProtocolUnknown) == (traffic_class_.role == kRoleUnknown));

  if (traffic_class_.protocol == kProtocolUnknown) {
    traffic_class_ = traffic_class;
  } else if (traffic_class.protocol != kProtocolUnknown) {
    DCHECK_EQ(traffic_class.protocol, traffic_class.protocol)
        << "Not allowed to change the protocol of an active ConnectionTracker";
    DCHECK_EQ(traffic_class.role, traffic_class.role)
        << "Not allowed to change the role of an active ConnectionTracker";
  }
}

DataStream* ConnectionTracker::req_data() {
  switch (traffic_class_.role) {
    case kRoleRequestor:
      return &send_data_;
    case kRoleResponder:
      return &recv_data_;
    default:
      return nullptr;
  }
}

DataStream* ConnectionTracker::resp_data() {
  switch (traffic_class_.role) {
    case kRoleRequestor:
      return &recv_data_;
    case kRoleResponder:
      return &send_data_;
    default:
      return nullptr;
  }
}

template <class TMessageType>
void DataStream::ExtractMessages(MessageType type) {
  EventParser<TMessageType> parser;

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
    std::string_view msg = event.msg;

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
  auto& typed_messages = std::get<std::deque<TMessageType>>(messages);
  ParseResult<BufferPosition> parse_result = parser.ParseMessages(type, &typed_messages);

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

// Explicit instantiation for HTTPMessage.
template void DataStream::ExtractMessages<HTTPMessage>(MessageType type);
template void DataStream::ExtractMessages<http2::Frame>(MessageType type);

}  // namespace stirling
}  // namespace pl
