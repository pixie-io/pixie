#pragma once

#include <deque>
#include <map>
#include <memory>
#include <string>

#include "src/stirling/common/socket_trace.h"
#include "src/stirling/cql/types.h"
#include "src/stirling/http/http_parse.h"
#include "src/stirling/http2/http2.h"
#include "src/stirling/http2u/types.h"
#include "src/stirling/mysql/types.h"

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
class DataStream {
 public:
  /**
   * @brief Adds a raw (unparsed) chunk of data into the stream.
   * Uses seq_num inside the SocketDataEvent to determine the sequence spot.
   * @param event The data.
   */
  void AddData(std::unique_ptr<SocketDataEvent> event);

  /**
   * @brief Parses as many messages as it can from the raw events into the messages container.
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
               "I.e., ConnectionTracker cannot change the type it holds during runtime. $0 -> $1",
               frames_.index(), typeid(TFrameType).name());
    if (std::holds_alternative<std::monostate>(frames_)) {
      // Reset the type to the expected type.
      frames_ = std::deque<TFrameType>();
    }
    return std::get<std::deque<TFrameType>>(frames_);
  }

  template <typename TFrameType>
  const std::deque<TFrameType>& Frames() const {
    DCHECK(std::holds_alternative<std::deque<TFrameType>>(frames_))
        << "Must hold the same type as requested.";
    return std::get<std::deque<TFrameType>>(frames_);
  }

  /**
   * Returns the current set of streams (for Uprobe-based HTTP2 only)
   * @return deque of streams.
   */
  std::deque<http2u::Stream>& http2_streams() { return http2_streams_; }
  const std::deque<http2u::Stream>& http2_streams() const { return http2_streams_; }

  /**
   * @brief Clears all unparsed and parsed data from the Datastream.
   */
  void Reset();

  /**
   * @brief Checks if the DataStream is empty of both raw events and parsed messages.
   * @return true if empty of all data.
   */
  template <typename TFrameType>
  bool Empty() const {
    return events_.empty() && (std::holds_alternative<std::monostate>(frames_) ||
                               std::get<std::deque<TFrameType>>(frames_).empty());
  }
  const auto& events() const { return events_; }

  /**
   * @brief Checks if the DataStream is in a Stuck state, which means that it has
   * raw events with no missing events, but that it cannot parse anything.
   *
   * @return true if DataStream is stuck.
   */
  bool IsStuck() const {
    constexpr int kMaxStuckCount = 3;
    return bytes_to_frames_stuck_count_ > kMaxStuckCount;
  }

  /**
   * @brief Checks if the DataStream is at end-of-stream (EOS), which means that we
   * should stop processing the data on the stream, even if more exists.
   *
   * One use case is for HTTP connection upgrades. We want to stop monitoring the
   * connection after the upgrade, since we don't understand the new protocol.
   *
   * @return true if DataStream parsing is at EOS.
   */
  bool IsEOS() const { return last_parse_state_ == ParseState::kEOS; }

  http2::Inflater* HTTP2Inflater() {
    if (inflater_ == nullptr) {
      inflater_ = std::make_unique<http2::Inflater>();
    }
    return inflater_.get();
  }

  /**
   * @brief Cleanup frames that are parsed from the BPF events, when the condition is right.
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
   * @brief Cleanup HTTP2 that are received from the BPF uprobe events, when the condition is right.
   */
  void CleanupHTTP2Streams() {
    // TODO(yzhao): Consider put the size computation into a member function of DataStream.
    size_t size = 0;
    for (const auto& stream : http2_streams()) {
      size += stream.ByteSize();
    }

    if (size > FLAGS_messages_size_limit_bytes) {
      LOG(WARNING) << absl::Substitute(
          "HTTP2 Streams were cleared, because their size $0 is larger than the specified limit "
          "$1.",
          size, FLAGS_messages_size_limit_bytes);
      http2_streams().clear();
    }

    EraseExpiredFrames(std::chrono::seconds(FLAGS_messages_expiration_duration_secs),
                       &http2_streams());
  }

  /**
   * @brief Cleanup BPF events that are not able to be be processed.
   */
  void CleanupEvents() {
    if (IsStuck()) {
      // We are assuming that when this stream is stuck, the messages previously parsed are unlikely
      // to be useful, as they are even older than the events being purged now.
      Reset();
    }
  }

 private:
  template <typename TFrameType>
  static void EraseExpiredFrames(std::chrono::seconds exp_dur, std::deque<TFrameType>* msgs) {
    auto iter = msgs->begin();
    for (; iter != msgs->end(); ++iter) {
      auto frame_age = std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::steady_clock::now() - iter->creation_timestamp);
      // As messages are put into the list with monotonically increasing creation time stamp,
      // we can just stop at the first frame that is younger than the expiration duration.
      //
      // NOTE:
      // http2u::Stream are not appended into the deque. http2u::Stream is created when the first
      // trace event with its stream ID is received. Therefore, their timestamps depend on the
      // order streams are initiated inside application code. As HTTP2 spec forbids reducing stream
      // IDs, it's very unlikely that http2u::Stream would violate the above statement.
      //
      // TODO(yzhao): Benchmark with binary search and pick the faster one.
      if (frame_age < exp_dur) {
        break;
      }
    }
    msgs->erase(msgs->begin(), iter);
  }

  // Helper function that appends all contiguous events to the parser.
  // Returns number of events appended.
  template <typename TFrameType>
  size_t AppendEvents(EventParser<TFrameType>* parser) const;

  // Raw data events from BPF.
  // TODO(oazizi/yzhao): Convert this to vector or deque.
  std::map<size_t, std::unique_ptr<SocketDataEvent>> events_;

  // Keep track of the sequence number of the stream.
  // This is used to identify missing events.
  size_t next_seq_num_ = 0;

  // To support partially processed events,
  // the stream may start at an offset in the first raw data event.
  size_t offset_ = 0;

  // Vector of parsed HTTP/MySQL messages.
  // Once parsed, the raw data events should be discarded.
  // std::variant adds 8 bytes of overhead (to 80->88 for deque)
  //
  // std::variant<> default constructs with the first type parameter. So by default,
  // std::get<> will succeed only for the first type variant, if the variant has not been
  // initialized after definition.
  //
  // Additionally, ConnectionTracker must not switch type during runtime, which indicates serious
  // bug, so we add std::monostate as the default type. And switch to the right time in runtime.
  std::variant<std::monostate, std::deque<http::Message>, std::deque<http2::Frame>,
               std::deque<mysql::Packet>, std::deque<cass::Frame>>
      frames_;

  // Used by Uprobe-based HTTP2 only.
  std::deque<http2u::Stream> http2_streams_;

  // The following state keeps track of whether the raw events were touched or not since the last
  // call to ProcessBytesToFrames(). It enables ProcessToRecords() to exit early if nothing has
  // changed.
  bool has_new_events_ = false;

  // Number of consecutive calls to ProcessToRecords(), where there are a non-zero number of events,
  // but no parsed messages are produced.
  int bytes_to_frames_stuck_count_ = 0;

  // A copy of the parse state from the last call to ProcessToRecords().
  ParseState last_parse_state_ = ParseState::kInvalid;

  // Only meaningful for kprobe HTTP2 tracing. Uprobe tracing extracts plain text header fields from
  // http2 library, therefore does not need to inflate headers.
  //
  // TODO(yzhao): We can put this into a std::variant.
  std::unique_ptr<http2::Inflater> inflater_;

  template <typename TFrameType>
  friend std::string DebugString(const DataStream& d, std::string_view prefix);
};

// Note: can't make DebugString a class member because of GCC restrictions.

template <typename TFrameType>
inline std::string DebugString(const DataStream& d, std::string_view prefix) {
  std::string info;
  info += absl::Substitute("$0raw events=$1\n", prefix, d.events().size());
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

template <>
inline std::string DebugString<http2u::Stream>(const DataStream& d, std::string_view prefix) {
  std::string info;
  info += absl::Substitute("$0active streams=$1\n", prefix, d.http2_streams().size());
  return info;
}

}  // namespace stirling
}  // namespace pl
