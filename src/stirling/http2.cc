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
#include <variant>
#include <vector>

#include "absl/strings/str_cat.h"
#include "src/common/base/error.h"
#include "src/common/base/status.h"
#include "src/common/grpcutils/utils.h"
#include "src/stirling/bcc_bpf_interface/grpc.h"
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
  // HTTP2 spec specifies a maximal frame size. Any frames that is larger than that should be
  // considered a parse failure, which usually is caused by losing track of the frame boundary.
  constexpr int kMaxFrameSize = (1 << 24) - 1;
  if (frame->frame.hd.length > kMaxFrameSize) {
    return ParseState::kInvalid;
  }

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

}  // namespace

/**
 * @brief Given a list of frames for one stream, stitches them together into gRPC request and
 * response messages. Also mark consumed frames, so the caller can destroy them afterwards.
 */
// TODO(yzhao): Turn this into a class that parse frames one by one.
ParseState StitchFrames(const std::vector<const Frame*>& frames, nghttp2_hd_inflater* inflater,
                        std::vector<GRPCMessage>* msgs) {
  size_t header_block_size = 0;
  std::vector<const Frame*> header_block_frames;

  size_t data_block_size = 0;
  std::vector<const Frame*> data_block_frames;

  GRPCMessage msg;
  bool is_in_header_block = false;

  auto handle_headers_or_continuation = [&is_in_header_block, &header_block_size,
                                         &header_block_frames, &msg, inflater](const Frame* f) {
    header_block_size += f->u8payload.size();
    header_block_frames.push_back(f);
    if (IsEndHeaders(f->frame.hd)) {
      is_in_header_block = false;
      u8string u8buf;
      u8buf.reserve(header_block_size);
      for (auto* f : header_block_frames) {
        u8buf.append(f->u8payload);
      }
      msg.frames.insert(msg.frames.end(), header_block_frames.begin(), header_block_frames.end());
      header_block_frames.clear();
      header_block_size = 0;
      msg.parse_state = InflateHeaderBlock(inflater, u8buf, &msg.headers);
    }
  };

  auto handle_end_stream = [&data_block_size, &data_block_frames, &msg, msgs]() {
    // HTTP2 spec allows the following frames sequence:
    // HEADERS frame, no END_HEADERS flag, with END_STREAM flag
    // 0 or more CONTINUATION frames, no END_HEADERS flag
    // CONTINUATION frame, with END_HEADERS flag
    //
    // Fortunately gRPC only set END_STREAM and END_HEADERS together, and never do the above
    // sequence.
    //
    // TODO(yzhao): Consider handle the above case.

    // Join the messages scattered in the frames.
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
        if (is_in_header_block) {
          LOG(WARNING) << "DATA frame must not follow a unended HEADERS frame. "
                          "Indicates losing track of the frame boundary. Stream ID: "
                       << f->frame.hd.stream_id;
          return ParseState::kInvalid;
        }
        is_in_header_block = false;
        data_block_size += f->u8payload.size();
        data_block_frames.push_back(f);
        // gRPC request EOS (end-of-stream) is indicated by END_STREAM flag on the last DATA frame.
        // This also indicates this message is a request. Keep appending data, and then export when
        // END_STREAM is seen.
        if (IsEndStream(f->frame.hd)) {
          msg.type = MessageType::kRequest;
          msg.timestamp_ns = f->timestamp_ns;
          handle_end_stream();
        }
        break;
      case NGHTTP2_HEADERS:
        if (is_in_header_block) {
          LOG(WARNING) << "HEADERS frame must not follow another unended HEADERS frame. "
                          "Indicates losing track of the frame boundary. Stream ID: "
                       << f->frame.hd.stream_id;
          return ParseState::kInvalid;
        }
        is_in_header_block = true;
        handle_headers_or_continuation(f);
        // gRPC response EOS (end-of-stream) is indicated by END_STREAM flag on the last HEADERS
        // frame. This also indicates this message is a response.
        // No CONTINUATION frame will be used.
        if (IsEndStream(f->frame.hd)) {
          msg.type = MessageType::kResponse;
          msg.timestamp_ns = f->timestamp_ns;
          handle_end_stream();
        }
        break;
      case NGHTTP2_CONTINUATION:
        if (!is_in_header_block) {
          LOG(WARNING) << "CONTINUATION frame must follow a HEADERS or CONTINUATION frame. "
                          "Indicates losing track of the frame boundary. Stream ID: "
                       << f->frame.hd.stream_id;
          return ParseState::kInvalid;
        }
        handle_headers_or_continuation(f);
        // No need to handle END_STREAM as CONTINUATION frame does not define END_STREAM flag.
        break;
      default:
        LOG(DFATAL) << "This function does not accept any frame types other than "
                       "DATA, HEADERS, and CONTINUATION.";
        break;
    }
  }
  return ParseState::kSuccess;
}

ParseState StitchFramesToGRPCMessages(const std::deque<Frame>& frames, Inflater* inflater,
                                      std::map<uint32_t, GRPCMessage>* stream_msgs) {
  std::map<uint32_t, std::vector<const Frame*>> stream_frames;

  // Collect frames for each stream.
  for (const Frame& f : frames) {
    stream_frames[f.frame.hd.stream_id].push_back(&f);
  }
  for (auto& [stream_id, frame_ptrs] : stream_frames) {
    std::vector<GRPCMessage> msgs;
    ParseState state = StitchFrames(frame_ptrs, inflater->inflater(), &msgs);
    if (state != ParseState::kSuccess) {
      return state;
    }
    if (msgs.empty()) {
      continue;
    }
    if (msgs.size() > 1) {
      return ParseState::kInvalid;
    }
    stream_msgs->emplace(stream_id, std::move(msgs[0]));
  }
  return ParseState::kSuccess;
}

std::vector<GRPCReqResp> MatchGRPCReqResp(std::map<uint32_t, GRPCMessage> reqs,
                                          std::map<uint32_t, GRPCMessage> resps) {
  std::vector<GRPCReqResp> res;

  // Treat 2 maps as 2 lists ordered according to stream ID, then merge on common stream IDs.
  // It probably will be faster than naive iteration and search.
  for (auto req_iter = reqs.begin(), resp_iter = resps.begin();
       req_iter != reqs.end() && resp_iter != resps.end();) {
    const uint32_t req_stream_id = req_iter->first;
    const uint32_t resp_stream_id = resp_iter->first;

    GRPCMessage& stream_req = req_iter->second;
    GRPCMessage& stream_resp = resp_iter->second;
    if (req_stream_id == resp_stream_id) {
      LOG_IF(DFATAL, stream_req.type == stream_resp.type)
          << "gRPC messages from two streams should be different, got the same type: "
          << static_cast<int>(stream_req.type);
      res.push_back(GRPCReqResp{std::move(stream_req), std::move(stream_resp)});
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

ParseState DecodeInteger(u8string_view* buf, size_t prefix, uint32_t* res) {
  // Mandates that the input buffer includes all bytes of the encode integer. That means a few
  // cases handled by nghttp2_hd_decode_length() are not needed.

  // 'shift' is used to indicate how many bits are already decoded in the previous calls of
  // nghttp2_hd_decode_length.
  size_t shift = 0;
  // 'finished' is used to indicate the completion of the decoding.
  int finished = 0;
  // 'initial' is the decoded partial value of the previous calls. Note this cannot replace 'shift'
  // as the leading 0's will confuse the shift size.
  const size_t initial = 0;
  ssize_t rv = nghttp2_hd_decode_length(res, &shift, &finished, initial, shift,
                                        const_cast<uint8_t*>(buf->data()),
                                        const_cast<uint8_t*>(buf->data()) + buf->size(), prefix);
  if (rv == -1) {
    return ParseState::kInvalid;
  }
  if (finished != 1) {
    return ParseState::kNeedsMoreData;
  }
  buf->remove_prefix(rv);
  return ParseState::kSuccess;
}

constexpr uint8_t kMaskTableSizeUpdate = 0xE0;
constexpr uint8_t kBitsTableSizeUpdate = 0x20;
constexpr size_t kPrefixTableSizeUpdate = 5;

constexpr uint8_t kMaskIndexed = 0x80;
constexpr uint8_t kBitsIndexed = 0x80;
constexpr size_t kPrefixIndexed = 7;

constexpr uint8_t kMaskLiteralIndexing = 0xC0;
constexpr uint8_t kBitsLiteralIndexing = 0x40;
constexpr size_t kPrefixLiteralIndexing = 6;

// NoIndexing subsumes "without indexing" and "never indexing" as referred in HPACK spec.
// Where they do share the same bit pattern as the following names suggest,
// except kBitsLiteralNeverIndexing.
constexpr uint8_t kMaskLiteralNoIndexing = 0xF0;
constexpr uint8_t kBitsLiteralNoIndexing = 0x00;
constexpr uint8_t kBitsLiteralNeverIndexing = 0x10;
constexpr size_t kPrefixLiteralNoIndexing = 4;

// Prefix of a length field.
constexpr size_t kPrefixLength = 7;

// Decode length and get the rest of the field as data.
ParseState DecodeLengthPrefixedData(u8string_view* buf, u8string_view* res) {
  uint32_t len = 0;
  const ParseState state = DecodeInteger(buf, kPrefixLength, &len);
  if (state != ParseState::kSuccess) {
    return state;
  }

  if (buf->size() < len) {
    return ParseState::kNeedsMoreData;
  }
  *res = buf->substr(0, len);
  buf->remove_prefix(len);
  return ParseState::kSuccess;
}

inline bool IsHuffEncoded(uint8_t first_byte) {
  // The huffman encoded bit is the leftest bit.
  return (first_byte & (1 << 7)) != 0;
}

template <typename NameValueType>
ParseState DecodeLengthPrefixedNameValue(u8string_view* buf, bool new_name,
                                         size_t name_index_prefix, NameValueType* res) {
  std::variant<uint32_t, u8string_view> name_variant;
  if (new_name) {
    // All 0 in the lower 6 bits indicate a new name, instead of an indexed one.
    buf->remove_prefix(1);
    if (buf->empty()) {
      return ParseState::kNeedsMoreData;
    }
    res->is_name_huff_encoded = IsHuffEncoded(buf->front());
    u8string_view name;
    const ParseState state = DecodeLengthPrefixedData(buf, &name);
    if (state != ParseState::kSuccess) {
      return state;
    }
    name_variant = name;
  } else {
    uint32_t name_index = 0;
    const ParseState state = DecodeInteger(buf, name_index_prefix, &name_index);
    if (state != ParseState::kSuccess) {
      return state;
    }
    name_variant = name_index;
  }
  res->is_value_huff_encoded = IsHuffEncoded(buf->front());
  u8string_view value;
  const ParseState state = DecodeLengthPrefixedData(buf, &value);
  if (state != ParseState::kSuccess) {
    return state;
  }
  res->name = name_variant;
  res->value = value;
  return ParseState::kSuccess;
}

// An alternative approach is to patch nghttp2 so that it can output encoded header fields without
// early termination. We prefer this, as the understanding of the protocol details is required for
// the protobuf inference.
//
// See https://http2.github.io/http2-spec/compression.html#detailed.format for more details on
// HPACK header field format.
ParseState ParseHeaderBlock(u8string_view* buf, std::vector<HeaderField>* res) {
  while (!buf->empty()) {
    if ((buf->front() & kMaskTableSizeUpdate) == kBitsTableSizeUpdate) {
      uint32_t v = 0;
      const ParseState state = DecodeInteger(buf, kPrefixTableSizeUpdate, &v);
      if (state != ParseState::kSuccess) {
        return state;
      }
      res->emplace_back(TableSizeUpdate{v});
    } else if ((buf->front() & kMaskIndexed) == kBitsIndexed) {
      uint32_t v = 0;
      const ParseState state = DecodeInteger(buf, kPrefixIndexed, &v);
      if (state != ParseState::kSuccess) {
        return state;
      }
      res->emplace_back(IndexedHeaderField{v});
    } else if ((buf->front() & kMaskLiteralIndexing) == kBitsLiteralIndexing) {
      LiteralHeaderField field;
      field.update_dynamic_table = true;
      ParseState state = DecodeLengthPrefixedNameValue(buf, buf->front() == kBitsLiteralIndexing,
                                                       kPrefixLiteralIndexing, &field);
      if (state != ParseState::kSuccess) {
        return state;
      }
      res->emplace_back(field);
    } else if ((buf->front() & kMaskLiteralNoIndexing) == kBitsLiteralNoIndexing ||
               (buf->front() & kMaskLiteralNoIndexing) == kBitsLiteralNeverIndexing) {
      LiteralHeaderField field;
      field.update_dynamic_table = false;
      ParseState state = DecodeLengthPrefixedNameValue(
          buf, buf->front() == kBitsLiteralNoIndexing || buf->front() == kBitsLiteralNeverIndexing,
          kPrefixLiteralNoIndexing, &field);
      if (state != ParseState::kSuccess) {
        return state;
      }
      res->emplace_back(field);
    }
  }
  return ParseState::kSuccess;
}

}  // namespace http2

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
