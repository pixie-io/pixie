#include "src/stirling/source_connectors/socket_tracer/http2_streams_container.h"

#include <algorithm>

DECLARE_uint32(messages_expiration_duration_secs);
DECLARE_uint32(messages_size_limit_bytes);

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

}  // namespace stirling
}  // namespace pl
