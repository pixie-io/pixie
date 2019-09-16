#pragma once

#include <deque>
#include <string>
#include <vector>
#include "src/stirling/connection_tracker.h"
#include "src/stirling/event_parser.h"
#include "src/stirling/mysql/mysql.h"

namespace pl {
namespace stirling {

/**
 * @brief Parses the input string as a sequence of MySQL responses, writes the messages in result.
 *
 * @return ParseState To indicate the final state of the parsing. The second return value is the
 * bytes count of the parsed data.
 */
template <>
ParseResult<size_t> Parse(MessageType type, std::string_view buf,
                          std::deque<mysql::Packet>* messages);

template <>
size_t FindMessageBoundary<mysql::Packet>(MessageType type, std::string_view buf, size_t start_pos);

}  // namespace stirling
}  // namespace pl
