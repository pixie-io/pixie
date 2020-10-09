#pragma once

#include <chrono>
#include <string>

#include "src/stirling/protocols/common/protocol_traits.h"
#include "src/stirling/protocols/http2/frame.h"  // For http2::NVMap
#include "src/stirling/protocols/http2/message.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace http2u {

// This struct represents the frames of interest transmitted on an HTTP2 stream.
// It is called a HalfStream because it captures one direction only.
// For example, the request is one HalfStream while the response is on another HalfStream,
// both of which are on the same stream ID of the same connection.
struct HalfStream {
  uint64_t timestamp_ns = 0;
  http2::NVMap headers;
  std::string data;
  http2::NVMap trailers;
  bool end_stream = false;

  void UpdateTimestamp(uint64_t t) {
    if (timestamp_ns == 0) {
      timestamp_ns = t;
    } else {
      timestamp_ns = std::min<uint64_t>(timestamp_ns, t);
    }
  }

  size_t ByteSize() const {
    return sizeof(HalfStream) + data.size() + CountStringMapSize(headers) +
           CountStringMapSize(trailers);
  }

  bool HasGRPCContentType() const {
    return absl::StrContains(headers.ValueByKey(http2::headers::kContentType),
                             http2::headers::kContentTypeGRPC);
  }

  std::string DebugString() const {
    return absl::Substitute("[headers=$0] [data=$1] [trailers=$2] [end_stream=$3]",
                            headers.DebugString(), data, trailers.DebugString(), end_stream);
  }
};

// This struct represents an HTTP2 stream (https://http2.github.io/http2-spec/#StreamsLayer).
// It is split out into a send and recv. Depending on whether we are tracing the requestor
// or the responder, send and recv contain either the request or response.
struct Stream {
  HalfStream send;
  HalfStream recv;

  bool StreamEnded() { return send.end_stream && recv.end_stream; }

  bool consumed = false;

  size_t ByteSize() const { return send.ByteSize() + recv.ByteSize(); }

  std::string DebugString() const {
    return absl::Substitute("[send=$0] [recv=$1]", send.DebugString(), recv.DebugString());
  }
};

using Record = Stream;

struct ProtocolTraits {
  using frame_type = Stream;
  using record_type = Record;
  using state_type = NoState;
};

}  // namespace http2u
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
