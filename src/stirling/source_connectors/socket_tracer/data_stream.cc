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

#include <gflags/gflags.h>
#include <utility>

#include "src/stirling/source_connectors/socket_tracer/data_stream.h"
#include "src/stirling/source_connectors/socket_tracer/metrics.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/types.h"

DEFINE_uint32(datastream_buffer_spike_size,
              gflags::Uint32FromEnv("PL_DATASTREAM_BUFFER_SPIKE_SIZE", 50 * 1024 * 1024),
              "The maximum temporary size of a data stream buffer before processing.");
DEFINE_uint32(
    datastream_buffer_max_gap_size,
    gflags::Uint32FromEnv("PL_DATASTREAM_BUFFER_MAX_GAP_SIZE", 10 * 1024 * 1024),
    "The maximum gap in data to allow before giving up on previous events in the buffer.");
DEFINE_uint32(datastream_buffer_allow_before_gap_size,
              gflags::Uint32FromEnv("PL_DATASTREAM_BUFFER_ALLOW_BEFORE_GAP_SIZE", 1 * 1024 * 1024),
              "After a PL_DATASTREAM_BUFFER_MAX_GAP_SIZE gap occurs, we allow for this amount of "
              "data to come in before (byte position wise) the event that caused the large gap.");

DEFINE_uint32(buffer_resync_duration_secs, 5,
              "The duration, in seconds, after which a buffer resync will happen if there has been "
              "no progress in the parser.");
DEFINE_uint32(
    buffer_expiration_duration_secs, 10,
    "The duration, in seconds, after which the buffer will be cleared if there is no progress in "
    "the parser.");

namespace px {
namespace stirling {

void DataStream::AddData(std::unique_ptr<SocketDataEvent> event) {
  LOG_IF(WARNING, event->attr.msg_size > event->msg.size() && !event->msg.empty())
      << absl::Substitute("Message truncated, original size: $0, transferred size: $1",
                          event->attr.msg_size, event->msg.size());

  data_buffer_.Add(event->attr.pos, event->msg, event->attr.timestamp_ns);

  has_new_events_ = true;
}

// ProcessBytesToFrames() processes the raw data in the DataStream to extract parsed frames.
//
// It considers contiguous events from the head of the stream. Any missing events in the sequence
// are treated as lost forever; it is not expected that these events arrive in a subsequent
// iteration due to the way BPF capture works.
//
// If a gap (missing event) in the stream occurs, it is skipped over, and the next sequence
// of contiguous events are processed. Note that the sequence of contiguous events are parsed
// independently of each other.
//
// To be robust to lost events, which are not necessarily aligned to parseable entity boundaries,
// ProcessBytesToFrames() will invoke a call to ParseFrames() with a stream recovery argument when
// necessary.
template <typename TKey, typename TFrameType, typename TStateType = protocols::NoState>
void DataStream::ProcessBytesToFrames(message_type_t type, TStateType* state) {
  auto& typed_messages = Frames<TKey, TFrameType>();

  // TODO(oazizi): Convert to ECHECK once we have more confidence.
  LOG_IF(WARNING, IsEOS()) << "DataStream reaches EOS, no more data to process.";

  const size_t orig_pos = data_buffer_.position();

  // A description of some key variables in this function:
  //
  // - last_progress_time_: The timestamp of when progress was made in the parser. It's used to
  //                        calculate if we are stuck.
  //
  // - has_new_events_: An optimization to avoid the expensive call to ParseFrames() when
  //                    nothing has changed in the DataStream. Note that we *do* want to call
  //                    ParseFrames() even when there are no new events, if the
  //                    stuck_count_ is high enough and we want to attempt a stream
  //                    recovery.
  //
  // Local variables are intermediate computations to help simplify the code:
  // - keep_processing: Controls the loop iterations. If we hit a gap in the stream events,
  //                    we use keep_processing to indicate that we should make one more iteration
  //                    for the next sequence of contiguous events.
  //
  // - attempt_sync: Indicates that we should attempt to process the stream even if there are no
  //                 new events, because we have hit the threshold to attempt a stream recovery.
  //                 Used for the first iteration only.

  // We appear to be stuck with an an unparsable sequence of events blocking the head.
  bool attempt_sync = IsSyncRequired();

  bool keep_processing = has_new_events_ || attempt_sync || conn_closed();

  protocols::ParseResult<TKey> parse_result;
  parse_result.state = ParseState::kNeedsMoreData;
  parse_result.end_position = 0;

  size_t frame_bytes = 0;

  while (keep_processing && !data_buffer_.empty()) {
    size_t contiguous_bytes = data_buffer_.Head().size();

    // Now parse the raw data.
    parse_result =
        protocols::ParseFrames(type, &data_buffer_, &typed_messages, IsSyncRequired(), state);
    if (contiguous_bytes != data_buffer_.size()) {
      // We weren't able to submit all bytes, which means we ran into a missing event.
      // We don't expect missing events to arrive in the future, so just cut our losses.
      // Drop all events up to this point, and then try to resume.
      data_buffer_.RemovePrefix(contiguous_bytes);
      data_buffer_.Trim();

      keep_processing = (parse_result.state != ParseState::kEOS);
    } else {
      // We had a contiguous stream, with no missing events.
      // Erase bytes that have been fully processed.
      // If anything was processed at all, reset stuck count.
      if (parse_result.end_position != 0) {
        data_buffer_.RemovePrefix(parse_result.end_position);
      }

      keep_processing = false;
    }

    for (const auto& [stream, positions] : parse_result.frame_positions) {
      stat_valid_frames_ += positions.size();
    }
    stat_invalid_frames_ += parse_result.invalid_frames;
    stat_raw_data_gaps_ += keep_processing;

    frame_bytes += parse_result.frame_bytes;
  }

  // Check to see if we are blocked on parsing.
  // Note that missing events is handled separately (not considered stuck).
  bool made_progress = data_buffer_.empty() || (data_buffer_.position() != orig_pos);
  if (made_progress) {
    UpdateLastProgressTime();
  }

  if (parse_result.state == ParseState::kEOS) {
    ECHECK(made_progress);
  }

  // If parse state is kInvalid, then no amount of waiting is going to help us.
  // Reset the data right away to potentially unblock.
  if (parse_result.state == ParseState::kInvalid) {
    // Currently, we reset all the data.
    // Alternative is to find the next frame boundary, rather than discarding all data.

    // TODO(oazizi): A dedicated data_buffer_.Flush() implementation would be more efficient.
    data_buffer_.RemovePrefix(data_buffer_.size());
    UpdateLastProgressTime();
  }

  // Keep track of "lost" data in prometheus. "lost" data includes any gaps in the data stream as
  // well as data that wasn't able to be successfully parsed.
  ssize_t num_bytes_advanced = data_buffer_.position() - last_processed_pos_;
  if (num_bytes_advanced > 0 && static_cast<size_t>(num_bytes_advanced) > frame_bytes) {
    size_t bytes_lost = num_bytes_advanced - frame_bytes;
    SocketTracerMetrics::GetProtocolMetrics(protocol_, ssl_source_)
        .data_loss_bytes.Increment(bytes_lost);
  }
  last_processed_pos_ = data_buffer_.position();

  last_parse_state_ = parse_result.state;

  // has_new_events_ should be false for the next transfer cycle.
  has_new_events_ = false;
}

// PROTOCOL_LIST: Requires update on new protocols.
template void DataStream::ProcessBytesToFrames<
    protocols::http::stream_id_t, protocols::http::Message, protocols::http::StateWrapper>(
    message_type_t type, protocols::http::StateWrapper* state);
template void DataStream::ProcessBytesToFrames<protocols::mux::stream_id_t, protocols::mux::Frame,
                                               protocols::NoState>(message_type_t type,
                                                                   protocols::NoState* state);
template void DataStream::ProcessBytesToFrames<
    protocols::mysql::connection_id_t, protocols::mysql::Packet, protocols::mysql::StateWrapper>(
    message_type_t type, protocols::mysql::StateWrapper* state);
template void DataStream::ProcessBytesToFrames<protocols::cass::stream_id_t, protocols::cass::Frame,
                                               protocols::NoState>(message_type_t type,
                                                                   protocols::NoState* state);
template void DataStream::ProcessBytesToFrames<
    protocols::pgsql::connection_id_t, protocols::pgsql::RegularMessage,
    protocols::pgsql::StateWrapper>(message_type_t type, protocols::pgsql::StateWrapper* state);
template void DataStream::ProcessBytesToFrames<protocols::dns::stream_id_t, protocols::dns::Frame,
                                               protocols::NoState>(message_type_t type,
                                                                   protocols::NoState* state);
template void DataStream::ProcessBytesToFrames<protocols::redis::stream_id_t,
                                               protocols::redis::Message, protocols::NoState>(
    message_type_t type, protocols::NoState* state);
template void DataStream::ProcessBytesToFrames<
    protocols::kafka::correlation_id_t, protocols::kafka::Packet, protocols::kafka::StateWrapper>(
    message_type_t type, protocols::kafka::StateWrapper* state);
template void DataStream::ProcessBytesToFrames<protocols::nats::stream_id_t,
                                               protocols::nats::Message, protocols::NoState>(
    message_type_t type, protocols::NoState* state);
template void DataStream::ProcessBytesToFrames<protocols::amqp::channel_id, protocols::amqp::Frame,
                                               protocols::NoState>(message_type_t type,
                                                                   protocols::NoState* state);
template void DataStream::ProcessBytesToFrames<
    protocols::mongodb::stream_id_t, protocols::mongodb::Frame, protocols::mongodb::StateWrapper>(
    message_type_t type, protocols::mongodb::StateWrapper* state);
void DataStream::Reset() {
  data_buffer_.Reset();
  has_new_events_ = false;
  UpdateLastProgressTime();

  frames_ = std::monostate();
}

}  // namespace stirling
}  // namespace px
