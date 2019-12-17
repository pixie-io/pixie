#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "src/stirling/common/utils.h"
#include "src/stirling/http2/frame.h"

namespace pl {
namespace stirling {
namespace http2 {

struct HTTP2Message {
  // TODO(yzhao): We keep this field for easier testing. Update tests to not rely on input invalid
  // data.
  ParseState parse_state = ParseState::kUnknown;
  ParseState headers_parse_state = ParseState::kUnknown;
  MessageType type = MessageType::kUnknown;
  TimeSpan time_span;
  uint64_t timestamp_ns = 0;

  NVMap headers;
  std::string message;
  std::vector<const Frame*> frames;

  void MarkFramesConsumed() const {
    for (const auto* f : frames) {
      f->consumed = true;
    }
  }
};

// This struct represents the frames of interest transmitted on an HTTP2 stream.
// It is called a HalfStream because it captures one direction only.
// For example, the request is one HalfStream while the response is on another HalfStream,
// both of which are on the same stream ID of the same connection.
struct HalfStream {
  uint64_t timestamp_ns = 0;
  NVMap headers;
  std::string data;
  NVMap trailers;
  bool end_stream;

  void UpdateTimestamp(uint64_t t) {
    if (timestamp_ns == 0) {
      timestamp_ns = t;
    } else {
      timestamp_ns = std::min<uint64_t>(timestamp_ns, t);
    }
  }
};

// This struct represents an HTTP2 stream (https://http2.github.io/http2-spec/#StreamsLayer).
// It is split out into a send and recv. Depending on whether we are tracing the requestor
// or the responder, send and recv contain either the request or response.
struct Stream {
  HalfStream send;
  HalfStream recv;
};

}  // namespace http2
}  // namespace stirling
}  // namespace pl
