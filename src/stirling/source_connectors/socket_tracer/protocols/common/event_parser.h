#pragma once

#include <deque>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <absl/base/macros.h>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/common.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/utils/parse_state.h"
#include "src/stirling/utils/utils.h"

namespace pl {
namespace stirling {
namespace protocols {

// All protocol Frames should derive off this base definition, which includes standard fields.
struct FrameBase {
  uint64_t timestamp_ns = 0;

  // ByteSize() is used as part of Cleanup(); used to determine how much memory a tracker is using.
  virtual size_t ByteSize() const = 0;

  virtual ~FrameBase() = default;
};

struct BufferPosition {
  size_t seq_num;
  size_t offset;
};

template <typename PositionType>
struct StartEndPos {
  // Start position is the location of the first byte of the frame.
  PositionType start;
  // End position is the location of the last byte of the frame.
  // Unlike STL container end, this is not 1 byte passed the end.
  PositionType end;
};

inline bool operator==(const BufferPosition& lhs, const BufferPosition& rhs) {
  return lhs.seq_num == rhs.seq_num && lhs.offset == rhs.offset;
}

template <typename TPosType>
inline bool operator==(const StartEndPos<TPosType>& lhs, const StartEndPos<TPosType>& rhs) {
  return lhs.start == rhs.start && lhs.end == rhs.end;
}

// A ParseResult returns a vector of parsed frames, and also some position markers.
//
// It is templated based on the position type, because we have two concepts of position:
//    Position in a contiguous buffer: PositionType is size_t.
//    Position in a set of disjoint buffers: PositionType is BufferPosition.
//
// The two concepts are used by two different parse functions we have:
//
// ParseResult<size_t> Parse(MessageType type, std::string_view buf);
// ParseResult<BufferPosition> ParseFrames(MessageType type);
template <typename PositionType>
struct ParseResult {
  // Positions of frame start and end positions in the source buffer.
  std::vector<StartEndPos<PositionType>> frame_positions;
  // Position of where parsing ended consuming the source buffer.
  // When PositionType is bytes, this is total bytes successfully consumed.
  PositionType end_position;
  // State of the last attempted frame parse.
  ParseState state = ParseState::kInvalid;
  // Number of invalid frames that were discarded.
  int invalid_frames;
};

/**
 * Utility to convert positions from a position within a set of combined buffers,
 * to the position within a set of matching content in disjoint buffers.
 */
class PositionConverter {
 public:
  PositionConverter() { Reset(); }

  void Reset() {
    curr_seq_ = 0;
    size_ = 0;
  }

  /**
   * @brief Convert position within a set of combined buffers
   * to the position within a set of matching content in disjoint buffers.
   *
   * @param msgs The original set of disjoint buffers.
   * @param pos The position within the combined buffer to convert.
   * @return Position within disjoint buffers, as buffer number and offset within the buffer.
   */
  BufferPosition Convert(const std::vector<std::string_view>& msgs, size_t pos) {
    DCHECK_GE(pos, last_query_pos_)
        << "Position converter cannot go backwards (enforced for performance reasons).";
    // If we ever want to remove the restriction above, the following would do the trick:
    //   if (pos <= last_query_pos_) { Reset(); }

    // Record position of this call, to enforce that we never go backwards.
    last_query_pos_ = pos;

    while (curr_seq_ < msgs.size()) {
      const auto& msg = msgs[curr_seq_];

      // If next frame would cause the crossover,
      // then we have found the point we're looking for.
      if (pos < size_ + msg.size()) {
        return {curr_seq_, pos - size_};
      }

      ++curr_seq_;
      size_ += msg.size();
    }
    return {curr_seq_, 0};
  }

 private:
  // Optimization: keep track of last state, so we can efficiently resume search,
  // so long as the next position to Convert() is after the last one.
  size_t curr_seq_ = 0;
  size_t size_ = 0;
  size_t last_query_pos_ = 0;
};

/**
 * @brief Parses a stream of events traced from write/send/read/recv syscalls,
 * and emits as many complete parsed frames as it can.
 */
class EventParser {
 public:
  /**
   * @brief Append raw data to the internal buffer.
   */
  void Append(const SocketDataEvent& event) {
    msgs_.push_back(event.msg);
    ts_nses_.push_back(event.attr.timestamp_ns);
    msgs_size_ += event.msg.size();
  }

  /**
   * @brief Parses internal data buffer (see Append()) for frames, and writes resultant
   * parsed frames into the provided frames container.
   *
   * This is a templated function. The caller must provide the type of frame to parsed (e.g.
   * http::Message), and must ensure that the corresponding ParseFrame() function with the desired
   * frame type is implemented.
   *
   * @param type The Type of frames to parse.
   * @param frames The container to which newly parsed frames are added.
   * @param resync If set to true, Parse will first search for the next frame boundary (even
   * if it is currently at a valid frame boundary).
   *
   * @return ParseResult with locations where parseable frames were found in the source buffer.
   */
  template <typename TFrameType>
  ParseResult<BufferPosition> ParseFrames(MessageType type, std::deque<TFrameType>* frames,
                                          bool resync = false) {
    std::string buf = Combine();

    size_t start_pos = 0;
    if (resync) {
      VLOG(3) << "Finding next frame boundary";
      // Since we've been asked to resync, we search from byte 1 to find a new boundary.
      // Don't want to stay at the same position.
      constexpr int kStartPos = 1;
      start_pos = FindFrameBoundary<TFrameType>(type, buf, kStartPos);

      // Couldn't find a boundary, so stay where we are.
      // Chances are we won't be able to parse, but we have no other option.
      if (start_pos == std::string::npos) {
        start_pos = 0;
      }
    }

    // Grab size before we start, so we know where the new parsed frames are.
    const size_t prev_size = frames->size();

    // Parse and append new frames to the frames vector.
    std::string_view buf_view(buf);
    buf_view.remove_prefix(start_pos);
    ParseResult<size_t> result = ParseFramesLoop(type, buf_view, frames);

    VLOG(3) << absl::Substitute("Parsed $0 new frames", frames->size() - prev_size);

    std::vector<StartEndPos<BufferPosition>> frame_positions;

    PositionConverter converter;

    // Match timestamps with the parsed frames.
    for (size_t i = 0; i < result.frame_positions.size(); ++i) {
      StartEndPos<BufferPosition> frame_position = {
          converter.Convert(msgs_, start_pos + result.frame_positions[i].start),
          converter.Convert(msgs_, start_pos + result.frame_positions[i].end)};
      DCHECK(frame_position.start.seq_num < msgs_.size()) << absl::Substitute(
          "The sequence number must be in valid range of [0, $0)", msgs_.size());
      DCHECK(frame_position.end.seq_num < msgs_.size()) << absl::Substitute(
          "The sequence number must be in valid range of [0, $0)", msgs_.size());
      frame_positions.push_back(frame_position);

      auto& msg = (*frames)[prev_size + i];
      msg.timestamp_ns = ts_nses_[frame_position.end.seq_num];
    }

    BufferPosition end_position = converter.Convert(msgs_, start_pos + result.end_position);

    // Reset all state. Call to ParseFrames() is destructive of Append() state.
    msgs_.clear();
    ts_nses_.clear();
    msgs_size_ = 0;

    return {std::move(frame_positions), end_position, result.state, result.invalid_frames};
  }

  /**
   * @brief Calls ParseFrame() repeatedly on a continguous stream of raw bytes.
   * parsed frames into the provided frames container.
   *
   * Note: This is a helper function for EventParser::ParseFrames().
   * It is left public for now because it is used heavily by tests.
   *
   * @param type The Type of frames to parse.
   * @param buf The raw bytes to parse
   * @param frames The output where the parsed frames will be placed.
   *
   * @return ParseResult with locations where parseable frames were found in the source buffer.
   */
  // TODO(oazizi): Convert tests to use ParseFrames() instead of ParseFramesLoop().
  template <typename TFrameType>
  ParseResult<size_t> ParseFramesLoop(MessageType type, std::string_view buf,
                                      std::deque<TFrameType>* frames) {
    std::vector<StartEndPos<size_t>> frame_positions;
    const size_t buf_size = buf.size();
    ParseState s = ParseState::kSuccess;
    size_t bytes_processed = 0;
    int invalid_count = 0;

    while (!buf.empty() && s != ParseState::kEOS) {
      TFrameType frame;

      s = ParseFrame(type, &buf, &frame);

      bool stop = false;
      bool push = false;
      switch (s) {
        case ParseState::kNeedsMoreData:
          // Can't process any more frames.
          stop = true;
          break;
        case ParseState::kInvalid: {
          // An invalid frame may occur when first parsing a connection, or after a lost event.
          // Attempt to look for next valid frame boundary.
          size_t pos = FindFrameBoundary<TFrameType>(type, buf, 1);
          if (pos != std::string::npos) {
            DCHECK_NE(pos, 0);
            buf.remove_prefix(pos);
            stop = false;
            push = false;
          } else {
            stop = true;
            push = false;
          }
          ++invalid_count;
        } break;
        case ParseState::kIgnored:
          // Successful case, but do not record the result.
          stop = false;
          push = false;
          break;
        case ParseState::kEOS:
        case ParseState::kSuccess:
          // Successful cases. Record the result.
          stop = false;
          push = true;
          break;
        default:
          DCHECK(false);
      }

      if (stop) {
        break;
      }

      size_t start_position = bytes_processed;
      bytes_processed = (buf_size - buf.size());
      size_t end_position = bytes_processed - 1;

      if (push) {
        frame_positions.push_back({start_position, end_position});
        frames->push_back(std::move(frame));
      }
    }
    return ParseResult<size_t>{std::move(frame_positions), bytes_processed, s, invalid_count};
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

  // ts_nses_ is the time stamp in nanosecond for the frame in msgs_ with the same indexes.
  std::vector<uint64_t> ts_nses_;
  std::vector<std::string_view> msgs_;

  // The total size of all strings in msgs_. Used to reserve memory space for concatenation.
  size_t msgs_size_ = 0;
};

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
