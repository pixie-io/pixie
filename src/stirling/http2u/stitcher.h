#pragma once

#include <deque>
#include <vector>

#include "src/stirling/http2u/types.h"

namespace pl {
namespace stirling {
namespace http2u {

void ProcessHTTP2Streams(std::deque<http2u::Stream>* http2_streams,
                         uint32_t* oldest_active_stream_id_ptr,
                         std::vector<http2u::Record>* trace_records);

}
}  // namespace stirling
}  // namespace pl
