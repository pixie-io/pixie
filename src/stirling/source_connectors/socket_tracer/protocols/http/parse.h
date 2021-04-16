#pragma once

#include <deque>
#include <string>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http/types.h"

namespace px {
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
}  // namespace px
