#pragma once

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/timestamp_stitcher.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http/types.h"

DECLARE_string(http_response_header_filters);

namespace pl {
namespace stirling {
namespace protocols {
namespace http {

/**
 * ProcessMessages is the entry point of the HTTP Stitcher. It loops through the resp_packets,
 * matches them with the corresponding req_packets, and optionally produces an entry to emit.
 *
 * @param req_messages: deque of all request messages.
 * @param resp_messages: deque of all response messages.
 * @return A vector of entries to be appended to table store.
 */
RecordsWithErrorCount<Record> ProcessMessages(std::deque<Message>* req_messages,
                                              std::deque<Message>* resp_messages);

void PreProcessMessage(Message* message);

}  // namespace http

template <>
inline RecordsWithErrorCount<http::Record> StitchFrames(std::deque<http::Message>* req_messages,
                                                        std::deque<http::Message>* resp_messages,
                                                        NoState* /* state */) {
  // NOTE: This cannot handle HTTP pipelining if there is any missing message.
  return StitchMessagesWithTimestampOrder<http::Record>(req_messages, resp_messages);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
