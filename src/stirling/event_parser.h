#pragma once

#include <deque>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "src/common/base/base.h"
#include "src/stirling/bcc_bpf/socket_trace.h"

namespace pl {
namespace stirling {

// The direction of traffic expected on a probe.
enum class MessageType { kUnknown, kRequests, kResponses, kMixed };

enum class ParseState {
  kUnknown,
  // The data is invalid.
  kInvalid,
  // The data appears to be an incomplete message - more data is needed.
  kNeedsMoreData,
  kSuccess,
};

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
  ParseState state;
};

/**
 * @brief Parses a stream of events traced from write/send/read/recv syscalls,
 * and emits as many complete parsed messages as it can.
 */
template <class TMessageType>
class EventParser {
 public:
  /**
   * @brief Append a sequence message to the internal buffer, ts_ns stands for time stamp in
   * nanosecond.
   */
  void Append(std::string_view msg, uint64_t ts_ns) {
    msgs_.push_back(msg);
    ts_nses_.push_back(ts_ns);
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
  ParseResult<BufferPosition> ParseMessages(MessageType type, std::deque<TMessageType>* messages) {
    std::string buf = Combine();

    // Grab size before we start, so we know where the new parsed messages are.
    size_t prev_size = messages->size();

    // Parse and append new messages to the messages vector.
    ParseResult<size_t> result = Parse(type, buf, messages);
    DCHECK(messages->size() >= prev_size);

    std::vector<BufferPosition> positions;

    // Match timestamps with the parsed messages.
    for (size_t i = 0; i < result.start_positions.size(); ++i) {
      auto& msg = (*messages)[prev_size + i];
      // TODO(oazizi): ConvertPosition is inefficient, because it starts searching from scratch
      // everytime. Could do better if ConvertPosition took a starting seq and size.
      BufferPosition position = ConvertPosition(msgs_, result.start_positions[i]);
      positions.push_back(position);
      DCHECK(position.seq_num < msgs_.size()) << absl::Substitute(
          "The sequence number must be in valid range of [0, $0)", msgs_.size());
      msg.timestamp_ns = ts_nses_[position.seq_num];
    }

    BufferPosition end_position = ConvertPosition(msgs_, result.end_position);

    msgs_.clear();
    ts_nses_.clear();
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

  /** Utility to convert positions from a position within a set of combined buffers,
   * to the position within a set of matching content in disjoint buffers.
   * @param msgs The original set of disjoint buffers.
   * @param pos The position within the combined buffer.
   * @return Position within disjoint buffers, as buffer number and offset within the buffer.
   */
  BufferPosition ConvertPosition(std::vector<std::string_view> msgs, size_t pos) {
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

  // The total size of all strings in msgs_. Used for reserve memory space for concatenation.
  size_t msgs_size_ = 0;
  std::vector<uint64_t> ts_nses_;
  std::vector<std::string_view> msgs_;
};

}  // namespace stirling
}  // namespace pl
