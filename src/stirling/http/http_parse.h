#pragma once

#include <deque>
#include <string>

#include "src/stirling/common/event_parser.h"
#include "src/stirling/http/types.h"

namespace pl {
namespace stirling {

/**
 * Parses a single HTTP message from the input string.
 */
template <>
ParseState ParseFrame(MessageType type, std::string_view* buf, http::Message* frame);

template <>
size_t FindFrameBoundary<http::Message>(MessageType type, std::string_view buf, size_t start_pos);

}  // namespace stirling
}  // namespace pl
