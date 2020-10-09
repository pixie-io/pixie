#pragma once

extern "C" {
#include <nghttp2/nghttp2.h>
#include <nghttp2/nghttp2_frame.h>
#include <nghttp2/nghttp2_hd.h>
#include <nghttp2/nghttp2_helper.h>
}

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "src/common/base/mixins.h"
#include "src/common/base/status.h"
#include "src/common/grpcutils/service_descriptor_database.h"
#include "src/stirling/bcc_bpf_interface/socket_trace.h"
#include "src/stirling/common/parse_state.h"
#include "src/stirling/protocols/common/event_parser.h"
#include "src/stirling/protocols/common/protocol_traits.h"
#include "src/stirling/protocols/common/stitcher.h"
#include "src/stirling/protocols/http2/frame.h"
#include "src/stirling/protocols/http2/message.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace http2 {

using u8string_view = std::basic_string_view<uint8_t>;

// HTTP2 frame header size, see https://http2.github.io/http2-spec/#FrameHeader.
constexpr size_t kFrameHeaderSizeInBytes = 9;

/**
 * @brief Inflater wraps nghttp2_hd_inflater and implements RAII.
 */
class Inflater {
 public:
  Inflater() {
    int rv = nghttp2_hd_inflate_init(&inflater_, nghttp2_mem_default());
    LOG_IF(DFATAL, rv != 0) << "Failed to initialize nghttp2_hd_inflater!";
  }

  ~Inflater() { nghttp2_hd_inflate_free(&inflater_); }

  nghttp2_hd_inflater* inflater() { return &inflater_; }

 private:
  nghttp2_hd_inflater inflater_;
};

/**
 * @brief Inflates a complete header block in the input buf, writes the header field to nv_map.
 */
ParseState InflateHeaderBlock(nghttp2_hd_inflater* inflater, u8string_view buf, NVMap* nv_map);

/**
 * @brief Extract HTTP2 frame from the input buffer, and removes the consumed data from the buffer.
 */
ParseState UnpackFrame(std::string_view* buf, Frame* frame);

struct Record {
  HTTP2Message req;
  HTTP2Message resp;
};

struct State {
  std::monostate global;
  http2::Inflater send;
  http2::Inflater recv;
};

struct ProtocolTraits {
  using frame_type = Frame;
  using record_type = Record;
  using state_type = State;
};

/**
 * @brief Stitches frames to create header blocks and inflate them.
 *
 * HTTP2 requires the frames of a header block not mixed with any types of frames other than
 * CONTINUATION or frames with any other stream IDs.
 *
 * Since they need to be parsed exactly once and in the same order as they arrive, they are stitched
 * together first and then inflated in place. The resultant name & value pairs of a header block is
 * stored in the first HEADERS frame of the header block.
 */
void StitchAndInflateHeaderBlocks(nghttp2_hd_inflater* inflater, std::deque<Frame>* frames);

// Used by StitchFramesToGRPCMessages() put here for testing.
ParseState StitchGRPCMessageFrames(const std::vector<const Frame*>& frames,
                                   std::vector<HTTP2Message>* msgs);

/*
 * @brief Stitches frames as either request or response. Also marks the consumed frames.
 * You must then erase the consumed frames afterwards.
 *
 * @param frames The frames for gRPC request or response messages.
 * @param stream_msgs The gRPC messages for each stream, keyed by stream ID. Note this is HTTP2
 * stream ID, not our internal stream ID for TCP connections.
 */
ParseState StitchFramesToGRPCMessages(const std::deque<Frame>& frames,
                                      std::map<uint32_t, HTTP2Message>* stream_msgs);

/**
 * @brief Matchs req & resp HTTP2Message of the same streams. The input arguments are moved to the
 * returned result.
 */
std::vector<Record> MatchGRPCReqResp(std::map<uint32_t, HTTP2Message> reqs,
                                     std::map<uint32_t, HTTP2Message> resps);

inline void EraseConsumedFrames(std::deque<Frame>* frames) {
  frames->erase(
      std::remove_if(frames->begin(), frames->end(), [](const Frame& f) { return f.consumed; }),
      frames->end());
}

// TODO(yzhao): gRPC has a feature called bidirectional streaming:
// https://grpc.io/docs/guides/concepts/. Investigate how to parse that off HTTP2 frames.

/**
 * @brief Decode a variable length integer used in HPACK. If succeeded, the consumed bytes are
 * removed from the input buf, and the value is written to res.
 */
ParseState DecodeInteger(u8string_view* buf, size_t prefix, uint32_t* res);

struct TableSizeUpdate {
  uint32_t size;
};

struct IndexedHeaderField {
  uint32_t index;
};

// Will update the dynamic table.
struct LiteralHeaderField {
  // If true, this field should be inserted into the dynamic table.
  bool update_dynamic_table = false;
  // Only meaningful if the name is a string value.
  bool is_name_huff_encoded = false;
  // uint32_t is for the indexed name, u8string_view is for a potentially-huffman-encoded string.
  std::variant<uint32_t, u8string_view> name;
  // TODO(yzhao): Consider create a struct to hold a string value to represent a potentially
  // huffman-encoded string literal.
  bool is_value_huff_encoded = false;
  u8string_view value;
};

using HeaderField = std::variant<TableSizeUpdate, IndexedHeaderField, LiteralHeaderField>;

inline bool ShouldUpdateDynamicTable(const HeaderField& field) {
  return std::holds_alternative<LiteralHeaderField>(field) &&
         std::get<LiteralHeaderField>(field).update_dynamic_table;
}

inline uint32_t GetIndex(const HeaderField& field) {
  return std::get<IndexedHeaderField>(field).index;
}

constexpr size_t kStaticTableSize = 61;

inline bool IsInStaticTable(const HeaderField& field) {
  return std::holds_alternative<IndexedHeaderField>(field) && GetIndex(field) <= kStaticTableSize;
}

inline bool IsInDynamicTable(const HeaderField& field) {
  return std::holds_alternative<IndexedHeaderField>(field) && GetIndex(field) > kStaticTableSize;
}

inline bool HoldsPlainTextName(const HeaderField& field) {
  return std::holds_alternative<LiteralHeaderField>(field) &&
         std::holds_alternative<u8string_view>(std::get<LiteralHeaderField>(field).name) &&
         !std::get<LiteralHeaderField>(field).is_name_huff_encoded;
}

inline std::string_view GetLiteralNameAsStringView(const HeaderField& field) {
  u8string_view res = std::get<u8string_view>(std::get<LiteralHeaderField>(field).name);
  return {reinterpret_cast<const char*>(res.data()), res.size()};
}

/**
 * @brief Parses a complete header block, writes the encoded header fields to res, and removes any
 * parsed data from buf.
 */
ParseState ParseHeaderBlock(u8string_view* buf, std::vector<HeaderField>* res);

RecordsWithErrorCount<Record> ProcessFrames(std::deque<Frame>* req_frames,
                                            nghttp2_hd_inflater* req_inflater,
                                            std::deque<Frame>* resp_frames,
                                            nghttp2_hd_inflater* resp_inflater);

}  // namespace http2

inline RecordsWithErrorCount<http2::Record> ProcessFrames(std::deque<http2::Frame>* req_frames,
                                                          std::deque<http2::Frame>* resp_frames,
                                                          http2::State* state) {
  return ProcessFrames(req_frames, state->send.inflater(), resp_frames, state->recv.inflater());
}

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
