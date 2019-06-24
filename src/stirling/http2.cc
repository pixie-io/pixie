#include "src/stirling/http2.h"

#include <arpa/inet.h>
extern "C" {
#include <nghttp2/nghttp2_frame.h>
#include <nghttp2/nghttp2_helper.h>
}

#include <map>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "src/common/base/error.h"
#include "src/common/base/status.h"

namespace pl {
namespace stirling {
namespace http2 {

namespace {

bool IsPadded(const nghttp2_frame_hd& hd) { return hd.flags & NGHTTP2_FLAG_PADDED; }

// HTTP2 frame header size, see https://http2.github.io/http2-spec/#FrameHeader.
constexpr size_t kFrameHeaderSizeInBytes = 9;
constexpr size_t kGRPCMessageHeaderSizeInBytes = 5;

void UnpackData(const uint8_t* buf, Frame* frame) {
  int frame_body_length = frame->frame.hd.length;
  if (IsPadded(frame->frame.hd)) {
    frame->frame.data.padlen = *buf;
    ++buf;
    --frame_body_length;
  }
  frame->payload.assign(reinterpret_cast<const char*>(buf),
                        frame_body_length - frame->frame.data.padlen);
}

Status UnpackHeaders(const uint8_t* buf, nghttp2_headers* frame) {
  int inflate_flags = 0;
  int rv = 0;
  nghttp2_nv nv = {};
  // TODO(yzhao): Investigate if we can obtain the count of nv pairs before hand.
  std::vector<nghttp2_nv> nvs;
  int frame_body_length = frame->hd.length;

  if (IsPadded(frame->hd)) {
    frame->padlen = *buf;
    ++buf;
    --frame_body_length;
  }
  nghttp2_frame_unpack_headers_payload(frame, buf);
  rv = nghttp2_frame_headers_payload_nv_offset(frame);
  buf += rv;
  frame_body_length -= rv;
  nghttp2_hd_inflater inflater = {};
  rv = nghttp2_hd_inflate_init(&inflater, nghttp2_mem_default());
  if (rv != 0) {
    return error::Internal("Failed to initialize nghttp2_hd_inflater");
  }
  nvs.clear();
  for (;;) {
    inflate_flags = 0;
    // This function handles padding correctly, so we do not need to account for the unused data at
    // the tail of the payload.
    rv = nghttp2_hd_inflate_hd2(&inflater, &nv, &inflate_flags, buf, frame_body_length,
                                frame_body_length);
    if (rv < 0) {
      return error::Internal("Failed to inflate header");
    }
    buf += rv;
    frame_body_length -= rv;

    if (inflate_flags & NGHTTP2_HD_INFLATE_EMIT) {
      nvs.push_back(nv);
    }
    if (inflate_flags & NGHTTP2_HD_INFLATE_FINAL) {
      break;
    }
    if ((inflate_flags & NGHTTP2_HD_INFLATE_EMIT) == 0 && frame_body_length == 0) {
      break;
    }
  }
  // TODO(yzhao): Right now we do not handle CONTINUATION frame, and simply end inflating for
  // this header after loop. Continuation frame could contain data for a headers frame, so we
  // need to fix that.
  nghttp2_hd_inflate_end_headers(&inflater);
  frame->nvlen = nvs.size();
  // TODO(yzhao): Create a new C++-flavored data structure for HEADERS frame so that the headers can
  // be managed through RAII.
  rv = nghttp2_nv_array_copy(&frame->nva, nvs.data(), nvs.size(), nghttp2_mem_default());
  nghttp2_hd_inflate_free(&inflater);
  if (rv != 0) {
    return error::Internal("Failed to copy nghttp_nv array");
  }
  return Status::OK();
}

}  // namespace

std::string_view FrameTypeName(uint8_t type) {
  switch (type) {
    case NGHTTP2_DATA:
      return "DATA";
    case NGHTTP2_HEADERS:
      return "HEADERS";
    case NGHTTP2_PRIORITY:
      return "PRIORITY";
    case NGHTTP2_RST_STREAM:
      return "RST_STREAM";
    case NGHTTP2_SETTINGS:
      return "SETTINGS";
    case NGHTTP2_PUSH_PROMISE:
      return "PUSH_PROMISE";
    case NGHTTP2_PING:
      return "PING";
    case NGHTTP2_GOAWAY:
      return "GOAWAY";
    case NGHTTP2_WINDOW_UPDATE:
      return "WINDOW_UPDATE";
    case NGHTTP2_CONTINUATION:
      return "CONTINUATION";
    case NGHTTP2_ALTSVC:
      return "ALTSVC";
    case NGHTTP2_ORIGIN:
      return "ORIGIN";
    default:
      return "unknown";
  }
}

// frame{} zero initialize the member, which is needed to make sure default value is sensible.
Frame::Frame() : frame{} {}
Frame::~Frame() {
  if (frame.hd.type == NGHTTP2_HEADERS) {
    nghttp2_frame_headers_free(&frame.headers, nghttp2_mem_default());
  }
}

Status UnpackFrame(std::string_view* buf, Frame* frame) {
  if (buf->size() < kFrameHeaderSizeInBytes) {
    return error::InvalidArgument(absl::Substitute(
        "Not enough data to parse, got: $0 must be >= $1", buf->size(), kFrameHeaderSizeInBytes));
  }

  const uint8_t* u8_buf = reinterpret_cast<const uint8_t*>(buf->data());
  nghttp2_frame_unpack_frame_hd(&frame->frame.hd, u8_buf);

  if (buf->size() < kFrameHeaderSizeInBytes + frame->frame.hd.length) {
    return error::InvalidArgument(
        absl::Substitute("Not enough u8_buf to parse, got: $0 must be >= $1", buf->size(),
                         kFrameHeaderSizeInBytes + frame->frame.hd.length));
  }

  buf->remove_prefix(kFrameHeaderSizeInBytes + frame->frame.hd.length);
  u8_buf += kFrameHeaderSizeInBytes;

  const uint8_t type = frame->frame.hd.type;
  switch (type) {
    case NGHTTP2_DATA:
      UnpackData(u8_buf, frame);
      break;
    case NGHTTP2_HEADERS:
      PL_RETURN_IF_ERROR(UnpackHeaders(u8_buf, &frame->frame.headers));
      break;
    case NGHTTP2_CONTINUATION:
      return error::Unimplemented("CONTINUATION frame parsing is in progress");
      break;
    case NGHTTP2_PRIORITY:
    case NGHTTP2_RST_STREAM:
    case NGHTTP2_SETTINGS:
    case NGHTTP2_PUSH_PROMISE:
    case NGHTTP2_PING:
    case NGHTTP2_GOAWAY:
    case NGHTTP2_WINDOW_UPDATE:
    case NGHTTP2_ALTSVC:
    case NGHTTP2_ORIGIN:
      return error::Cancelled(absl::StrCat("Ignored frame type: ", FrameTypeName(type)));
    default:
      return error::Cancelled(absl::StrCat("Unknown frame type: ", type));
  }
  return Status::OK();
}

Status UnpackFrames(std::string_view* buf, std::vector<std::unique_ptr<Frame>>* frames) {
  while (!buf->empty()) {
    auto frame = std::make_unique<Frame>();
    Status s = UnpackFrame(buf, frame.get());
    if (error::IsCancelled(s) || error::IsUnimplemented(s)) {
      continue;
    }
    PL_RETURN_IF_ERROR(s);
    frames->push_back(std::move(frame));
  }
  return Status::OK();
}

Status UnpackGRPCMessage(std::string_view* buf, GRPCMessage* msg) {
  if (buf->size() < kGRPCMessageHeaderSizeInBytes) {
    return error::InvalidArgument(absl::StrCat("Needs at least 5 bytes, got: ", buf->size()));
  }
  const uint8_t* u8_buf = reinterpret_cast<const uint8_t*>(buf->data());
  msg->compressed_flag = u8_buf[0];
  msg->length = nghttp2_get_uint32(u8_buf + 1);
  if (buf->size() < kGRPCMessageHeaderSizeInBytes + msg->length) {
    return error::InvalidArgument(absl::Substitute("Needs at least $0 bytes, got: $1",
                                                   kGRPCMessageHeaderSizeInBytes + msg->length,
                                                   buf->size()));
  }
  msg->message = buf->substr(kGRPCMessageHeaderSizeInBytes, msg->length);
  buf->remove_prefix(kGRPCMessageHeaderSizeInBytes + msg->length);
  return Status::OK();
}

}  // namespace http2
}  // namespace stirling
}  // namespace pl
