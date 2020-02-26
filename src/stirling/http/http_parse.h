#pragma once

#include <deque>
#include <string>

#include "src/stirling/common/event_parser.h"
#include "src/stirling/http/http_types.h"

namespace pl {
namespace stirling {

/**
 * @brief Parses the input string as a sequence of HTTP responses, writes the messages in result.
 *
 * @return ParseState To indicate the final state of the parsing. The second return value is the
 * bytes count of the parsed data.
 */
template <>
ParseResult<size_t> ParseFrame(MessageType type, std::string_view buf,
                               std::deque<http::Message>* messages);

template <>
size_t FindFrameBoundary<http::Message>(MessageType type, std::string_view buf, size_t start_pos);

}  // namespace stirling
}  // namespace pl
