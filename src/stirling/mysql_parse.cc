#include "src/stirling/mysql_parse.h"
#include <arpa/inet.h>
#include <deque>
#include <string>
#include <string_view>
#include <utility>
#include "src/stirling/event_parser.h"
#include "src/stirling/utils.h"

namespace pl {
namespace stirling {
namespace {
MySQLEventType infer_mysql_event_type(std::string_view buf) {
  std::string_view command = buf.substr(0, 1);
  if (command == MySQLParser::kComStmtPrepare) {
    return MySQLEventType::kMySQLComStmtPrepare;
  } else if (command == MySQLParser::kComStmtExecute) {
    return MySQLEventType::kMySQLComStmtExecute;
  } else if (command == MySQLParser::kComQuery) {
    return MySQLEventType::kMySQLComQuery;
  } else {
    return MySQLEventType::kMySQLUnknown;
  }
}
}  // namespace

// TODO(chengruizhe): Could be templatized with HTTP Parser
ParseResult<size_t> Parse(MessageType type, std::string_view buf,
                          std::deque<MySQLMessage>* messages) {
  MySQLParser parser;
  std::vector<size_t> start_position;
  const size_t buf_size = buf.size();
  ParseState s = ParseState::kSuccess;
  size_t bytes_prcocessed = 0;

  while (!buf.empty()) {
    s = parser.Parse(type, buf);
    if (s != ParseState::kSuccess) {
      break;
    }

    MySQLMessage message;
    s = parser.Write(type, &message);
    if (s != ParseState::kSuccess) {
      break;
    }
    start_position.push_back(bytes_prcocessed);
    messages->push_back(std::move(message));
    buf = parser.unparsed_data;
    bytes_prcocessed = (buf_size - buf.size());
  }
  ParseResult<size_t> result{std::move(start_position), bytes_prcocessed, s};
  return result;
}

ParseState MySQLParser::Parse(MessageType type, std::string_view buf) {
  if (type != MessageType::kRequests && type != MessageType::kResponses) {
    return ParseState::kInvalid;
  }

  if (buf.size() <= kPacketHeaderLength) {
    return ParseState::kInvalid;
  }
  if (type == MessageType::kRequests) {
    MySQLEventType event_type = infer_mysql_event_type(buf.substr(4));
    if (event_type == MySQLEventType::kMySQLUnknown) {
      return ParseState::kInvalid;
    }
    curr_type_ = event_type;
  }

  char len_char_LE[] = {buf[0], buf[1], buf[2]};
  char len_char_BE[3];
  EndianSwap<3>(len_char_LE, len_char_BE);
  int packet_length = BEBytesToInt(len_char_BE, 3);
  int buffer_length = buf.length();

  // 3 bytes of packet length and 1 byte of packet number.
  if (buffer_length < kPacketHeaderLength + packet_length) {
    return ParseState::kNeedsMoreData;
  }
  curr_msg_ = buf.substr(kPacketHeaderLength, packet_length);
  unparsed_data = buf.substr(kPacketHeaderLength + packet_length);
  return ParseState::kSuccess;
}

ParseState MySQLParser::WriteRequest(MySQLMessage* result) {
  result->type = curr_type_;
  result->msg = curr_msg_;
  return ParseState::kSuccess;
}

ParseState MySQLParser::WriteResponse(MySQLMessage* result) {
  result->msg = curr_msg_;
  return ParseState::kSuccess;
}

}  // namespace stirling
}  // namespace pl
