#pragma once

#include <deque>

#include "src/stirling/common/event_parser.h"
#include "src/stirling/http2/http2.h"

namespace pl {
namespace stirling {

/**
 * @brief Unpacks the buf as HTTP2 frames. The results are put into messages.
 * The parameter type is not used, but is required to matches the function used by
 * EventParser<std::unique_ptr<Frame>>.
 */
template <>
ParseResult<size_t> ParseFrames(MessageType unused_type, std::string_view buf,
                                std::deque<http2::Frame>* messages);

template <>
size_t FindFrameBoundary<http2::Frame>(MessageType type, std::string_view buf, size_t start_pos);

}  // namespace stirling
}  // namespace pl
