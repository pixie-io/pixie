#pragma once

#include <deque>
#include <string>
#include <vector>

#include "src/stirling/socket_tracer/protocols/common/interface.h"
#include "src/stirling/socket_tracer/protocols/mysql/types.h"

namespace pl {
namespace stirling {
namespace protocols {

/**
 * Parses a single MySQL packet from the input string.
 */
template <>
ParseState ParseFrame(MessageType type, std::string_view* buf, mysql::Packet* frame);

template <>
size_t FindFrameBoundary<mysql::Packet>(MessageType type, std::string_view buf, size_t start_pos);

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
