#pragma once

#include <deque>
#include <string>
#include <vector>
#include "src/stirling/connection_tracker.h"
#include "src/stirling/event_parser.h"

namespace pl {
namespace stirling {

struct MySQLMessage {
  uint64_t timestamp_ns;
  std::string msg;
  MySQLEventType type = MySQLEventType::kMySQLUnknown;
};

struct MySQLParser {
  ParseState Parse(MessageType type, std::string_view buf) {
    switch (type) {
      case MessageType::kRequests:
        return ParseRequest(buf);
      case MessageType::kResponses:
        return ParseState::kInvalid;
      default:
        return ParseState::kInvalid;
    }
  }
  ParseState Write(MessageType type, MySQLMessage* result) {
    switch (type) {
      case MessageType::kRequests:
        return WriteRequest(result);
      case MessageType::kResponses:
        return ParseState::kInvalid;
      default:
        return ParseState::kUnknown;
    }
  }
  ParseState ParseRequest(std::string_view buf);
  ParseState WriteRequest(MySQLMessage* result);
  // TODO(chengruizhe): Add mysql response parsing
  // ParseState ParseResponse(std::string_view buf);
  // ParseState WriteResponse(MySQLMessage* result);

  std::string_view unparsed_data;
  inline static constexpr ConstStrView kComStmtPrepare = "\x16";
  inline static constexpr ConstStrView kComStmtExecute = "\x17";
  inline static constexpr ConstStrView kComQuery = "\x03";

 private:
  std::string_view curr_msg_;
  MySQLEventType curr_type_;
};

/**
 * @brief Parses the input string as a sequence of MySQL responses, writes the messages in result.
 *
 * @return ParseState To indicate the final state of the parsing. The second return value is the
 * bytes count of the parsed data.
 */
ParseResult<size_t> Parse(MessageType type, std::string_view buf,
                          std::deque<MySQLMessage>* messages);
}  // namespace stirling
}  // namespace pl
