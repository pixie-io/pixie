#pragma once

#include <deque>
#include <string>
#include <vector>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/dns/types.h"

namespace pl {
namespace stirling {
namespace protocols {

/**
 * Parses the input string as a DNS protocol frame.
 */
template <>
ParseState ParseFrame(MessageType type, std::string_view* buf, dns::Frame* frame);

template <>
size_t FindFrameBoundary<dns::Frame>(MessageType type, std::string_view buf, size_t start_pos);

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
