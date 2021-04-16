#pragma once

#include <deque>
#include <vector>

#include "src/stirling/source_connectors/socket_tracer/protocols/http2/http2_streams_container.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace http2 {

// Outputs HTTP2 messages with the same stream ID into req & resp records. If conn_closed is true,
// also outputs messages without END_STREAM flag.
void ProcessHTTP2Streams(HTTP2StreamsContainer* http2_streams, bool conn_closed,
                         RecordsWithErrorCount<http2::Record>* result);

}  // namespace http2
}  // namespace protocols
}  // namespace stirling
}  // namespace px
