#pragma once

#include <deque>
#include <string>
#include <vector>
#include "src/stirling/common/event_parser.h"
#include "src/stirling/mysql/mysql_types.h"

namespace pl {
namespace stirling {

/**
 * Parses the input string as a sequence of MySQL packets.
 *
 * @param type Whether to process bytes as requests or responses.
 * @param buf The raw data to be parsed.
 * @param messages The deque to which parsed messages are appended.
 *
 * @return ParseState Indicates the final state of the parsing, and where the parsing stopped.
 */
template <>
ParseResult<size_t> ParseFrame(MessageType type, std::string_view buf,
                               std::deque<mysql::Packet>* frames);

template <>
size_t FindFrameBoundary<mysql::Packet>(MessageType type, std::string_view buf, size_t start_pos);

}  // namespace stirling
}  // namespace pl
