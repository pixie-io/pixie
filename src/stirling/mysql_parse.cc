#include "src/stirling/mysql_parse.h"

#include <arpa/inet.h>
#include <deque>
#include <string>
#include <string_view>
#include <utility>

#include "src/common/base/byte_utils.h"
#include "src/stirling/event_parser.h"
#include "src/stirling/mysql/mysql.h"

namespace pl {
namespace stirling {

namespace mysql {
ParseState Parse(MessageType type, std::string_view* buf, Packet* result) {
  if (type != MessageType::kRequest && type != MessageType::kResponse) {
    return ParseState::kInvalid;
  }

  if (buf->size() <= kPacketHeaderLength) {
    return ParseState::kInvalid;
  }

  // TODO(oazizi): Is pre-checking requests here a good idea? Somewhat out of place.
  // Better fit for stitcher (process of converting packets to events).
  if (type == MessageType::kRequest) {
    uint8_t command = (*buf)[kPacketHeaderLength];
    if (command > kMaxCommandValue) {
      return ParseState::kInvalid;
    }
  }

  int packet_length = utils::LEStrToInt(buf->substr(0, kPacketHeaderLength - 1));
  int buffer_length = buf->length();

  // 3 bytes of packet length and 1 byte of packet number.
  if (buffer_length < kPacketHeaderLength + packet_length) {
    return ParseState::kNeedsMoreData;
  }

  result->msg = buf->substr(kPacketHeaderLength, packet_length);
  buf->remove_prefix(kPacketHeaderLength + packet_length);

  return ParseState::kSuccess;
}
}  // namespace mysql

// TODO(chengruizhe): Could be templatized with HTTP Parser
template <>
ParseResult<size_t> Parse(MessageType type, std::string_view buf,
                          std::deque<mysql::Packet>* messages) {
  std::vector<size_t> start_positions;
  const size_t buf_size = buf.size();
  ParseState s = ParseState::kSuccess;
  size_t bytes_processed = 0;

  while (!buf.empty()) {
    mysql::Packet message;

    s = mysql::Parse(type, &buf, &message);
    if (s != ParseState::kSuccess) {
      break;
    }

    start_positions.push_back(bytes_processed);
    message.creation_timestamp = std::chrono::steady_clock::now();
    messages->push_back(std::move(message));
    bytes_processed = (buf_size - buf.size());
  }
  ParseResult<size_t> result{std::move(start_positions), bytes_processed, s};
  return result;
}

template <>
size_t FindMessageBoundary<mysql::Packet>(MessageType /*type*/, std::string_view /*buf*/,
                                          size_t /*start_pos*/) {
  return 0;
}

}  // namespace stirling
}  // namespace pl
