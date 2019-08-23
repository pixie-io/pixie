#include "src/stirling/http2.h"

#include <arpa/inet.h>
extern "C" {
#include <nghttp2/nghttp2_frame.h>
#include <nghttp2/nghttp2_hd.h>
#include <nghttp2/nghttp2_helper.h>
}

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "src/common/base/error.h"
#include "src/common/base/status.h"
#include "src/common/grpcutils/utils.h"
#include "src/stirling/bcc_bpf/grpc.h"
#include "src/stirling/grpc.h"

namespace pl {
namespace stirling {
namespace http2 {

using ::pl::grpc::MethodInputOutput;
using ::pl::grpc::MethodPath;
using ::pl::grpc::ServiceDescriptorDatabase;
using ::pl::stirling::grpc::kGRPCMessageHeaderSizeInBytes;

namespace {

bool IsPadded(const nghttp2_frame_hd& hd) { return hd.flags & NGHTTP2_FLAG_PADDED; }
bool IsEndHeaders(const nghttp2_frame_hd& hd) { return hd.flags & NGHTTP2_FLAG_END_HEADERS; }
bool IsEndStream(const nghttp2_frame_hd& hd) { return hd.flags & NGHTTP2_FLAG_END_STREAM; }

// HTTP2 frame header size, see https://http2.github.io/http2-spec/#FrameHeader.
constexpr size_t kFrameHeaderSizeInBytes = 9;

void UnpackData(const uint8_t* buf, Frame* f) {
  nghttp2_data& frame = f->frame.data;
  size_t frame_body_length = frame.hd.length;

  DCHECK(frame.hd.type == NGHTTP2_DATA) << "Must be a DATA frame, got: " << frame.hd.type;

  if (IsPadded(frame.hd)) {
    frame.padlen = *buf;
    ++buf;
    --frame_body_length;
  }

  if (frame_body_length >= frame.padlen) {
    f->u8payload.assign(buf, frame_body_length - frame.padlen);
  } else {
    LOG(DFATAL) << "Pad length cannot be larger than frame body";
    f->u8payload.clear();
  }
}

void UnpackHeaders(const uint8_t* buf, Frame* f) {
  nghttp2_headers& frame = f->frame.headers;
  size_t frame_body_length = frame.hd.length;

  DCHECK(frame.hd.type == NGHTTP2_HEADERS) << "Must be a HEADERS frame, got: " << frame.hd.type;

  if (IsPadded(frame.hd)) {
    frame.padlen = *buf;
    ++buf;
    --frame_body_length;
  }

  nghttp2_frame_unpack_headers_payload(&frame, buf);

  const size_t offset = nghttp2_frame_headers_payload_nv_offset(&frame);
  buf += offset;
  frame_body_length -= offset;

  if (frame_body_length >= frame.padlen) {
    f->u8payload.assign(buf, frame_body_length - frame.padlen);
  } else {
    LOG(DFATAL) << "Pad length cannot be larger than frame body";
    f->u8payload.clear();
  }
}

void UnpackContinuation(const uint8_t* buf, Frame* frame) {
  frame->u8payload.assign(buf, frame->frame.hd.length);
}

}  // namespace

/**
 * @brief Inflates a complete header block. If the input header block fragment, then the results are
 * undefined.
 *
 * This code follows: https://github.com/nghttp2/nghttp2/blob/master/examples/deflate.c.
 */
ParseState InflateHeaderBlock(nghttp2_hd_inflater* inflater, u8string_view buf, NVMap* nv_map) {
  // TODO(yzhao): Experiment continuous parsing of multiple header block fragments from different
  // HTTP2 streams.
  constexpr bool is_final = true;
  for (;;) {
    int inflate_flags = 0;
    nghttp2_nv nv = {};

    const int rv =
        nghttp2_hd_inflate_hd2(inflater, &nv, &inflate_flags, buf.data(), buf.size(), is_final);
    if (rv < 0) {
      return ParseState::kInvalid;
    }
    buf.remove_prefix(rv);

    if (inflate_flags & NGHTTP2_HD_INFLATE_EMIT) {
      nv_map->insert(
          std::make_pair(std::string(reinterpret_cast<const char*>(nv.name), nv.namelen),
                         std::string(reinterpret_cast<const char*>(nv.value), nv.valuelen)));
    }
    if (inflate_flags & NGHTTP2_HD_INFLATE_FINAL) {
      nghttp2_hd_inflate_end_headers(inflater);
      break;
    }
    if ((inflate_flags & NGHTTP2_HD_INFLATE_EMIT) == 0 && buf.empty()) {
      break;
    }
  }
  return ParseState::kSuccess;
}

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
    DCHECK(frame.headers.nva == nullptr);
    DCHECK_EQ(frame.headers.nvlen, 0u);
  }
}

ParseState UnpackFrame(std::string_view* buf, Frame* frame) {
  if (buf->size() < kFrameHeaderSizeInBytes) {
    return ParseState::kNeedsMoreData;
  }

  const uint8_t* u8_buf = reinterpret_cast<const uint8_t*>(buf->data());
  nghttp2_frame_unpack_frame_hd(&frame->frame.hd, u8_buf);

  if (buf->size() < kFrameHeaderSizeInBytes + frame->frame.hd.length) {
    return ParseState::kNeedsMoreData;
  }

  buf->remove_prefix(kFrameHeaderSizeInBytes + frame->frame.hd.length);
  u8_buf += kFrameHeaderSizeInBytes;

  const uint8_t type = frame->frame.hd.type;
  switch (type) {
    case NGHTTP2_DATA:
      UnpackData(u8_buf, frame);
      break;
    case NGHTTP2_HEADERS:
      UnpackHeaders(u8_buf, frame);
      break;
    case NGHTTP2_CONTINUATION:
      UnpackContinuation(u8_buf, frame);
      break;
    default:
      VLOG(1) << "Ignoring frame that is not DATA, HEADERS, or CONTINUATION, got: "
              << FrameTypeName(type);
      return ParseState::kIgnored;
  }
  return ParseState::kSuccess;
}

namespace {

ParseState CheckGRPCMessage(std::string_view buf) {
  if (buf.size() < kGRPCMessageHeaderSizeInBytes) {
    return ParseState::kNeedsMoreData;
  }
  const uint32_t len = nghttp2_get_uint32(reinterpret_cast<const uint8_t*>(buf.data()) + 1);
  if (buf.size() < kGRPCMessageHeaderSizeInBytes + len) {
    return ParseState::kNeedsMoreData;
  }
  return ParseState::kSuccess;
}

/**
 * @brief Given a list of frames for one stream, stitches them together into gRPC request and
 * response messages. Also mark consumed frames, so the caller can destroy them afterwards.
 */
// TODO(yzhao): Turn this into a class that parse frames one by one.
void StitchFrames(const std::vector<const Frame*>& frames, nghttp2_hd_inflater* inflater,
                  std::vector<GRPCMessage>* msgs) {
  size_t header_block_size = 0;
  std::vector<const Frame*> header_block_frames;

  size_t data_block_size = 0;
  std::vector<const Frame*> data_block_frames;

  GRPCMessage msg;
  enum class Progress {
    kUnknown,
    kInDataBlock,
    kInHeadersBlock,
  };
  Progress progress = Progress::kUnknown;

  auto handle_headers_or_continuation = [&progress, &header_block_size, &header_block_frames, &msg,
                                         inflater](const Frame* f) {
    header_block_size += f->u8payload.size();
    header_block_frames.push_back(f);
    if (IsEndHeaders(f->frame.hd)) {
      progress = Progress::kUnknown;
      u8string u8buf;
      u8buf.reserve(header_block_size);
      for (auto* f : header_block_frames) {
        u8buf.append(f->u8payload);
      }
      msg.frames.insert(msg.frames.end(), header_block_frames.begin(), header_block_frames.end());
      header_block_frames.clear();
      header_block_size = 0;
      msg.parse_state = InflateHeaderBlock(inflater, u8buf, &msg.headers);
      LOG_IF(WARNING, msg.parse_state != ParseState::kSuccess) << "Header parsing failed.";
    }
  };

  auto handle_end_stream = [&progress, &data_block_size, &data_block_frames, &msg,
                            msgs](const Frame* f) {
    progress = Progress::kUnknown;
    msg.timestamp_ns = f->timestamp_ns;

    // Now join the messages scattered in the frames.
    msg.message.reserve(data_block_size);
    for (auto* f : data_block_frames) {
      msg.message.append(reinterpret_cast<const char*>(f->u8payload.data()), f->u8payload.size());
    }
    msg.frames.insert(msg.frames.end(), data_block_frames.begin(), data_block_frames.end());
    data_block_frames.clear();
    data_block_size = 0;
    msg.parse_state = CheckGRPCMessage(msg.message);
    msgs->emplace_back(std::move(msg));
  };

  for (const Frame* f : frames) {
    const uint8_t type = f->frame.hd.type;
    switch (type) {
      case NGHTTP2_DATA:
        LOG_IF(DFATAL, progress == Progress::kInHeadersBlock)
            << "DATA frame must not follow a unended HEADERS frame.";
        if (progress == Progress::kUnknown) {
          // This is the first data frame. We must receive a payload with certain size.
          progress = Progress::kInDataBlock;
        }
        data_block_size += f->u8payload.size();
        data_block_frames.push_back(f);
        // gRPC request EOS (end-of-stream) is indicated by END_STREAM flag on the last DATA frame.
        // This also indicates this message is a request. Keep appending data, and then export when
        // END_STREAM is seen.
        if (IsEndStream(f->frame.hd)) {
          msg.type = MessageType::kRequest;
          handle_end_stream(f);
        }
        break;
      case NGHTTP2_HEADERS:
        LOG_IF(DFATAL, progress == Progress::kInHeadersBlock)
            << "HEADERS frame must not follow another unended HEADERS frame.";
        progress = Progress::kInHeadersBlock;
        handle_headers_or_continuation(f);
        // gRPC response EOS (end-of-stream) is indicated by END_STREAM flag on the last HEADERS
        // frame. This also indicates this message is a response.
        // No CONTINUATION frame will be used.
        if (IsEndStream(f->frame.hd)) {
          msg.type = MessageType::kResponse;
          handle_end_stream(f);
        }
        break;
      case NGHTTP2_CONTINUATION:
        LOG_IF(DFATAL, progress != Progress::kInHeadersBlock)
            << "CONTINUATION frame must follow a HEADERS or CONTINUATION frame.";
        handle_headers_or_continuation(f);
        // No need to handle END_STREAM as CONTINUATION frame does not define END_STREAM flag.
        break;
      default:
        constexpr char kErr[] =
            "This function does not accept any frame types other than "
            "DATA, HEADERS, and CONTINUATION";
        CHECK(false) << kErr;
        break;
    }
  }
}

}  // namespace

void StitchGRPCStreamFrames(const std::deque<Frame>& frames, Inflater* inflater,
                            std::map<uint32_t, std::vector<GRPCMessage>>* stream_msgs) {
  std::map<uint32_t, std::vector<const Frame*>> stream_frames;

  // Collect frames for each stream.
  for (const Frame& f : frames) {
    stream_frames[f.frame.hd.stream_id].push_back(&f);
  }
  for (auto& [stream_id, frame_ptrs] : stream_frames) {
    std::vector<GRPCMessage> msgs;
    StitchFrames(frame_ptrs, inflater->inflater(), &msgs);
    if (msgs.empty()) {
      continue;
    }
    stream_msgs->emplace(stream_id, std::move(msgs));
  }
}

std::vector<GRPCReqResp> MatchGRPCReqResp(std::map<uint32_t, std::vector<GRPCMessage>> reqs,
                                          std::map<uint32_t, std::vector<GRPCMessage>> resps) {
  std::vector<GRPCReqResp> res;

  // Treat 2 maps as 2 lists ordered according to stream ID, then merge on common stream IDs.
  // It probably will be faster than naive iteration and search.
  for (auto req_iter = reqs.begin(), resp_iter = resps.begin();
       req_iter != reqs.end() && resp_iter != resps.end();) {
    const uint32_t req_stream_id = req_iter->first;
    const uint32_t resp_stream_id = resp_iter->first;

    std::vector<GRPCMessage>& stream_reqs = req_iter->second;
    std::vector<GRPCMessage>& stream_resps = resp_iter->second;
    LOG_IF(DFATAL, stream_reqs.size() != 1)
        << "Each stream should have exactly one request, stream ID: " << req_stream_id
        << " got: " << stream_reqs.size();
    LOG_IF(DFATAL, stream_resps.size() != 1)
        << "Each stream should have exactly one response, stream ID: " << resp_stream_id
        << " got: " << stream_resps.size();
    if (req_stream_id == resp_stream_id) {
      LOG_IF(DFATAL, stream_reqs.front().type == stream_resps.front().type)
          << "gRPC messages from two streams should be different, got the same type: "
          << static_cast<int>(stream_reqs.front().type);
      res.push_back(GRPCReqResp{std::move(stream_reqs.front()), std::move(stream_resps.front())});
      ++req_iter;
      ++resp_iter;
    } else if (req_stream_id < resp_stream_id) {
      ++req_iter;
    } else {
      ++resp_iter;
    }
  }
  return res;
}

MethodInputOutput GetProtobufMessages(const GRPCMessage& req, ServiceDescriptorDatabase* db) {
  const char kPathHeader[] = ":path";
  auto iter = req.headers.find(kPathHeader);
  if (iter == req.headers.end()) {
    // No call path specified, bail out.
    return {};
  }
  return db->GetMethodInputOutput(MethodPath(iter->second));
}

namespace {

size_t FindFrameBoundaryForGRPCReq(std::string_view buf) {
  if (buf.size() < kFrameHeaderSizeInBytes + kGRPCReqMinHeaderBlockSize) {
    return std::string_view::npos;
  }
  for (size_t i = 0; i <= buf.size() - kFrameHeaderSizeInBytes - kGRPCReqMinHeaderBlockSize; ++i) {
    if (looks_like_grpc_req_http2_headers_frame(buf.data() + i, buf.size() - i)) {
      return i;
    }
  }
  return std::string_view::npos;
}

// There is a HTTP2 frame header and at least 1 encoded header field.
constexpr size_t kRespHeadersFrameMinSize = kFrameHeaderSizeInBytes + 1;

// Assumes the input buf begins with a HTTP2 frame, and verify if that frame is the first HEADERS
// frame of a gRPC response. Looks for 3 byte constant with specific format, and one byte with
// 0 bit. The chance of a random byte sequence passes this function would be at most 1/2^25.
//
// TODO(yzhao): Consider move this into bcc_bpf/http2.h, that way this becomes symmetric with
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
  if (buf[kFrameHeaderSizeInBytes + header_block_offset] != kStatus200) {
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
  for (size_t i = 0; i <= buf.size() - kFrameHeaderSizeInBytes - 1; ++i) {
    if (LooksLikeHeadersFrameForGRPCResp(buf.substr(i))) {
      return i;
    }
  }
  return std::string_view::npos;
}

}  // namespace
}  // namespace http2

template <>
ParseResult<size_t> Parse(MessageType unused_type, std::string_view buf,
                          std::deque<http2::Frame>* frames) {
  PL_UNUSED(unused_type);

  std::vector<size_t> start_position;
  const size_t buf_size = buf.size();
  ParseState s = ParseState::kSuccess;

  // Note that HTTP2 connection preface, or MAGIC as used in nghttp2, must be at the beginning of
  // the stream. The following tries to detect complete or partial connection preface.
  if (buf.size() < NGHTTP2_CLIENT_MAGIC_LEN &&
      buf == std::string_view(NGHTTP2_CLIENT_MAGIC, buf.size())) {
    return {{}, 0, ParseState::kNeedsMoreData};
  }
  if (absl::StartsWith(buf, NGHTTP2_CLIENT_MAGIC)) {
    buf.remove_prefix(NGHTTP2_CLIENT_MAGIC_LEN);
  }

  while (!buf.empty()) {
    const size_t frame_begin = buf_size - buf.size();
    http2::Frame frame;
    s = UnpackFrame(&buf, &frame);
    if (s == ParseState::kNeedsMoreData) {
      break;
    }
    if (s == ParseState::kIgnored) {
      // Even if the last frame is ignored, the parse is still successful.
      s = ParseState::kSuccess;
      continue;
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
      res = http2::FindFrameBoundaryForGRPCReq(buf.substr(start_pos));
      break;
    case MessageType::kResponse:
      res = http2::FindFrameBoundaryForGRPCResp(buf.substr(start_pos));
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
