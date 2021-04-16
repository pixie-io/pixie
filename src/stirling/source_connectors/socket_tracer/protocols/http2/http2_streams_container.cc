#include "src/stirling/source_connectors/socket_tracer/protocols/http2/http2_streams_container.h"

#include <algorithm>

DEFINE_uint32(stirling_http2_stream_id_gap_threshold, 100,
              "If a stream ID jumps by this many spots or more, an error is assumed and the entire "
              "connection info is cleared.");

namespace px {
namespace stirling {

namespace {
// According to the HTTP2 protocol, Stream IDs are incremented by 2.
// Client-initiated streams use odd IDs, while server-initiated streams use even IDs.
static constexpr int kHTTP2StreamIDIncrement = 2;

// TODO(oazizi): This has been observed to take a significant amount of Stirling time in some cases.
//               In particular, walking the deque seems to be a problem.
//               Needs to be investigated and optimized.
void EraseExpiredStreams(std::chrono::seconds exp_dur,
                         std::deque<protocols::http2::Stream>* streams) {
  auto now = std::chrono::steady_clock::now();

  auto iter = streams->begin();
  for (; iter != streams->end(); ++iter) {
    uint64_t timestamp_ns = std::max(iter->send.timestamp_ns, iter->recv.timestamp_ns);
    auto last_activity =
        std::chrono::time_point<std::chrono::steady_clock>(std::chrono::nanoseconds(timestamp_ns));

    auto stream_age = std::chrono::duration_cast<std::chrono::seconds>(now - last_activity);
    if (stream_age < exp_dur) {
      break;
    }
  }
  streams->erase(streams->begin(), iter);
}
}  // namespace

size_t HTTP2StreamsContainer::StreamsSize() {
  size_t size = 0;
  for (const auto& stream : streams_) {
    size += stream.ByteSize();
  }
  return size;
}

void HTTP2StreamsContainer::Cleanup(size_t size_limit_bytes, int expiration_duration_secs) {
  size_t size = StreamsSize();
  if (size > size_limit_bytes) {
    VLOG(1) << absl::Substitute("HTTP2 streams cleared due to size limit ($0 > $1).", size,
                                size_limit_bytes);
    streams_.clear();
  }

  EraseExpiredStreams(std::chrono::seconds(expiration_duration_secs), &streams_);
}

void HTTP2StreamsContainer::EraseHead(size_t n) {
  streams_.erase(streams_.begin(), streams_.begin() + n);
  oldest_active_stream_id_ += kHTTP2StreamIDIncrement * n;
}

protocols::http2::HalfStream* HTTP2StreamsContainer::HalfStreamPtr(uint32_t stream_id,
                                                                   bool write_event) {
  // Now we have to figure out where the stream_id is located in streams_, as an index.
  // This may also require growing the deque.
  // There are 3 cases:
  //  1) Deque is empty.
  //      - Initialize the deque to size 1 and set index to 0.
  //  2) StreamID precedes the oldest event in the deque.
  //      - Grow the deque backwards.
  //  3) StreamID follows the oldest event in the deque.
  //      - Grow the deque forwards (if required).

  size_t index = 0;
  if (streams_.empty()) {
    streams_.resize(1);
    index = 0;
    oldest_active_stream_id_ = stream_id;
  } else if (stream_id < oldest_active_stream_id_) {
    // Need to grow the deque at the front.
    size_t new_entries_count = (oldest_active_stream_id_ - stream_id) / kHTTP2StreamIDIncrement;

    // If going too far backwards, something is likely wrong. Reset everything for now.
    if (new_entries_count > FLAGS_stirling_http2_stream_id_gap_threshold) {
      VLOG(1) << absl::Substitute(
          "Encountered a stream ID $0 that is too far from the last known stream ID $1. Resetting "
          "all streams on this connection.",
          stream_id, oldest_active_stream_id_ + streams_.size() * 2);
      streams_.clear();
      streams_.resize(1);
      index = 0;
      oldest_active_stream_id_ = stream_id;
    } else {
      streams_.insert(streams_.begin(), new_entries_count, protocols::http2::Stream());
      index = 0;
      oldest_active_stream_id_ = stream_id;
    }
  } else {
    // Stream ID is after the front. We may or may not need to grow the deque,
    // depending on its current size.
    index = (stream_id - oldest_active_stream_id_) / kHTTP2StreamIDIncrement;
    size_t new_size = std::max(streams_.size(), index + 1);

    // If we are to grow by more than some threshold, then something appears wrong.
    // Reset everything for now.
    if (new_size - streams_.size() > FLAGS_stirling_http2_stream_id_gap_threshold) {
      VLOG(1) << absl::Substitute(
          "Encountered a stream ID $0 that is too far from the last known stream ID $1. Resetting "
          "all streams on this connection",
          stream_id, oldest_active_stream_id_ + streams_.size() * 2);
      streams_.clear();
      streams_.resize(1);
      index = 0;
      oldest_active_stream_id_ = stream_id;
    } else {
      streams_.resize(new_size);
    }
  }

  auto& stream = streams_[index];

  if (stream.consumed) {
    // Don't expect this to happen, but log it just in case.
    // If http2/stitcher.cc uses std::move on consumption, this would indicate an issue.
    VLOG(1) << "Trying to access a consumed stream.";
  }

  protocols::http2::HalfStream* half_stream_ptr = write_event ? &stream.send : &stream.recv;
  return half_stream_ptr;
}

}  // namespace stirling
}  // namespace px
