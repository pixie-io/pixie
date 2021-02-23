#include "src/stirling/source_connectors/socket_tracer/protocols/http2/stitcher.h"

#include <utility>

namespace pl {
namespace stirling {
namespace protocols {
namespace http2 {

void ProcessHTTP2Streams(std::deque<http2::Stream>* http2_streams,
                         uint32_t* oldest_active_stream_id_ptr,
                         RecordsWithErrorCount<http2::Record>* result) {
  int count_head_consumed = 0;
  bool skipped = false;
  for (auto& stream : *http2_streams) {
    if (!stream.consumed && stream.StreamEnded()) {
      // TODO(oazizi): Investigate ways of using std::move(stream) for performance.
      //               But be careful since the object may still be accessed after the move
      //               (see HalfStreamPtr in connection_tracker.cc).
      result->records.emplace_back(http2::Record{stream});
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
