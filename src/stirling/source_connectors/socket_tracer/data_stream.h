#pragma once

#include <algorithm>
#include <deque>
#include <map>
#include <memory>
#include <string>

#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/data_stream_buffer.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/types.h"

DECLARE_uint32(messages_expiration_duration_secs);
DECLARE_uint32(messages_size_limit_bytes);

namespace pl {
namespace stirling {

/**
 * DataStream is an object that contains the captured data of either send or recv traffic
 * on a connection.
 *
 * Each DataStream contains a container of raw events, and a container of parsed events.
 * Since events are not aligned in any way, may contain only partial messages.
 * Events stay in the raw container until whole messages are parsed out and placed in the
 * container of parsed messaged.
 */
class DataStream : NotCopyMoveable {
 public:
  /**
   * Adds a raw (unparsed) chunk of data into the stream.
   */
  void AddData(std::unique_ptr<SocketDataEvent> event);

  /**
   * Parses as many messages as it can from the raw events into the messages container.
   * @tparam TFrameType The parsed message type within the deque.
   * @param type whether to parse as requests, responses or mixed traffic.
   * @return deque of parsed messages.
   */
  template <typename TFrameType>
  void ProcessBytesToFrames(MessageType type);

  /**
   * Returns the current set of parsed frames.
   * @tparam TFrameType The parsed frame type within the deque.
   * @return deque of frames.
   */
  template <typename TFrameType>
  std::deque<TFrameType>& Frames() {
    DCHECK(std::holds_alternative<std::monostate>(frames_) ||
           std::holds_alternative<std::deque<TFrameType>>(frames_))
        << absl::Substitute(
               "Must hold the default std::monostate, or the same type as requested. "
               "I.e., ConnTracker cannot change the type it holds during runtime. $0 -> $1",
               frames_.index(), typeid(TFrameType).name());
    if (std::holds_alternative<std::monostate>(frames_)) {
      // Reset the type to the expected type.
      frames_ = std::deque<TFrameType>();
      LOG_IF(ERROR, frames_.valueless_by_exception())
          << absl::Substitute("valueless_by_exception() triggered by initializing to type: $0",
                              typeid(TFrameType).name());
    }
    LOG_IF(ERROR, frames_.valueless_by_exception()) << absl::Substitute(
        "valueless_by_exception() triggered by type: $0", typeid(TFrameType).name());
    return std::get<std::deque<TFrameType>>(frames_);
  }

  template <typename TFrameType>
  const std::deque<TFrameType>& Frames() const {
    DCHECK(std::holds_alternative<std::deque<TFrameType>>(frames_)) << absl::Substitute(
        "Must hold the same type as requested. "
        "I.e., ConnTracker cannot change the type it holds during runtime. $0 -> $1",
        frames_.index(), typeid(TFrameType).name());
    return std::get<std::deque<TFrameType>>(frames_);
  }

  /**
   * Clears all unparsed and parsed data from the Datastream.
   */
  void Reset();

  /**
   * Checks if the DataStream is empty of both raw events and parsed messages.
   * @return true if empty of all data.
   */
  template <typename TFrameType>
  bool Empty() const {
    return data_buffer_.empty() && (std::holds_alternative<std::monostate>(frames_) ||
                                    std::get<std::deque<TFrameType>>(frames_).empty());
  }

  /**
   * Checks if the DataStream is in a Stuck state, which means that it has
   * raw events with no missing events, but that it cannot parse anything.
   *
   * @return true if DataStream is stuck.
   */
  bool IsStuck() const {
    constexpr int kMaxStuckCount = 3;
    return stuck_count_ > kMaxStuckCount;
  }

  int stat_invalid_frames() const { return stat_invalid_frames_; }
  int stat_valid_frames() const { return stat_valid_frames_; }
  int stat_raw_data_gaps() const { return stat_raw_data_gaps_; }

  /**
   * Fraction of frame parsing attempts that resulted in an invalid frame.
   *
   * Frame parsing attempts is defined as number of frames extracted,
   * not as number of calls to ParseFrames().
   */
  double ParseFailureRate() const {
    int total_attempts = stat_invalid_frames_ + stat_valid_frames_;

    // Don't report rates until there some meaningful amount of events.
    // - Avoids division by zero.
    // - Avoids caller making decisions based on too little data.
    if (total_attempts <= 5) {
      return 0.0;
    }

    return 1.0 * stat_invalid_frames_ / total_attempts;
  }

  /**
   * Checks if the DataStream is at end-of-stream (EOS), which means that we
   * should stop processing the data on the stream, even if more exists.
   *
   * One use case is for HTTP connection upgrades. We want to stop monitoring the
   * connection after the upgrade, since we don't understand the new protocol.
   *
   * @return true if DataStream parsing is at EOS.
   */
  bool IsEOS() const { return last_parse_state_ == ParseState::kEOS; }

  /**
   * Cleanup frames that are parsed from the BPF events, when the condition is right.
   */
  template <typename TFrameType>
  void CleanupFrames() {
    size_t size = 0;
    // TODO(yzhao): Consider put the size computation into a member function of DataStream.
    for (const auto& msg : Frames<TFrameType>()) {
      size += msg.ByteSize();
    }
    if (size > FLAGS_messages_size_limit_bytes) {
      LOG(WARNING) << absl::Substitute(
          "Messages are cleared, because their size $0 is larger than the specified limit $1.",
          size, FLAGS_messages_size_limit_bytes);
      Frames<TFrameType>().clear();
    }
    EraseExpiredFrames(std::chrono::seconds(FLAGS_messages_expiration_duration_secs),
                       &Frames<TFrameType>());
  }

  /**
   * Cleanup BPF events that are not able to be be processed.
   */
  bool CleanupEvents() {
    if (IsStuck()) {
      // We are assuming that when this stream is stuck, the messages previously parsed are unlikely
      // to be useful, as they are even older than the events being purged now.
      Reset();
      return true;
    }

    return false;
  }

  const protocols::DataStreamBuffer& data_buffer() const { return data_buffer_; }

 private:
  template <typename TFrameType>
  static void EraseExpiredFrames(std::chrono::seconds exp_dur, std::deque<TFrameType>* frames) {
    auto now = std::chrono::steady_clock::now();

    auto iter = frames->begin();
    for (; iter != frames->end(); ++iter) {
      auto frame_timestamp = std::chrono::time_point<std::chrono::steady_clock>(
          std::chrono::nanoseconds(iter->timestamp_ns));
      auto frame_age = std::chrono::duration_cast<std::chrono::seconds>(now - frame_timestamp);
      // As messages are put into the list with monotonically increasing creation time stamp,
      // we can just stop at the first frame that is younger than the expiration duration.
      //
      // TODO(yzhao): Benchmark with binary search and pick the faster one.
      if (frame_age < exp_dur) {
        break;
      }
    }
    frames->erase(frames->begin(), iter);
  }

  // Raw data events from BPF.
  protocols::DataStreamBuffer data_buffer_;

  // Vector of parsed HTTP/MySQL messages.
  // Once parsed, the raw data events should be discarded.
  // std::variant adds 8 bytes of overhead (to 80->88 for deque)
  //
  // std::variant<> default constructs with the first type parameter. So by default,
  // std::get<> will succeed only for the first type variant, if the variant has not been
  // initialized after definition.
  //
  // Additionally, ConnTracker must not switch type during runtime, which indicates serious
  // bug, so we add std::monostate as the default type. And switch to the right time in runtime.
  protocols::FrameDequeVariant frames_;

  // The following state keeps track of whether the raw events were touched or not since the last
  // call to ProcessBytesToFrames(). It enables ProcessToRecords() to exit early if nothing has
  // changed.
  bool has_new_events_ = false;

  // Number of consecutive calls to ProcessToRecords(), where there are a non-zero number of events,
  // but no parsed messages are produced.
  // Note: unlike the monotonic stats below, this resets when the stuck condition is cleared.
  // Thus it is a state, not a statistic.
  int stuck_count_ = 0;

  // Keep some stats on ParseFrames() attempts.
  int stat_valid_frames_ = 0;
  int stat_invalid_frames_ = 0;
  int stat_raw_data_gaps_ = 0;

  // A copy of the parse state from the last call to ProcessToRecords().
  ParseState last_parse_state_ = ParseState::kInvalid;

  template <typename TFrameType>
  friend std::string DebugString(const DataStream& d, std::string_view prefix);
};

// Note: can't make DebugString a class member because of GCC restrictions.

template <typename TFrameType>
inline std::string DebugString(const DataStream& d, std::string_view prefix) {
  std::string info;
  info += absl::Substitute("$0raw event bytes=$1\n", prefix, d.data_buffer_.size());
  int frames_size;
  if (std::holds_alternative<std::deque<TFrameType>>(d.frames_)) {
    frames_size = std::get<std::deque<TFrameType>>(d.frames_).size();
  } else if (std::holds_alternative<std::monostate>(d.frames_)) {
    frames_size = 0;
  } else {
    frames_size = -1;
    LOG(DFATAL) << "Bad variant access";
  }
  info += absl::Substitute("$0parsed frames=$1\n", prefix, frames_size);
  return info;
}

}  // namespace stirling
}  // namespace pl
