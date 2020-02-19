#pragma once

// 4 bytes for encoded code words of :method, :scheme, :authority, and :path.
// This is for sharing with code inside src/stirling.
const size_t kGRPCReqMinHeaderBlockSize = 4;

// Assumes the input buf begins with a HTTP2 frame, and verify if that frame is HEADERS frame as
// part of a gRPC request message. Looks for 4 byte constant with specific format. The chance of a
// random byte sequence passes this function would be at most 1/2^32.
//
// From the spec:
//
// All frames begin with a fixed 9-octet header followed by a variable-length payload.
//
// +-----------------------------------------------+
// |                 Length (24)                   |
// +---------------+---------------+---------------+
// |   Type (8)    |   Flags (8)   |
// +-+-------------+---------------+-------------------------------+
// |R|                 Stream Identifier (31)                      |
// +=+=============================================================+
// |                   Frame Payload (0...)                      ...
// +---------------------------------------------------------------+
static __inline bool looks_like_grpc_req_http2_headers_frame(const char* buf, size_t buf_size) {
  const int kFrameHeaderSize = 9;
  // Requires 9 bytes of HTTP2 frame header and data from header block fragment.
  if (buf_size < kFrameHeaderSize + kGRPCReqMinHeaderBlockSize) {
    return false;
  }
  // Type and flag are the 4th and 5th bytes respectively.
  // See https://http2.github.io/http2-spec/#FrameHeader for HTTP2 frame format.
  char type = buf[3];
  const char kFrameTypeHeaders = 0x01;
  if (type != kFrameTypeHeaders) {
    return false;
  }
  char flag = buf[4];
  const char kFrameFlagEndHeaders = 0x04;
  if (!(flag & kFrameFlagEndHeaders)) {
    return false;
  }
  // Pad-length and stream-dependency might follow the HTTP2 frame header, before header block
  // fragment. See https://http2.github.io/http2-spec/#HEADERS for HEADERS payload format.
  size_t header_block_offset = 0;
  const char kFrameFlagPadded = 0x08;
  if (flag & kFrameFlagPadded) {
    header_block_offset += 1;
  }
  const char kFrameFlagPriority = 0x20;
  if (flag & kFrameFlagPriority) {
    header_block_offset += 5;
  }
  if (buf_size < kFrameHeaderSize + kGRPCReqMinHeaderBlockSize + header_block_offset) {
    return false;
  }

  const char kMethodPostCode = 0x83;
  bool found_method_post = false;

  const char kSchemeHTTPCode = 0x86;
  bool found_scheme_http = false;

  // Search for known static coded header field.
  buf += (kFrameHeaderSize + header_block_offset);
#if defined(__clang__)
#pragma unroll
#endif
  for (size_t i = 0; i < kGRPCReqMinHeaderBlockSize; ++i) {
    if (buf[i] == kMethodPostCode) {
      found_method_post = true;
    }
    if (buf[i] == kSchemeHTTPCode) {
      found_scheme_http = true;
    }
  }
  return found_method_post && found_scheme_http;
}
