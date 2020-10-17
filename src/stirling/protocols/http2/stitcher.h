#pragma once

#include <deque>
#include <vector>

#include "src/stirling/protocols/http2/types.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace http2u {

void ProcessHTTP2Streams(std::deque<http2u::Stream>* http2_streams,
                         uint32_t* oldest_active_stream_id_ptr,
                         std::vector<http2u::Record>* trace_records);

}
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
