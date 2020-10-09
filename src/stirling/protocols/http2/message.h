#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "src/stirling/common/utils.h"
#include "src/stirling/protocols/http2/frame.h"

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

  bool HasGRPCContentType() const {
    return absl::StrContains(headers.ValueByKey(headers::kContentType), headers::kContentTypeGRPC);
  }
};

}  // namespace http2
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
