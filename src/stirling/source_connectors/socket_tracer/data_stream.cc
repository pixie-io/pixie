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

#include "src/stirling/source_connectors/socket_tracer/data_stream.h"

#include <utility>

#include "src/stirling/source_connectors/socket_tracer/protocols/pgsql/types.h"
// TODO(yzhao): Without this line :stirling_wrapper fails to link redis template specializations
// of FindFrameBoundary() and ParseFrames().
#include "src/stirling/source_connectors/socket_tracer/protocols/redis/parse.h"

DEFINE_uint32(datastream_buffer_size, 1024 * 1024, "The maximum size of a data stream buffer.");

namespace px {
namespace stirling {

void DataStream::AddData(std::unique_ptr<SocketDataEvent> event) {
  // Note that the BPF code will also generate a missing sequence number when truncation occurs,
  // so the data stream will naturally reset after processing this event.
  LOG_IF(ERROR, event->attr.msg_size > event->msg.size() && !event->msg.empty())
      << absl::Substitute("Message truncated, original size: $0, transferred size: $1",
                          event->attr.msg_size, event->msg.size());

  data_buffer_.Add(event->attr.pos, event->msg, event->attr.timestamp_ns);

  has_new_events_ = true;
}

namespace {

bool IsSyncRequired(int64_t stuck_count) {
  ECHECK_GE(stuck_count, 0);

  // Stuck counts where we switch the sync policy.
  static constexpr int64_t kBasicSyncThreshold = 1;

  // Thresholds must be in increasing order.
  static_assert(kBasicSyncThreshold > 0);

  if (stuck_count <= kBasicSyncThreshold) {
    // If stuck_count == 0, then no reason to sync.
    // If stuck_count != 0, but is low, it could mean we have partial data (i.e. kNeedsMoreData).
    // The rest of the data could now be avilable in this new iteration,
    // so still don't try to search for a message boundary yet.
    return false;
  }

  // Multiple stuck cycles implies there is something unparseable at the head.
  // It is neither returning ParseState::kInvalid nor ParseState::kSuccess.
  // It constantly is returning ParseState::kNeedsMoreData.
  // Run ParseFrames() with a search for a new message boundary;
  return true;
}

}  // namespace

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
template <typename TFrameType>
void DataStream::ProcessBytesToFrames(MessageType type) {
  auto& typed_messages = Frames<TFrameType>();

  // TODO(oazizi): Convert to ECHECK once we have more confidence.
  LOG_IF(WARNING, IsEOS()) << "DataStream reaches EOS, no more data to process.";

  const size_t orig_pos = data_buffer_.position();

  // A description of some key variables in this function:
  //
  // - stuck_count_: Number of calls to where no new frames were produced.
  //                 indicates an unparseable event at the head that is blocking progress.
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

  // We appear to be stuck with an an unparseable sequence of events blocking the head.
  bool attempt_sync = IsSyncRequired(stuck_count_);

  bool keep_processing = has_new_events_ || attempt_sync;

  protocols::ParseResult parse_result;
  parse_result.state = ParseState::kNeedsMoreData;
  parse_result.end_position = 0;

  while (keep_processing && !data_buffer_.empty()) {
    size_t contiguous_bytes = data_buffer_.Head().size();

    // Now parse the raw data.
    parse_result =
        protocols::ParseFrames(type, data_buffer_, &typed_messages, IsSyncRequired(stuck_count_));

    if (contiguous_bytes != data_buffer_.size()) {
      // We weren't able to submit all bytes, which means we ran into a missing event.
      // We don't expect missing events to arrive in the future, so just cut our losses.
      // Drop all events up to this point, and then try to resume.
      data_buffer_.RemovePrefix(contiguous_bytes);
      data_buffer_.Trim();

      // Update stuck count so we use the correct sync type on the next iteration.
      stuck_count_ = 0;

      keep_processing = (parse_result.state != ParseState::kEOS);
    } else {
      // We had a contiguous stream, with no missing events.
      // Erase bytes that have been fully processed.
      // If anything was processed at all, reset stuck count.
      if (parse_result.end_position != 0) {
        data_buffer_.RemovePrefix(parse_result.end_position);
        stuck_count_ = 0;
      }

      keep_processing = false;
    }

    stat_valid_frames_ += parse_result.frame_positions.size();
    stat_invalid_frames_ += parse_result.invalid_frames;
    stat_raw_data_gaps_ += keep_processing;
  }

  // Check to see if we are blocked on parsing.
  // Note that missing events is handled separately (not considered stuck).
  bool events_but_no_progress = !data_buffer_.empty() && (data_buffer_.position() == orig_pos);
  if (events_but_no_progress) {
    ++stuck_count_;
  }

  if (parse_result.state == ParseState::kEOS) {
    ECHECK(!events_but_no_progress);
  }

  // If parse state is kInvalid, then no amount of waiting is going to help us.
  // Reset the data right away to potentially unblock.
  if (parse_result.state == ParseState::kInvalid) {
    // Currently, we reset all the data.
    // Alternative is to find the next frame boundary, rather than discarding all data.

    // TODO(oazizi): A dedicated data_buffer_.Flush() implementation would be more efficient.
    data_buffer_.RemovePrefix(data_buffer_.size());
    stuck_count_ = 0;
  }

  last_parse_state_ = parse_result.state;

  // has_new_events_ should be false for the next transfer cycle.
  has_new_events_ = false;
}

// PROTOCOL_LIST: Requires update on new protocols.
template void DataStream::ProcessBytesToFrames<protocols::http::Message>(MessageType type);
template void DataStream::ProcessBytesToFrames<protocols::mysql::Packet>(MessageType type);
template void DataStream::ProcessBytesToFrames<protocols::cass::Frame>(MessageType type);
template void DataStream::ProcessBytesToFrames<protocols::pgsql::RegularMessage>(MessageType type);
template void DataStream::ProcessBytesToFrames<protocols::dns::Frame>(MessageType type);
template void DataStream::ProcessBytesToFrames<protocols::redis::Message>(MessageType type);
template void DataStream::ProcessBytesToFrames<protocols::kafka::Packet>(MessageType type);

void DataStream::Reset() {
  data_buffer_.Reset();
  has_new_events_ = false;
  stuck_count_ = 0;

  frames_ = std::monostate();
}

}  // namespace stirling
}  // namespace px
