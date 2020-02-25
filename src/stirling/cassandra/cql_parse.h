#pragma once

#include <deque>
#include <string>
#include <vector>

#include "src/stirling/cassandra/cass_types.h"
#include "src/stirling/common/event_parser.h"

namespace pl {
namespace stirling {

/**
 * Parses the input string as a sequence of CQL binary protocol frames.
 *
 * @param type Whether to process bytes as requests or responses.
 * @param buf The raw data to be parsed.
 * @param messages The deque to which parsed messages are appended.
 *
 * @return ParseState Indicates the final state of the parsing, and where the parsing stopped.
 */
template <>
ParseResult<size_t> ParseFrame(MessageType type, std::string_view buf,
                               std::deque<cass::Frame>* messages);

template <>
size_t FindFrameBoundary<cass::Frame>(MessageType type, std::string_view buf, size_t start_pos);

}  // namespace stirling
}  // namespace pl
