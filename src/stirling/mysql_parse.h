#pragma once

#include <deque>
#include <string>
#include <vector>
#include "src/stirling/connection_tracker.h"
#include "src/stirling/event_parser.h"
#include "src/stirling/mysql/mysql.h"

namespace pl {
namespace stirling {
namespace mysql {

struct MySQLParser {
  ParseState Parse(MessageType type, std::string_view buf);

  ParseState Write(MessageType type, Packet* result) {
    switch (type) {
      case MessageType::kRequest:
        return WriteRequest(result);
      case MessageType::kResponse:
        return WriteResponse(result);
      default:
        return ParseState::kUnknown;
    }
  }
  ParseState WriteRequest(Packet* result);
  ParseState WriteResponse(Packet* result);

  std::string_view unparsed_data;

  inline static constexpr int kPacketHeaderLength = 4;

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
ParseResult<size_t> Parse(MessageType type, std::string_view buf, std::deque<Packet>* messages);

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
