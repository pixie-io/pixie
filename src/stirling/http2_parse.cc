#include "src/stirling/http2_parse.h"

#include <utility>
#include <vector>

extern "C" {
#include "src/stirling/bcc_bpf_interface/grpc.h"
}

namespace pl {
namespace stirling {

namespace {

size_t FindFrameBoundaryForGRPCReq(std::string_view buf) {
  if (buf.size() < http2::kFrameHeaderSizeInBytes + kGRPCReqMinHeaderBlockSize) {
    return std::string_view::npos;
  }
  for (size_t i = 0; i <= buf.size() - http2::kFrameHeaderSizeInBytes - kGRPCReqMinHeaderBlockSize;
       ++i) {
    if (looks_like_grpc_req_http2_headers_frame(buf.data() + i, buf.size() - i)) {
      return i;
    }
  }
  return std::string_view::npos;
}

// There is a HTTP2 frame header and at least 1 encoded header field.
constexpr size_t kRespHeadersFrameMinSize = http2::kFrameHeaderSizeInBytes + 1;

// Assumes the input buf begins with a HTTP2 frame, and verify if that frame is the first HEADERS
// frame of a gRPC response. Looks for 3 byte constant with specific format, and one byte with
// 0 bit. The chance of a random byte sequence passes this function would be at most 1/2^25.
//
// TODO(yzhao): Consider moving this into shared/http2.h, that way this becomes symmetric with
// looks_like_grpc_req_http2_headers_frame().
bool LooksLikeHeadersFrameForGRPCResp(std::string_view buf) {
  const char type = buf[3];
  if (type != NGHTTP2_HEADERS) {
    return false;
  }
  const char flag = buf[4];
  if (!(flag & NGHTTP2_FLAG_END_HEADERS)) {
    return false;
  }
  // The first header frame of a response cannot have END_STREAM flag. END_STREAM is reserved for
  // the last header frame in the HEADERS DATA HEADERS sequence for a succeeded RPC response.
  // TODO(yzhao): Figure out the pattern of a failed RPC call's response.
  if (flag & NGHTTP2_FLAG_END_STREAM) {
    return false;
  }
  size_t header_block_offset = 0;
  if (flag & NGHTTP2_FLAG_PADDED) {
    header_block_offset += 1;
  }
  if (flag & NGHTTP2_FLAG_PRIORITY) {
    header_block_offset += 4;
  }
  if (buf.size() < kRespHeadersFrameMinSize + header_block_offset) {
    return false;
  }
  // ":status OK" is almost the only HTTP2 status used in gRPC, because it, contrary to REST,
  // does not encode RPC-layer status in HTTP2 status.
  constexpr char kStatus200 = 0x88;
  if (buf[http2::kFrameHeaderSizeInBytes + header_block_offset] != kStatus200) {
    return false;
  }
  return true;
}

// Performs a linear search over the input buf and returns the start position of the first sequence
// that looks like a HEADERS frame. Returns npos if not found.
size_t FindFrameBoundaryForGRPCResp(std::string_view buf) {
  if (buf.size() < kRespHeadersFrameMinSize) {
    return std::string_view::npos;
  }
  for (size_t i = 0; i <= buf.size() - http2::kFrameHeaderSizeInBytes - 1; ++i) {
    if (LooksLikeHeadersFrameForGRPCResp(buf.substr(i))) {
      return i;
    }
  }
  return std::string_view::npos;
}

}  // namespace

template <>
ParseResult<size_t> Parse(MessageType unused_type, std::string_view buf,
                          std::deque<http2::Frame>* frames) {
  PL_UNUSED(unused_type);

  // Note that HTTP2 connection preface, or MAGIC as used in nghttp2, must be at the beginning of
  // the stream. The following tries to detect complete or partial connection preface.
  if (buf.size() < NGHTTP2_CLIENT_MAGIC_LEN &&
      buf == std::string_view(NGHTTP2_CLIENT_MAGIC, buf.size())) {
    return {{}, 0, ParseState::kNeedsMoreData};
  }

  std::vector<size_t> start_position;
  const size_t buf_size = buf.size();
  ParseState s = ParseState::kSuccess;

  if (absl::StartsWith(buf, NGHTTP2_CLIENT_MAGIC)) {
    buf.remove_prefix(NGHTTP2_CLIENT_MAGIC_LEN);
  }

  while (!buf.empty()) {
    const size_t frame_begin = buf_size - buf.size();
    http2::Frame frame;
    s = UnpackFrame(&buf, &frame);
    if (s == ParseState::kIgnored) {
      // Even if the last frame is ignored, the parse is still successful.
      s = ParseState::kSuccess;
      continue;
    }
    if (s != ParseState::kSuccess) {
      // For any other non-success states, stop and exit.
      break;
    }
    DCHECK(s == ParseState::kSuccess);
    start_position.push_back(frame_begin);
    frames->push_back(std::move(frame));
  }
  return {std::move(start_position), buf_size - buf.size(), s};
}

template <>
size_t FindMessageBoundary<http2::Frame>(MessageType type, std::string_view buf, size_t start_pos) {
  size_t res = std::string_view::npos;
  switch (type) {
    case MessageType::kRequest:
      res = FindFrameBoundaryForGRPCReq(buf.substr(start_pos));
      break;
    case MessageType::kResponse:
      res = FindFrameBoundaryForGRPCResp(buf.substr(start_pos));
      break;
    case MessageType::kUnknown:
      DCHECK(false) << "The message type must be specified.";
      break;
  }
  if (res == std::string_view::npos) {
    return std::string_view::npos;
  }
  return start_pos + res;
}

}  // namespace stirling
}  // namespace pl
