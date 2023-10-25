/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <absl/container/flat_hash_map.h>
#include <algorithm>
#include <deque>
#include <map>
#include <memory>
#include <string>

#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/data_stream_buffer.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/types.h"

DECLARE_uint32(datastream_buffer_spike_size);
DECLARE_uint32(datastream_buffer_max_gap_size);
DECLARE_uint32(datastream_buffer_allow_before_gap_size);

DECLARE_uint32(buffer_resync_duration_secs);
DECLARE_uint32(buffer_expiration_duration_secs);

namespace px {
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
  // Make the underlying raw buffer size limit the same as the parsed frames byte limit.
  DataStream(uint32_t spike_capacity = FLAGS_datastream_buffer_spike_size,
             uint32_t max_gap_size = FLAGS_datastream_buffer_max_gap_size,
             uint32_t allow_before_gap_size = FLAGS_datastream_buffer_allow_before_gap_size)
      : data_buffer_(spike_capacity, max_gap_size, allow_before_gap_size) {}

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
  template <typename TKey, typename TFrameType, typename TStateType>
  void ProcessBytesToFrames(message_type_t type, TStateType* state);

  /**
   * Initialize the frames to the requested frame type.
   */
  template <typename TKey, typename TFrameType>
  void InitFrames() {
    bool check_condition =
        std::holds_alternative<std::monostate>(frames_) ||
        std::holds_alternative<absl::flat_hash_map<TKey, std::deque<TFrameType>>>(frames_);
    DCHECK(check_condition) << absl::Substitute(
        "Must hold the default std::monostate, or the same type as requested. "
        "I.e., ConnTracker cannot change the type it holds during runtime. $0 -> $1",
        frames_.index(), typeid(TFrameType).name());
    if (std::holds_alternative<std::monostate>(frames_)) {
      // Reset the type to the expected type.
      frames_ = absl::flat_hash_map<TKey, std::deque<TFrameType>>();
      LOG_IF(ERROR, frames_.valueless_by_exception())
          << absl::Substitute("valueless_by_exception() triggered by initializing to type: $0",
                              typeid(TFrameType).name());
    }
  }

  /**
   * Returns the current set of parsed frames.
   * @tparam TFrameType The parsed frame type within the deque.
   * @return deque of frames.
   */
  template <typename TKey, typename TFrameType>
  absl::flat_hash_map<TKey, std::deque<TFrameType>>& Frames() {
    // As a safety net, make sure the frames have been initialized.
    InitFrames<TKey, TFrameType>();

    LOG_IF(ERROR, frames_.valueless_by_exception()) << absl::Substitute(
        "valueless_by_exception() triggered by type: $0", typeid(TFrameType).name());
    return std::get<absl::flat_hash_map<TKey, std::deque<TFrameType>>>(frames_);
  }

  template <typename TKey, typename TFrameType>
  const absl::flat_hash_map<TKey, std::deque<TFrameType>>& Frames() const {
    DCHECK((std::holds_alternative<absl::flat_hash_map<TKey, std::deque<TFrameType>>>(frames_)))
        << absl::Substitute(
               "Must hold the same type as requested. "
               "I.e., ConnTracker cannot change the type it holds during runtime. $0 -> $1",
               frames_.index(), typeid(TFrameType).name());
    return std::get<absl::flat_hash_map<TKey, std::deque<TFrameType>>>(frames_);
  }

  /**
   * Approximate size of the parsed frames in the DataStream.
   */
  template <typename TKey, typename TFrameType>
  size_t FramesSize() const {
    size_t size = 0;
    for (const auto& [_, frames] : Frames<TKey, TFrameType>()) {
      for (const auto& frame : frames) {
        size += frame.ByteSize();
      }
    }
    return size;
  }

  /**
   * Clears all the data in the data buffer;
   */
  void Reset();

  /**
   * Checks if the DataStream is empty of both raw events and parsed messages.
   * @return true if empty of all data.
   */
  template <typename TKey, typename TFrameType>
  bool Empty() const {
    bool data_buffer_empty = data_buffer_.empty();
    bool monostate = std::holds_alternative<std::monostate>(frames_);
    if (data_buffer_empty || monostate) {
      return true;
    }
    bool all_deques_empty = true;
    for (const auto& [_, frames] :
         std::get<absl::flat_hash_map<TKey, std::deque<TFrameType>>>(frames_)) {
      if (!frames.empty()) {
        all_deques_empty = false;
        break;
      }
    }
    return all_deques_empty;
  }

  /**
   * If buffer has not been successfully processed in the past kSyncTimeout duration,
   * run ParseFrames() with a search for a new message boundary.
   * A long stuck time implies there is something unparsable at the head. It is neither returning
   * ParseState::kInvalid nor ParseState::kSuccess, but instead is returning
   * ParseState::kNeedsMoreData.
   * @return true if a resync will be performed in the next ParseFrames().
   */
  bool IsSyncRequired() const {
    const auto kSyncTimeout = std::chrono::seconds(FLAGS_buffer_resync_duration_secs);

    return (current_time_ - last_progress_time_) >= kSyncTimeout;
  }

  int stat_invalid_frames() const { return stat_invalid_frames_; }
  int stat_valid_frames() const { return stat_valid_frames_; }
  int stat_raw_data_gaps() const { return stat_raw_data_gaps_; }

  void UpdateLastProgressTime() { last_progress_time_ = current_time_; }

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
   * Checks if the connection of the DataStream is closed.
   * @return true if connection is closed.
   */
  bool conn_closed() const { return conn_closed_; }

  void set_conn_closed() { conn_closed_ = true; }

  void set_ssl_source(ssl_source_t ssl_source) { ssl_source_ = ssl_source; }

  void set_current_time(std::chrono::time_point<std::chrono::steady_clock> time) {
    ECHECK(time >= current_time_);
    current_time_ = time;

    // If there's no previous activity, set to current time.
    if (last_progress_time_.time_since_epoch().count() == 0) {
      UpdateLastProgressTime();
    }
  }

  void set_protocol(traffic_protocol_t protocol) { protocol_ = protocol; }

  /**
   * Cleanup frames that are parsed from the BPF events, when the condition is right.
   */
  template <typename TKey, typename TFrameType>
  void CleanupFrames(size_t size_limit_bytes,
                     std::chrono::time_point<std::chrono::steady_clock> expiry_timestamp) {
    size_t size = FramesSize<TKey, TFrameType>();
    if (size > size_limit_bytes) {
      VLOG(1) << absl::Substitute("Messages cleared due to size limit ($0 > $1).", size,
                                  size_limit_bytes);
      for (auto& [_, frame_deque] : Frames<TKey, TFrameType>()) {
        frame_deque.clear();
      }
    }
    EraseExpiredFrames(expiry_timestamp, &Frames<TKey, TFrameType>());
  }

  /**
   * Cleanup BPF events that are not able to be be processed.
   */
  bool CleanupEvents(size_t size_limit_bytes,
                     std::chrono::time_point<std::chrono::steady_clock> expiry_timestamp) {
    // We are assuming that when this stream is stuck, we clear the data buffer.
    if (last_progress_time_ < expiry_timestamp) {
      data_buffer_.Reset();
      has_new_events_ = false;
      UpdateLastProgressTime();
      return true;
    }

    if (data_buffer_.size() > size_limit_bytes) {
      data_buffer_.RemovePrefix(data_buffer_.size() - size_limit_bytes);
    }

    // Shrink the data buffer's allocated memory to fit just what is retained.
    data_buffer_.ShrinkToFit();

    return false;
  }

  const protocols::DataStreamBuffer& data_buffer() const { return data_buffer_; }
  protocols::DataStreamBuffer& data_buffer() { return data_buffer_; }

 private:
  template <typename TKey, typename TFrameType>
  static void EraseExpiredFrames(
      std::chrono::time_point<std::chrono::steady_clock> expiry_timestamp,
      absl::flat_hash_map<TKey, std::deque<TFrameType>>* frames) {
    for (auto& [key, deque] : *frames) {
      auto iter = deque.begin();
      for (; iter != deque.end(); ++iter) {
        auto frame_timestamp = std::chrono::time_point<std::chrono::steady_clock>(
            std::chrono::nanoseconds(iter->timestamp_ns));
        // As messages are put into the list with monotonically increasing creation time stamp,
        // we can just stop at the first frame that is younger than the expiration duration.
        // TODO(yzhao): Benchmark with binary search and pick the faster one.
        if (expiry_timestamp < frame_timestamp) {
          break;
        }
      }
      deque.erase(deque.begin(), iter);
    }
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

  // The current_time_ is the time the tracker should assume to be "now" during its processing.
  // The value is set by set_current_time().
  // This approach helps to avoid repeated calls to get the clock, and improves testability.
  // In the context of the SocketTracer, the current time is set at beginning of each iteration.
  std::chrono::time_point<std::chrono::steady_clock> current_time_;

  // The timestamp when progress was last made in the data buffer. It's used in CleanupEvents().
  std::chrono::time_point<std::chrono::steady_clock> last_progress_time_;

  // This is set to true when connection is closed.
  bool conn_closed_ = false;

  ssl_source_t ssl_source_ = kSSLNone;

  // Keep some stats on ParseFrames() attempts.
  int stat_valid_frames_ = 0;
  int stat_invalid_frames_ = 0;
  int stat_raw_data_gaps_ = 0;

  // A copy of the parse state from the last call to ProcessToRecords().
  ParseState last_parse_state_ = ParseState::kInvalid;

  // Keep track of the byte position after the last processed position, in order to measure data
  // loss.
  size_t last_processed_pos_ = 0;
  // Keep track of the protocol for this DataStream so that data loss can be reported per protocol.
  traffic_protocol_t protocol_ = traffic_protocol_t::kProtocolUnknown;

  template <typename TKey, typename TFrameType>
  friend std::string DebugString(const DataStream& d, std::string_view prefix);
};

// Note: can't make DebugString a class member because of GCC restrictions.

template <typename TKey, typename TFrameType>
inline std::string DebugString(const DataStream& d, std::string_view prefix) {
  std::string info;
  info += absl::Substitute("$0raw event bytes=$1\n", prefix, d.data_buffer_.size());
  int frames_size;
  if (std::holds_alternative<absl::flat_hash_map<TKey, std::deque<TFrameType>>>(d.frames_)) {
    const auto& frames_map = std::get<absl::flat_hash_map<TKey, std::deque<TFrameType>>>(d.frames_);
    // Loop through the map to sum the sizes of all the deques
    frames_size = 0;
    for (const auto& [key, frame_deque] : frames_map) {
      frames_size += frame_deque.size();
    }
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
}  // namespace px
