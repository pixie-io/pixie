#pragma once

#include <deque>

#include "src/stirling/protocols/common/event_parser.h"
#include "src/stirling/protocols/http2/http2.h"

namespace pl {
namespace stirling {
namespace protocols {

/**
 * Unpacks a single HTTP2 frame from the input string.
 */
template <>
ParseState ParseFrame(MessageType type, std::string_view* buf, http2::Frame* frame);

template <>
size_t FindFrameBoundary<http2::Frame>(MessageType type, std::string_view buf, size_t start_pos);

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
