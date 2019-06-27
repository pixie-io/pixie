#pragma once

#include <deque>
#include <map>
#include <string>

#include "src/stirling/bcc_bpf/socket_trace.h"
#include "src/stirling/http_parse.h"

namespace pl {
namespace stirling {

/**
 * @brief Describes a connection from user space. This corresponds to struct conn_info_t in
 * src/stirling/bcc_bpf/socket_trace.h.
 */
struct SocketConnection {
  uint64_t timestamp_ns = 0;
  uint32_t tgid = 0;
  uint32_t fd = -1;
  std::string remote_addr = "-";
  int remote_port = -1;
};

struct DataStream {
  // Raw data events from BPF.
  // TODO(oazizi): Convert this to vector.
  std::map<uint64_t, socket_data_event_t> events;

  // To support partially processed events,
  // the stream may start at an offset in the first raw data event.
  uint64_t offset = 0;

  // Vector of parsed HTTP messages.
  // Once parsed, the raw data events should be discarded.
  std::deque<HTTPMessage> messages;
};

struct ConnectionTracker {
  SocketConnection conn;

  // TODO(oazizi): Will this be covered by conn?
  TrafficProtocol protocol;

  // The data collected by the stream, one per direction.
  DataStream send_data;
  DataStream recv_data;

  // TODO(oazizi): Add a bool to say whether the stream has been touched since last transfer (to
  // avoid useless computation).
  // TODO(oazizi): Could also record a timestamp, so we could destroy old EventStreams completely.
};

struct HTTPStream : public ConnectionTracker {
  HTTPStream() { protocol = kProtocolHTTP; }
};

struct HTTP2Stream : public ConnectionTracker {
  HTTP2Stream() { protocol = kProtocolHTTP2; }
  // TODO(yzhao): Add HTTP2Parser, or gRPC parser.
};

}  // namespace stirling
}  // namespace pl
