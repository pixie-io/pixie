#include "src/stirling/source_connectors/socket_tracer/http2_streams_container.h"

#include <algorithm>

DECLARE_uint32(messages_expiration_duration_secs);
DECLARE_uint32(messages_size_limit_bytes);

DEFINE_uint32(stirling_http2_stream_id_gap_threshold, 100,
              "If a stream ID jumps by this many spots or more, an error is assumed and the entire "
              "connection info is cleared.");

namespace pl {
namespace stirling {

namespace {
void EraseExpiredStreams(std::chrono::seconds exp_dur,
                         std::deque<protocols::http2::Stream>* streams) {
  auto iter = streams->begin();
  for (; iter != streams->end(); ++iter) {
    uint64_t timestamp_ns = std::max(iter->send.timestamp_ns, iter->recv.timestamp_ns);
    auto last_activity =
        std::chrono::time_point<std::chrono::steady_clock>(std::chrono::nanoseconds(timestamp_ns));
    auto now = std::chrono::steady_clock::now();
    auto stream_age = std::chrono::duration_cast<std::chrono::seconds>(now - last_activity);
    if (stream_age < exp_dur) {
      break;
    }
  }
  streams->erase(streams->begin(), iter);
}
}  // namespace

// TODO(oazizi/yzhao): Consider making the flags as inputs, so the flags can be controlled
//                     at the ConnTracker level.
void HTTP2StreamsContainer::Cleanup() {
  // TODO(yzhao): Consider put the size computation into a member function of
  // protocols::http2::Stream.
  size_t size = 0;
  for (const auto& stream : streams_) {
    size += stream.ByteSize();
  }

  if (size > FLAGS_messages_size_limit_bytes) {
    LOG_FIRST_N(WARNING, 10) << absl::Substitute(
        "HTTP2 Streams were cleared, because their size $0 is larger than the specified limit "
        "$1.",
        size, FLAGS_messages_size_limit_bytes);
    streams_.clear();
  }

  EraseExpiredStreams(std::chrono::seconds(FLAGS_messages_expiration_duration_secs), &streams_);
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
}  // namespace pl
