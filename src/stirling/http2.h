#pragma once

#include <nghttp2/nghttp2.h>

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "src/common/base/mixins.h"
#include "src/common/base/status.h"
#include "src/stirling/bcc_bpf/socket_trace.h"
#include "src/stirling/event_parser.h"

namespace pl {
namespace stirling {
namespace http2 {

constexpr size_t kGRPCMessageHeaderSizeInBytes = 5;

using u8string = std::basic_string<uint8_t>;
using u8string_view = std::basic_string_view<uint8_t>;
using NVMap = std::multimap<std::string, std::string>;

/**
 * @brief Returns a string for a particular type.
 */
std::string_view FrameTypeName(uint8_t type);

/**
 * @brief A wrapper around  nghttp2_frame. nghttp2_frame misses some fields, for example, it has no
 * data body field in nghttp2_data. The payload is a name meant to be generic enough so that it can
 * be used to store such fields for different message types.
 */
struct Frame {
  Frame();
  ~Frame();

  // TODO(yzhao): Consider use std::unique_ptr<nghttp2_frame> to avoid copy.
  nghttp2_frame frame;
  u8string u8payload;
  // TODO(yzhao): This will be landed in D1081. Add this to make build pass. Will land only after
  // D1081.
  uint64_t timestamp_ns;

  // If true, means this frame is processed and can be destroyed.
  bool consumed = false;
};

// TODO(yzhao): Move ParseState inside http_parse.h to utils/parse_state.h; and then use it as
// return type for UnpackFrame{s}.
/**
 * @brief Extract HTTP2 frame from the input buffer, and removes the consumed data from the buffer.
 */
ParseState UnpackFrame(std::string_view* buf, Frame* frame);

/**
 * @brief Unpacks the buf as HTTP2 frames. The results are put into messages.
 * The parameter type is not used, but is required to matches the function used by
 * EventParser<std::unique_ptr<Frame>>.
 */
ParseResult<size_t> Parse(MessageType unused_type, std::string_view buf,
                          std::deque<Frame>* messages);

struct GRPCMessage : public NotCopyable {
  GRPCMessage() = default;
  GRPCMessage(GRPCMessage&& other) noexcept
      : type(other.type),
        parse_succeeded(other.parse_succeeded),
        headers(std::move(other.headers)),
        message(std::move(other.message)) {}

  MessageType type = MessageType::kUnknown;
  uint64_t timestamp_ns = 0;
  // TODO(yzhao): We should not need this, as we'll be changing into a lazy parse pattern, where
  // incomplete messages will not be output, and the parser would restart from a known frame index.
  bool parse_succeeded = false;
  NVMap headers;
  u8string message;

  // TODO(yzhao): This will be changed into a free function that uses descriptor database APIs,
  // which simulates the interface of gRPC reflection, to parse the gRPC messages.
  template <typename ProtobufType>
  bool ParseProtobuf(ProtobufType* pb) const {
    if (!parse_succeeded) {
      return false;
    }
    return pb->ParseFromArray(message.data() + kGRPCMessageHeaderSizeInBytes,
                              message.size() - kGRPCMessageHeaderSizeInBytes);
  }
};

/**
 * @brief Required for fitting in the SocketTraceConnector::ConsumeMessage() template.
 */
void PreProcessMessage(GRPCMessage* message);

/*
 * @brief Stitches frames as either request or response. Also removes consumed frames.
 *
 * @param frames The frames for gRPC request or response messages.
 * @param stream_msgs The gRPC messages for each stream, keyed by stream ID. Note this is HTTP2
 * stream ID, not our internal stream ID for TCP connections.
 */
Status StitchGRPCStreamFrames(std::deque<Frame>* frames,
                              std::map<uint32_t, std::vector<GRPCMessage>>* stream_msgs);

}  // namespace http2
}  // namespace stirling
}  // namespace pl
