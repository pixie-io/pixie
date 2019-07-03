#include "src/stirling/mysql_parse.h"
#include <deque>
#include <string>
#include <string_view>
#include <utility>
#include "src/stirling/event_parser.h"

namespace {
template <size_t N>
void EndianSwap(const char bytes[N], char result[N]) {
  if (N == 0) {
    return;
  }
  for (size_t k = 0; k < N - 1; k++) {
    result[k] = bytes[N - k - 2];
  }
  result[N - 1] = '\x00';
}

int charArrToInt(const char arr[], int size) {
  int result = 0;
  for (int i = 0; i < size; i++) {
    result = arr[i] + (result << 4);
  }
  return result;
}
}  // namespace

namespace pl {
namespace stirling {
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

MySQLEventType infer_mysql_protocol(std::string_view buf) {
  if (buf.substr(0, 7) == COM_STMT_PREPARE + "SELECT") {
    return MySQLEventType::kMySQLComStmtPrepare;
  } else if (buf[0] == COM_STMT_EXECUTE[0]) {
    return MySQLEventType::kMySQLComStmtExecute;
  } else if (buf.substr(0, 7) == COM_QUERY + "3SELECT") {
    return MySQLEventType::kMySQLComQuery;
  } else {
    return MySQLEventType::kMySQLUnknown;
  }
}

ParseState MySQLParser::ParseRequest(std::string_view buf) {
  if (buf.size() < 5) {
    return ParseState::kInvalid;
  }
  MySQLEventType type = infer_mysql_protocol(buf.substr(4));
  if (type == MySQLEventType::kMySQLUnknown) {
    return ParseState::kInvalid;
  }
  char len_char_BE[] = {buf[0], buf[1], buf[2], '\x00'};
  char len_char_LE[4];
  EndianSwap<4>(len_char_BE, len_char_LE);
  int packet_length = charArrToInt(len_char_LE, 3);
  int buffer_length = buf.length();

  // 3 bytes of packet length and 1 byte of packet number
  if (buffer_length < 4 + packet_length) {
    return ParseState::kNeedsMoreData;
  }
  curr_msg_ = buf.substr(4, packet_length);
  unparsed_data = buf.substr(4 + packet_length);
  curr_type_ = type;
  return ParseState::kSuccess;
}

ParseState MySQLParser::WriteRequest(MySQLMessage* result) {
  result->type = curr_type_;
  result->msg = curr_msg_;
  return ParseState::kSuccess;
}

}  // namespace stirling
}  // namespace pl
