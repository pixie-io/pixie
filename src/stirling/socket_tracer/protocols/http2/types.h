#pragma once

#include <chrono>
#include <map>
#include <string>

#include <absl/strings/str_join.h>

#include "src/stirling/common/utils.h"
#include "src/stirling/socket_tracer/protocols/common/interface.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace http2 {

namespace headers {

constexpr char kContentType[] = "content-type";
constexpr char kMethod[] = ":method";
constexpr char kPath[] = ":path";

constexpr char kContentTypeGRPC[] = "application/grpc";

}  // namespace headers

// Note that NVMap keys (HTTP2 header field names) are assumed to be lowercase to match spec:
//
// From https://http2.github.io/http2-spec/#HttpHeaders:
// ... header field names MUST be converted to lowercase prior to their encoding in HTTP/2.
// A request or response containing uppercase header field names MUST be treated as malformed.
class NVMap : public std::multimap<std::string, std::string> {
 public:
  using std::multimap<std::string, std::string>::multimap;

  std::string ValueByKey(const std::string& key, const std::string& default_value = "") const {
    const auto iter = find(key);
    if (iter != end()) {
      return iter->second;
    }
    return default_value;
  }

  std::string ToString() const { return absl::StrJoin(*this, ", ", absl::PairFormatter(":")); }
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
    return absl::StrContains(headers.ValueByKey(headers::kContentType), headers::kContentTypeGRPC);
  }

  std::string ToString() const {
    return absl::Substitute("[headers=$0] [data=$1] [trailers=$2] [end_stream=$3]",
                            headers.ToString(), data, trailers.ToString(), end_stream);
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

  std::string ToString() const {
    return absl::Substitute("[send=$0] [recv=$1]", send.ToString(), recv.ToString());
  }
};

using Record = Stream;

struct ProtocolTraits {
  using frame_type = Stream;
  using record_type = Record;
  using state_type = NoState;
};

}  // namespace http2
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
