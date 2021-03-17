#pragma once

#include <deque>
#include <vector>

#include "src/stirling/source_connectors/socket_tracer/protocols/http2/http2_streams_container.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/types.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace http2 {

void ProcessHTTP2Streams(HTTP2StreamsContainer* http2_streams,
                         RecordsWithErrorCount<http2::Record>* result);

}
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
