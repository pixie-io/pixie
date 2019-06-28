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

}  // namespace stirling
}  // namespace pl
