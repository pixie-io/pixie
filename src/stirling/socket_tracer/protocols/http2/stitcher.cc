#include "src/stirling/socket_tracer/protocols/http2/stitcher.h"

#include <utility>

namespace pl {
namespace stirling {
namespace protocols {
namespace http2 {

void ProcessHTTP2Streams(std::deque<http2::Stream>* http2_streams,
                         uint32_t* oldest_active_stream_id_ptr,
                         std::vector<http2::Record>* trace_records) {
  int count_head_consumed = 0;
  bool skipped = false;
  for (auto& stream : *http2_streams) {
    if (stream.StreamEnded() && !stream.consumed) {
      trace_records->emplace_back(http2::Record{std::move(stream)});
      stream.consumed = true;
    }

    // TODO(oazizi): If a stream is not ended, but looks stuck,
    // we should force process it and mark it as consumed.
    // Otherwise we will have a memory leak.

    if (!stream.consumed) {
      skipped = true;
    }

    if (!skipped && stream.consumed) {
      ++count_head_consumed;
    }
  }

  // Erase contiguous set of consumed streams at head.
  http2_streams->erase(http2_streams->begin(), http2_streams->begin() + count_head_consumed);
  *oldest_active_stream_id_ptr += 2 * count_head_consumed;
}

}  // namespace http2
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
