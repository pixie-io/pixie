#pragma once

#include <deque>
#include <vector>

#include "src/stirling/source_connectors/socket_tracer/protocols/http2/types.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace http2 {

void ProcessHTTP2Streams(std::deque<http2::Stream>* http2_streams,
                         uint32_t* oldest_active_stream_id_ptr,
                         RecordsWithErrorCount<http2::Record>* result);

}
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
