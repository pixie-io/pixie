#pragma once

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

  std::string HeaderValue(const std::string& key, const std::string& default_value = "") {
    auto iter = headers.find(key);
    if (iter != headers.end()) {
      return iter->second;
    }
    return default_value;
  }
};

struct HalfStream {
  NVMap headers;
  std::string data;
  NVMap trailers;
  bool end_stream;
};

struct Stream {
  HalfStream req;
  HalfStream resp;
};

}  // namespace http2
}  // namespace stirling
}  // namespace pl
