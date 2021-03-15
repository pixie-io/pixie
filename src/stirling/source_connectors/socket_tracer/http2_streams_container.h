#pragma once

#include <deque>
#include <string>

#include "src/common/base/mixins.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/types.h"

namespace pl {
namespace stirling {

/**
 * HTTP2StreamsContainer is an object that holds the captured HTTP2 stream data from BPF.
 * This is managed differently from other protocols because it comes as UProbe data
 * and is already structured. This in contrast to other protocols which are captured via
 * KProbes and need to be parsed.
 */
class HTTP2StreamsContainer : NotCopyMoveable {
 public:
  std::deque<protocols::http2::Stream>& streams() { return streams_; }
  const std::deque<protocols::http2::Stream>& streams() const { return streams_; }

  /**
   * Cleans up the HTTP2 events from BPF uprobes that are too old,
   * either because they are too far back in time, or too far back in bytes.
   */
  void Cleanup();

  std::string DebugString(std::string_view prefix) const {
    std::string info;
    info += absl::Substitute("$0active streams=$1\n", prefix, streams_.size());
    return info;
  }

 private:
  std::deque<protocols::http2::Stream> streams_;
};

}  // namespace stirling
}  // namespace pl
