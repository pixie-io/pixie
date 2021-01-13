#include "src/stirling/source_connectors/socket_tracer/protocols/cql/parse.h"

#include <arpa/inet.h>
#include <deque>
#include <string_view>
#include <utility>

#include "src/common/base/byte_utils.h"
#include "src/common/base/types.h"
#include "src/stirling/common/parse_state.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/types.h"

// TODO(oazizi): Consider splitting this file into public and private pieces. The public one would
// have the template implementations, while the private one would have functions in the cass
// namespace.

namespace pl {
namespace stirling {
namespace protocols {
namespace cass {

// For how to parse frames, see the Cassandra spec:
// https://git-wip-us.apache.org/repos/asf?p=cassandra.git;a=blob_plain;f=doc/native_protocol_v3.spec
ParseState ParseFrame(MessageType type, std::string_view* buf, Frame* result) {
  DCHECK(type == MessageType::kRequest || type == MessageType::kResponse);

  if (buf->size() < kFrameHeaderLength) {
    return ParseState::kNeedsMoreData;
  }

  std::optional<Opcode> opcode = magic_enum::enum_cast<Opcode>((*buf)[4]);
  if (!opcode) {
    return ParseState::kInvalid;
  }

  bool is_resp = static_cast<uint8_t>((*buf)[0]) & kDirectionMask;
  result->hdr.version = static_cast<uint8_t>((*buf)[0]) & kVersionMask;
  result->hdr.flags = static_cast<uint8_t>((*buf)[1]);
  result->hdr.stream = ntohs(utils::LEndianBytesToInt<uint16_t>(buf->substr(2, 2)));
  result->hdr.opcode = static_cast<Opcode>(opcode.value());
  result->hdr.length = ntohl(utils::LEndianBytesToInt<int32_t>(buf->substr(5, 4)));

  if (is_resp != IsRespOpcode(result->hdr.opcode)) {
    return ParseState::kInvalid;
  }

  if (result->hdr.version < kMinSupportedProtocolVersion ||
      result->hdr.version > kMaxSupportedProtocolVersion) {
    return ParseState::kInvalid;
  }

  if (result->hdr.length > kMaxFrameLength || result->hdr.length < 0) {
    return ParseState::kInvalid;
  }

  // Do we have all the data for the frame?
  if (static_cast<ssize_t>(buf->length()) < kFrameHeaderLength + result->hdr.length) {
    return ParseState::kNeedsMoreData;
  }

  result->msg = buf->substr(kFrameHeaderLength, result->hdr.length);
  buf->remove_prefix(kFrameHeaderLength + result->hdr.length);

  return ParseState::kSuccess;
}
}  // namespace cass

template <>
ParseState ParseFrame(MessageType type, std::string_view* buf, cass::Frame* result) {
  return cass::ParseFrame(type, buf, result);
}

template <>
size_t FindFrameBoundary<cass::Frame>(MessageType /*type*/, std::string_view /*buf*/,
                                      size_t /*start_pos*/) {
  // Not implemented.
  return std::string::npos;
}

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
