#pragma once

#include <deque>
#include <string>
#include <vector>

#include "src/stirling/common/event_parser.h"
#include "src/stirling/dns/types.h"

namespace pl {
namespace stirling {

/**
 * Parses the input string as a DNS protocol frame.
 */
template <>
ParseState ParseFrame(MessageType type, std::string_view* buf, dns::Frame* frame);

template <>
size_t FindFrameBoundary<dns::Frame>(MessageType type, std::string_view buf, size_t start_pos);

}  // namespace stirling
}  // namespace pl
