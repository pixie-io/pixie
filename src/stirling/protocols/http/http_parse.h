#pragma once

#include <deque>
#include <string>

#include "src/stirling/protocols/common/interface.h"
#include "src/stirling/protocols/http/types.h"

namespace pl {
namespace stirling {
namespace protocols {

/**
 * Parses a single HTTP message from the input string.
 */
template <>
ParseState ParseFrame(MessageType type, std::string_view* buf, http::Message* frame);

template <>
size_t FindFrameBoundary<http::Message>(MessageType type, std::string_view buf, size_t start_pos);

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
