#pragma once

#include <deque>
#include <map>
#include <memory>
#include <string>

#include "src/stirling/common/socket_trace.h"
#include "src/stirling/http/http_parse.h"
#include "src/stirling/http2/http2.h"
#include "src/stirling/mysql/mysql.h"

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
  void AddEvent(std::unique_ptr<SocketDataEvent> event);

  /**
   * @brief Parses as many messages as it can from the raw events into the messages container.
   * @tparam TMessageType The parsed message type within the deque.
   * @param type whether to parse as requests, responses or mixed traffic.
   * @return deque of parsed messages.
   */
  template <typename TMessageType>
  std::deque<TMessageType>& ExtractMessages(MessageType type);

  /**
   * Returns the current set of parsed messages.
   * @tparam TMessageType The parsed message type within the deque.
   * @return deque of messages.
   */
  template <typename TMessageType>
  std::deque<TMessageType>& Messages();

  /**
   * @brief Clears all unparsed and parsed data from the Datastream.
   */
  void Reset();

  /**
   * @brief Checks if the DataStream is empty of both raw events and parsed messages.
   * @return true if empty of all data.
   */
  template <typename TMessageType>
  bool Empty() const;

  /**
   * @brief Checks if the DataStream is in a Stuck state, which means that it has
   * raw events with no missing events, but that it cannot parse anything.
   *
   * @return true if DataStream is stuck.
   */
  bool IsStuck() const {
    constexpr int kMaxStuckCount = 3;
    return stuck_count_ > kMaxStuckCount;
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

  http2::Inflater* Inflater() {
    if (inflater_ == nullptr) {
      inflater_ = std::make_unique<http2::Inflater>();
    }
    return inflater_.get();
  }

  /**
   * @brief Cleanup messages that are parsed from the BPF events, when the condition is right.
   */
  template <typename TMessageType>
  void CleanupMessages() {
    size_t size = 0;
    // TODO(yzhao): Consider put the size computation into a member function of DataStream.
    for (const auto& msg : Messages<TMessageType>()) {
      size += msg.ByteSize();
    }
    if (size > FLAGS_messages_size_limit_bytes) {
      LOG(WARNING) << absl::Substitute(
          "Messages are cleared, because their size $0 is larger than the specified limit $1.",
          size, FLAGS_messages_size_limit_bytes);
      Messages<TMessageType>().clear();
    }
    EraseExpiredFrames(std::chrono::seconds(FLAGS_messages_expiration_duration_secs),
                       &Messages<TMessageType>());
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

  template <typename TMessageType>
  std::string DebugString(std::string_view prefix = "") const {
    std::string info;
    info += absl::Substitute("$0raw events=$1\n", prefix, events_.size());
    int messages_size;
    if (std::holds_alternative<std::deque<TMessageType>>(messages_)) {
      messages_size = std::get<std::deque<TMessageType>>(messages_).size();
    } else if (std::holds_alternative<std::monostate>(messages_)) {
      messages_size = 0;
    } else {
      messages_size = -1;
      LOG(DFATAL) << "Bad variant access";
    }
    info += absl::Substitute("$0parsed messages=$1\n", prefix, messages_size);
    return info;
  }

 private:
  template <typename TMessageType>
  static void EraseExpiredFrames(std::chrono::seconds exp_dur, std::deque<TMessageType>* frames) {
    auto iter = frames->begin();
    for (; iter != frames->end(); ++iter) {
      auto frame_age = std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::steady_clock::now() - iter->creation_timestamp);
      // As frames are put into the list with monotonically increasing creation time stamp,
      // we can just stop at the first frame that is younger than the expiration duration.
      //
      // TODO(yzhao): Benchmark with binary search and pick the faster one.
      if (frame_age < exp_dur) {
        break;
      }
    }
    frames->erase(frames->begin(), iter);
  }

  // Helper function that appends all contiguous events to the parser.
  // Returns number of events appended.
  template <typename TMessageType>
  size_t AppendEvents(EventParser<TMessageType>* parser) const;

  // Raw data events from BPF.
  // TODO(oazizi/yzhao): Convert this to vector or deque.
  std::map<size_t, std::unique_ptr<SocketDataEvent>> events_;

  // Keep track of the sequence number of the stream.
  // This is used to identify missing events.
  size_t next_seq_num_ = 0;

  // To support partially processed events,
  // the stream may start at an offset in the first raw data event.
  uint64_t offset_ = 0;

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
  std::variant<std::monostate, std::deque<http::HTTPMessage>, std::deque<http2::Frame>,
               std::deque<mysql::Packet>>
      messages_;

  // The following state keeps track of whether the raw events were touched or not since the last
  // call to ExtractMessages(). It enables ExtractMessages() to exit early if nothing has changed.
  bool has_new_events_ = false;

  // Number of consecutive calls to ExtractMessages(), where there are a non-zero number of events,
  // but no parsed messages are produced.
  int stuck_count_ = 0;

  // A copy of the parse state from the last call to ExtractMessages().
  ParseState last_parse_state_ = ParseState::kInvalid;

  // Only meaningful for HTTP2. See also Inflater().
  // TODO(yzhao): We can put this into a std::variant.
  std::unique_ptr<http2::Inflater> inflater_;
};

}  // namespace stirling
}  // namespace pl
