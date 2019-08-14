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
#include <vector>

#include "src/common/base/mixins.h"
#include "src/common/base/status.h"
#include "src/common/grpcutils/service_descriptor_database.h"
#include "src/stirling/bcc_bpf/socket_trace.h"
#include "src/stirling/event_parser.h"

namespace pl {
namespace stirling {
namespace http2 {

using u8string = std::basic_string<uint8_t>;
using u8string_view = std::basic_string_view<uint8_t>;
using NVMap = std::multimap<std::string, std::string>;

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
 * @brief Returns a string for a particular type.
 */
std::string_view FrameTypeName(uint8_t type);

/**
 * @brief Inflates a complete header block in the input buf, writes the header field to nv_map.
 */
ParseState InflateHeaderBlock(nghttp2_hd_inflater* inflater, u8string_view buf, NVMap* nv_map);

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
  mutable bool consumed = false;
};

// TODO(yzhao): Move ParseState inside http_parse.h to utils/parse_state.h; and then use it as
// return type for UnpackFrame{s}.
/**
 * @brief Extract HTTP2 frame from the input buffer, and removes the consumed data from the buffer.
 */
ParseState UnpackFrame(std::string_view* buf, Frame* frame);

struct GRPCMessage {
  // TODO(yzhao): We keep this field for easier testing. Update tests to not rely on input invalid
  // data.
  ParseState parse_state = ParseState::kUnknown;
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

/*
 * @brief Stitches frames as either request or response. Also removes consumed frames.
 *
 * @param frames The frames for gRPC request or response messages.
 * @param stream_msgs The gRPC messages for each stream, keyed by stream ID. Note this is HTTP2
 * stream ID, not our internal stream ID for TCP connections.
 */
void StitchGRPCStreamFrames(const std::deque<Frame>& frames, Inflater* inflater,
                            std::map<uint32_t, std::vector<GRPCMessage>>* stream_msgs);

/**
 * @brief A convenience holder of gRPC req & resp.
 */
// TODO(yzhao): Investigate converging with ReqRespPair in socket_trace_connector.h.
struct GRPCReqResp {
  GRPCMessage req;
  GRPCMessage resp;
};

/**
 * @brief Matchs req & resp GRPCMessage of the same streams. The input arguments are moved to the
 * returned result.
 */
std::vector<GRPCReqResp> MatchGRPCReqResp(std::map<uint32_t, std::vector<GRPCMessage>> reqs,
                                          std::map<uint32_t, std::vector<GRPCMessage>> resps);

inline void EraseConsumedFrames(std::deque<Frame>* frames) {
  frames->erase(
      std::remove_if(frames->begin(), frames->end(), [](const Frame& f) { return f.consumed; }),
      frames->end());
}

/**
 * @brief Returns the dynamic protobuf messages for the called method in the request.
 */
::pl::grpc::MethodInputOutput GetProtobufMessages(const GRPCMessage& req,
                                                  ::pl::grpc::ServiceDescriptorDatabase* db);

// TODO(yzhao): gRPC has a feature called bidirectional streaming:
// https://grpc.io/docs/guides/concepts/. Investigate how to parse that off HTTP2 frames.

}  // namespace http2

/**
 * @brief Unpacks the buf as HTTP2 frames. The results are put into messages.
 * The parameter type is not used, but is required to matches the function used by
 * EventParser<std::unique_ptr<Frame>>.
 */
template <>
ParseResult<size_t> Parse(MessageType unused_type, std::string_view buf,
                          std::deque<http2::Frame>* messages);

template <>
size_t FindMessageBoundary<http2::Frame>(MessageType type, std::string_view buf, size_t start_pos);

}  // namespace stirling
}  // namespace pl
