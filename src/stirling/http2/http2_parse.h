#pragma once

#include <deque>

#include "src/stirling/common/event_parser.h"
#include "src/stirling/http2/http2.h"

namespace pl {
namespace stirling {

/**
 * Unpacks a single HTTP2 frame from the input string.
 */
template <>
ParseState ParseFrame(MessageType type, std::string_view* buf, http2::Frame* frame);

template <>
size_t FindFrameBoundary<http2::Frame>(MessageType type, std::string_view buf, size_t start_pos);

}  // namespace stirling
}  // namespace pl
