#include <arpa/inet.h>
#include <deque>
#include <string>
#include <string_view>
#include <utility>

#include "src/stirling/event_parser.h"
#include "src/stirling/mysql/mysql.h"
#include "src/stirling/mysql_parse.h"
#include "src/stirling/utils/byte_format.h"

namespace pl {
namespace stirling {
namespace mysql {

namespace {
MySQLEventType infer_mysql_event_type(std::string_view buf) {
  char command = buf[0];
  switch (command) {
    case kStmtPreparePrefix:
      return MySQLEventType::kComStmtPrepare;
    case kStmtExecutePrefix:
      return MySQLEventType::kComStmtExecute;
    case kStmtClosePrefix:
      return MySQLEventType::kComStmtClose;
    case kQueryPrefix:
      return MySQLEventType::kComQuery;
    case kSleepPrefix:
      return MySQLEventType::kSleep;
    case kQuitPrefix:
      return MySQLEventType::kQuit;
    case kInitDBPrefix:
      return MySQLEventType::kInitDB;
    case kCreateDBPrefix:
      return MySQLEventType::kCreateDB;
    case kDropDBPrefix:
      return MySQLEventType::kDropDB;
    case kRefreshPrefix:
      return MySQLEventType::kRefresh;
    case kShutdownPrefix:
      return MySQLEventType::kShutdown;
    case kStatisticsPrefix:
      return MySQLEventType::kStatistics;
    case kConnectPrefix:
      return MySQLEventType::kConnect;
    case kProcessKillPrefix:
      return MySQLEventType::kProcessKill;
    case kDebugPrefix:
      return MySQLEventType::kDebug;
    case kPingPrefix:
      return MySQLEventType::kPing;
    case kTimePrefix:
      return MySQLEventType::kTime;
    case kDelayedInsertPrefix:
      return MySQLEventType::kDelayedInsert;
    case kComResetConnectionPrefix:
      return MySQLEventType::kComResetConnection;
    case kDaemonPrefix:
      return MySQLEventType::kDaemon;
  }
  return MySQLEventType::kUnknown;
}
}  // namespace

ParseState MySQLParser::Parse(MessageType type, std::string_view buf) {
  if (type != MessageType::kRequest && type != MessageType::kResponse) {
    return ParseState::kInvalid;
  }

  if (buf.size() <= kPacketHeaderLength) {
    return ParseState::kInvalid;
  }
  if (type == MessageType::kRequest) {
    MySQLEventType event_type = infer_mysql_event_type(buf.substr(4));
    if (event_type == MySQLEventType::kUnknown) {
      return ParseState::kInvalid;
    }
    curr_type_ = event_type;
  }

  int packet_length = utils::LEStrToInt(buf.substr(0, 3));
  int buffer_length = buf.length();

  // 3 bytes of packet length and 1 byte of packet number.
  if (buffer_length < kPacketHeaderLength + packet_length) {
    return ParseState::kNeedsMoreData;
  }

  curr_msg_ = buf.substr(kPacketHeaderLength, packet_length);
  unparsed_data = buf.substr(kPacketHeaderLength + packet_length);
  return ParseState::kSuccess;
}

ParseState MySQLParser::WriteRequest(Packet* result) {
  result->type = curr_type_;
  result->msg = curr_msg_;
  return ParseState::kSuccess;
}

ParseState MySQLParser::WriteResponse(Packet* result) {
  result->msg = curr_msg_;
  return ParseState::kSuccess;
}

}  // namespace mysql

// TODO(chengruizhe): Could be templatized with HTTP Parser
template <>
ParseResult<size_t> Parse(MessageType type, std::string_view buf,
                          std::deque<mysql::Packet>* messages) {
  mysql::MySQLParser parser;
  std::vector<size_t> start_position;
  const size_t buf_size = buf.size();
  ParseState s = ParseState::kSuccess;
  size_t bytes_prcocessed = 0;

  while (!buf.empty()) {
    s = parser.Parse(type, buf);
    if (s != ParseState::kSuccess) {
      break;
    }

    mysql::Packet message;
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

template <>
size_t FindMessageBoundary<mysql::Packet>(MessageType /*type*/, std::string_view /*buf*/,
                                          size_t /*start_pos*/) {
  return 0;
}

}  // namespace stirling
}  // namespace pl
