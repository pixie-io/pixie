#pragma once

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "src/stirling/common/parse_state.h"
#include "src/stirling/common/protocol_traits.h"
#include "src/stirling/http/types.h"

namespace pl {
namespace stirling {
namespace http {

/**
 * ProcessMessages is the entry point of the HTTP Stitcher. It loops through the resp_packets,
 * matches them with the corresponding req_packets, and optionally produces an entry to emit.
 *
 * @param req_messages: deque of all request messages.
 * @param resp_messages: deque of all response messages.
 * @return A vector of entries to be appended to table store.
 */
std::vector<Record> ProcessMessages(std::deque<Message>* req_messages,
                                    std::deque<Message>* resp_messages);

void PreProcessMessage(Message* message);

}  // namespace http

inline std::vector<http::Record> ProcessFrames(std::deque<http::Message>* req_messages,
                                               std::deque<http::Message>* resp_messages,
                                               NoState* /* state */) {
  return http::ProcessMessages(req_messages, resp_messages);
}

}  // namespace stirling
}  // namespace pl
