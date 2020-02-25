#include "src/stirling/cassandra/cql_parse.h"

#include <arpa/inet.h>
#include <deque>
#include <string_view>
#include <utility>

#include "src/common/base/byte_utils.h"
#include "src/common/base/types.h"
#include "src/stirling/cassandra/cass_types.h"
#include "src/stirling/common/parse_state.h"

// TODO(oazizi): Consider splitting this file into public and private pieces. The public one would
// have the template implementations, while the private one would have functions in the cass
// namespace.

namespace pl {
namespace stirling {

namespace cass {

// For how to parse frames, see the Cassandra spec:
// https://git-wip-us.apache.org/repos/asf?p=cassandra.git;a=blob_plain;f=doc/native_protocol_v3.spec
ParseState Parse(MessageType type, std::string_view* buf, Frame* result) {
  DCHECK(type == MessageType::kRequest || type == MessageType::kResponse);

  if (buf->size() <= kFrameHeaderLength) {
    return ParseState::kNeedsMoreData;
  }

  std::optional<Opcode> opcode = magic_enum::enum_cast<Opcode>((*buf)[4]);
  if (!opcode) {
    return ParseState::kInvalid;
  }

  result->hdr.version = static_cast<uint8_t>((*buf)[0]);
  result->hdr.flags = static_cast<uint8_t>((*buf)[1]);
  result->hdr.stream = ntohs(utils::LEndianBytesToInt<uint16_t>(buf->substr(2, 2)));
  result->hdr.opcode = static_cast<Opcode>(opcode.value());
  result->hdr.length = ntohl(utils::LEndianBytesToInt<int32_t>(buf->substr(5, 4)));

  // Do we have all the data for the frame?
  if (static_cast<ssize_t>(buf->length()) < kFrameHeaderLength + result->hdr.length) {
    return ParseState::kNeedsMoreData;
  }

  result->msg = buf->substr(kFrameHeaderLength, result->hdr.length);
  buf->remove_prefix(kFrameHeaderLength + result->hdr.length);

  return ParseState::kSuccess;
}

}  // namespace cass

// TODO(oazizi): Find a way to share this implementation across protocols.
template <>
ParseResult<size_t> ParseFrame(MessageType type, std::string_view buf,
                               std::deque<cass::Frame>* messages) {
  std::vector<size_t> start_positions;
  const size_t buf_size = buf.size();
  ParseState s = ParseState::kSuccess;
  size_t bytes_processed = 0;

  while (!buf.empty()) {
    cass::Frame message;

    s = cass::Parse(type, &buf, &message);
    if (s != ParseState::kSuccess) {
      break;
    }

    start_positions.push_back(bytes_processed);
    messages->push_back(std::move(message));
    bytes_processed = (buf_size - buf.size());
  }
  ParseResult<size_t> result{std::move(start_positions), bytes_processed, s};
  return result;
}

template <>
size_t FindFrameBoundary<cass::Frame>(MessageType /*type*/, std::string_view /*buf*/,
                                      size_t /*start_pos*/) {
  return 0;
}

}  // namespace stirling
}  // namespace pl
