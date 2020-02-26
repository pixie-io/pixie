#include "src/stirling/mysql/mysql_parse.h"

#include <arpa/inet.h>
#include <deque>
#include <string_view>
#include <utility>

#include "src/common/base/byte_utils.h"
#include "src/stirling/common/parse_state.h"
#include "src/stirling/mysql/mysql_types.h"

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

  result->sequence_id = static_cast<uint8_t>((*buf)[3]);

  // TODO(oazizi): Is pre-checking requests here a good idea? Somewhat out of place.
  // Better fit for stitcher (process of converting packets to events).
  if (type == MessageType::kRequest) {
    uint8_t command = (*buf)[kPacketHeaderLength];
    if (command > kMaxCommandValue) {
      return ParseState::kInvalid;
    }
  }

  int packet_length = utils::LEndianBytesToInt<int, kPayloadLengthLength>(*buf);
  ssize_t buffer_length = buf->length();

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
ParseResult<size_t> ParseFrame(MessageType type, std::string_view buf,
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
size_t FindFrameBoundary<mysql::Packet>(MessageType type, std::string_view buf, size_t start_pos) {
  if (buf.length() < mysql::kPacketHeaderLength) {
    return std::string::npos;
  }

  if (type == MessageType::kResponse) {
    // No real search implemented for responses.
    // TODO(oazizi): Is there something we can implement here?
    return std::string::npos;
  }

  // Need at least kPacketHeaderLength bytes + 1 command byte in buf.
  for (size_t i = start_pos; i < buf.size() - mysql::kPacketHeaderLength; ++i) {
    std::string_view cur_buf = buf.substr(i);
    int packet_length = utils::LEndianBytesToInt<int, mysql::kPayloadLengthLength>(cur_buf);
    uint8_t sequence_id = static_cast<uint8_t>(cur_buf[3]);
    auto command_byte =
        magic_enum::enum_cast<mysql::MySQLEventType>(cur_buf[mysql::kPacketHeaderLength]);

    // Requests must have sequence id of 0.
    if (sequence_id != 0) {
      continue;
    }

    // If the command byte doesn't decode to a valid command, then this can't a message boundary.
    if (!command_byte.has_value()) {
      continue;
    }

    // We can constrain the expected lengths, by command type.
    auto length_range = mysql::kMySQLCommandLengths[static_cast<int>(command_byte.value())];
    if (packet_length < length_range.min || packet_length > length_range.max) {
      continue;
    }

    return i;
  }

  return std::string::npos;
}

}  // namespace stirling
}  // namespace pl
