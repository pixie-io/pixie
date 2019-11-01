#pragma once

#include <deque>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "src/common/base/base.h"
#include "src/stirling/common/parse_state.h"
#include "src/stirling/common/utils.h"

namespace pl {
namespace stirling {

// The direction of traffic expected on a probe.
enum class MessageType { kUnknown, kRequest, kResponse };

struct BufferPosition {
  size_t seq_num;
  size_t offset;
};

// A ParseResult returns a vector of parsed messages, and also some position markers.
//
// It is templated based on the position type, because we have two concepts of position:
//    Position in a contiguous buffer: PositionType is uint64_t.
//    Position in a set of disjoint buffers: PositionType is BufferPosition.
//
// The two concepts are used by two different parse functions we have:
//
// ParseResult<size_t> Parse(MessageType type, std::string_view buf);
// ParseResult<BufferPosition> ParseMessages(MessageType type);
template <typename PositionType>
struct ParseResult {
  // Positions of message start positions in the source buffer.
  std::vector<PositionType> start_positions;
  // Position of where parsing ended consuming the source buffer.
  // When PositionType is bytes, this is total bytes successfully consumed.
  PositionType end_position;
  // State of the last attempted message parse.
  ParseState state = ParseState::kInvalid;
};

// NOTE: FindMessageBoundary() and Parse() must be implemented per protocol.

/**
 * Attempt to find the next message boundary.
 *
 * @tparam TMessageType Message type to search for.
 * @param type request or response.
 * @param buf the buffer in which to search for a message boundary.
 * @param start_pos A start position from which to search.
 * @return Either the position of a message start, if found (must be > start_pos),
 * or std::string::npos if no such message start was found.
 */
template <typename TMessageType>
size_t FindMessageBoundary(MessageType type, std::string_view buf, size_t start_pos);

/**
 * Parses the input string as a sequence of TMessageType, and write the messages to messages.
 *
 * @tparam TMessageType Message type to parse.
 * @param type selects whether to parse for request or response.
 * @param buf the buffer of data to parse as messages.
 * @param messages the parsed messages
 * @return result of the parse, including positions in the source buffer where messages were found.
 */
template <typename TMessageType>
ParseResult<size_t> Parse(MessageType type, std::string_view buf,
                          std::deque<TMessageType>* messages);

enum class ParseSyncType {
  // Do not perform a message boundary sync.
  None,
  // Perform a message boundary sync, where head might already be aligned.
  // Sync result of staying in the same spot is okay.
  Basic,
  // Perform a message boundary sync, where we want to force movement,
  // so disallow syncing to back to the existing position, unless no other boundary is discovered.
  Aggressive,
};

/**
 * @brief Parses a stream of events traced from write/send/read/recv syscalls,
 * and emits as many complete parsed messages as it can.
 */
template <typename TMessageType>
class EventParser {
 public:
  /**
   * @brief Append a sequence message to the internal buffer, ts_ns stands for time stamp in
   * nanosecond.
   */
  void Append(std::string_view msg, TimeSpan time_span) {
    msgs_.push_back(msg);
    ts_nses_.push_back(time_span.end_ns);
    time_spans_.push_back(time_span);
    msgs_size_ += msg.size();
  }

  /**
   * @brief Parses internal buffer text (see Append()) for messages, and writes resultant
   * parsed Messages into the provided messages container.
   *
   * This is a templated function. The caller must provide the type of message to parsed (e.g.
   * HTTPMessage), and must ensure that there is a corresponding Parse() function with the desired
   * message type.
   *
   * @param type The Type of message to parse.
   * @param messages The container to which newly parsed messages are added.
   *
   * @return ParseResult with locations where parseable messages were found in the source buffer.
   */
  ParseResult<BufferPosition> ParseMessages(MessageType type, std::deque<TMessageType>* messages,
                                            ParseSyncType sync_type = ParseSyncType::None) {
    std::string buf = Combine();

    size_t start_pos = 0;
    if (sync_type != ParseSyncType::None) {
      const bool force_movement = sync_type == ParseSyncType::Aggressive;
      start_pos = FindMessageBoundary<TMessageType>(type, buf, force_movement);

      // Couldn't find a boundary, so stay where we are.
      // Chances are we won't be able to parse, but we have no other option.
      if (start_pos == std::string::npos) {
        start_pos = 0;
      }
    }

    // Grab size before we start, so we know where the new parsed messages are.
    const size_t prev_size = messages->size();

    // Parse and append new messages to the messages vector.
    std::string_view buf_view(buf);
    buf_view.remove_prefix(start_pos);
    ParseResult<size_t> result = Parse(type, buf_view, messages);
    DCHECK(messages->size() >= prev_size);

    std::vector<BufferPosition> positions;

    // Match timestamps with the parsed messages.
    for (size_t i = 0; i < result.start_positions.size(); ++i) {
      // TODO(oazizi): ConvertPosition is inefficient, because it starts searching from scratch
      // everytime. Could do better if ConvertPosition took a starting seq and size.
      BufferPosition position = ConvertPosition(msgs_, start_pos + result.start_positions[i]);
      DCHECK(position.seq_num < msgs_.size()) << absl::Substitute(
          "The sequence number must be in valid range of [0, $0)", msgs_.size());
      positions.push_back(position);

      auto& msg = (*messages)[prev_size + i];
      msg.timestamp_ns = ts_nses_[position.seq_num];
      msg.time_span.begin_ns = time_spans_[position.seq_num].begin_ns;

      size_t last_byte_pos = (i == result.start_positions.size() - 1)
                                 ? result.end_position - 1
                                 : result.start_positions[i + 1] - 1;
      BufferPosition end_position = ConvertPosition(msgs_, start_pos + last_byte_pos);
      msg.time_span.end_ns = time_spans_[end_position.seq_num].end_ns;
    }

    BufferPosition end_position = ConvertPosition(msgs_, result.end_position);

    // Reset all state. Call to ParseMessages() is destructive of Append() state.
    msgs_.clear();
    ts_nses_.clear();
    time_spans_.clear();
    msgs_size_ = 0;

    return {std::move(positions), end_position, result.state};
  }

 private:
  std::string Combine() const {
    std::string result;
    result.reserve(msgs_size_);
    for (auto msg : msgs_) {
      result.append(msg);
    }
    return result;
  }

  /**
   * @brief Utility to convert positions from a position within a set of combined buffers,
   * to the position within a set of matching content in disjoint buffers.
   *
   * @param msgs The original set of disjoint buffers.
   * @param pos The position within the combined buffer.
   * @return Position within disjoint buffers, as buffer number and offset within the buffer.
   */
  BufferPosition ConvertPosition(const std::vector<std::string_view>& msgs, size_t pos) {
    size_t curr_seq = 0;
    size_t size = 0;
    for (auto msg : msgs) {
      size += msg.size();
      if (pos < size) {
        return {curr_seq, pos - (size - msg.size())};
      }
      ++curr_seq;
    }
    return {curr_seq, 0};
  }

  // ts_nses_ is the time stamp in nanosecond for the message in msgs_ with the same indexes.
  // TODO(yzhao): Remove this, as time_spans_ has the same information.
  std::vector<uint64_t> ts_nses_;
  std::vector<TimeSpan> time_spans_;
  std::vector<std::string_view> msgs_;

  // The total size of all strings in msgs_. Used to reserve memory space for concatenation.
  size_t msgs_size_ = 0;
};

}  // namespace stirling
}  // namespace pl
