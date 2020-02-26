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

}  // namespace http2
}  // namespace stirling
}  // namespace pl
