#include "src/stirling/data_stream.h"

#include <utility>

DEFINE_uint32(messages_expiration_duration_secs, 10 * 60,
              "The duration for which a cached message to be erased.");
DEFINE_uint32(messages_size_limit_bytes, 1024 * 1024,
              "The limit of the size of the parsed messages, not the BPF events, "
              "for each direction, of each connection tracker. "
              "All cached messages are erased if this limit is breached.");

namespace pl {
namespace stirling {

void DataStream::AddEvent(std::unique_ptr<SocketDataEvent> event) {
  uint64_t seq_num = event->attr.seq_num;

  // Note that the BPF code will also generate a missing sequence number when truncation occurs,
  // so the data stream will naturally reset after processing this event.
  LOG_IF(ERROR, event->attr.msg_size > event->msg.size())
      << absl::Substitute("Message truncated, original size: $0, accepted size: $1",
                          event->attr.msg_size, event->msg.size());

  if (seq_num < next_seq_num_) {
    LOG(WARNING) << absl::Substitute(
        "Ignoring event that has already been skipped [event seq_num=$0, current seq_num=$1].",
        seq_num, next_seq_num_);
    return;
  }

  auto res = events_.emplace(seq_num, TimestampedData(std::move(event)));
  LOG_IF(ERROR, !res.second) << absl::Substitute("Clobbering data event [pid=$0 fd=$1 gen=$2].",
                                                 event->attr.conn_id.pid, event->attr.conn_id.fd,
                                                 event->attr.conn_id.generation);
  has_new_events_ = true;
}

template <typename TMessageType>
std::deque<TMessageType>& DataStream::Messages() {
  DCHECK(std::holds_alternative<std::monostate>(messages_) ||
         std::holds_alternative<std::deque<TMessageType>>(messages_))
      << "Must hold the default std::monostate, or the same type as requested. "
         "I.e., ConnectionTracker cannot change the type it holds during runtime.";
  if (std::holds_alternative<std::monostate>(messages_)) {
    // Reset the type to the expected type.
    messages_ = std::deque<TMessageType>();
  }

  return std::get<std::deque<TMessageType>>(messages_);
}

template <typename TMessageType>
size_t DataStream::AppendEvents(EventParser<TMessageType>* parser) const {
  size_t append_count = 0;

  // Prepare all recorded events for parsing.
  size_t next_seq_num = next_seq_num_;
  size_t next_offset = offset_;
  for (const auto& [seq_num, event] : events_) {
    // Not at expected seq_num. Stop submitting events to parser.
    if (seq_num != next_seq_num) {
      break;
    }

    // The main message to submit to parser.
    std::string_view msg = event.msg;

    // First message may have been partially processed by a previous call to this function.
    // In such cases, the offset will be non-zero, and we need a sub-string of the first event.
    if (next_offset != 0) {
      ECHECK_LT(next_offset, event.msg.size());
      msg = msg.substr(next_offset, event.msg.size() - next_offset);
    }

    parser->Append(msg, {event.entry_timestamp_ns, event.return_timestamp_ns});

    next_offset = 0;
    ++next_seq_num;
    ++append_count;
  }

  return append_count;
}

namespace {

ParseSyncType SelectSyncType(int64_t stuck_count) {
  ECHECK_GE(stuck_count, 0);

  // Stuck counts where we switch the sync policy.
  static constexpr int64_t kBasicSyncThreshold = 1;
  static constexpr int64_t kAggressiveSyncThreshold = 2;

  // Thresholds must be in increasing order.
  static_assert(kBasicSyncThreshold > 0);
  static_assert(kAggressiveSyncThreshold > kBasicSyncThreshold);

  if (stuck_count == 0) {
    // Not stuck, so no reason to search for a message boundary.
    // Note that this is covered by the next if-statement, but left here to be explicit.
    return ParseSyncType::None;
  }
  if (stuck_count <= kBasicSyncThreshold) {
    // A low number of stuck cycles could mean we have partial data.
    // The rest might be back in this iteration, so still don't try to search for a message
    // boundary.
    return ParseSyncType::None;
  }
  if (stuck_count <= kAggressiveSyncThreshold) {
    // Multiple stuck cycles implies there is something unparseable at the head.
    // Run ParseMessages() with a search for a message boundary;
    return ParseSyncType::Basic;
  }

  // We're really having trouble now, so invoke ParseMessages() with a more aggressive search.
  // For now, more aggressive just means a message discovered at pos 0 is ignored,
  // because presumably it's the one that is giving us problems, and we want to skip over it.
  return ParseSyncType::Aggressive;
}

}  // namespace

// ExtractMessages() processes the events in the DataStream to extract parsed messages.
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
// ExtractMessages() will invoke a call to ParseMessages() with a stream recovery argument when
// necessary.
template <typename TMessageType>
std::deque<TMessageType>& DataStream::ExtractMessages(MessageType type) {
  auto& typed_messages = Messages<TMessageType>();

  // TODO(oazizi): Convert to ECHECK once we have more confidence.
  LOG_IF(WARNING, IsEOS()) << "Calling ExtractMessages on stream that is at EOS.";

  const size_t orig_offset = offset_;
  const size_t orig_seq_num = next_seq_num_;

  // A description of some key variables in this function:
  //
  // Member variables hold state across calls to ExtractMessages():
  // - stuck_count_: Number of calls to ExtractMessages() where no progress has been made.
  //                 indicates an unparseable event at the head that is blocking progress.
  //
  // - has_new_events_: An optimization to avoid the expensive call to ParseMessages() when
  //                    nothing has changed in the DataStream. Note that we *do* want to call
  //                    ParseMessages() even when there are no new events, if the stuck_count_
  //                    is high enough and we want to attempt a stream recovery.
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
  bool attempt_sync = SelectSyncType(stuck_count_) != ParseSyncType::None;

  bool keep_processing = has_new_events_ || attempt_sync;

  ParseResult<BufferPosition> parse_result;

  while (keep_processing) {
    EventParser<TMessageType> parser;

    // Set-up events in parser.
    size_t num_events_appended = AppendEvents(&parser);

    // Now parse all the appended events.
    parse_result = parser.ParseMessages(type, &typed_messages, SelectSyncType(stuck_count_));

    if (num_events_appended != events_.size()) {
      // We weren't able to append all events, which means we ran into a missing event.
      // We don't expect missing events to arrive in the future, so just cut our losses.
      // Drop all events up to this point, and then try to resume.
      auto erase_iter = events_.begin();
      std::advance(erase_iter, num_events_appended);
      events_.erase(events_.begin(), erase_iter);
      ECHECK(!events_.empty());
      next_seq_num_ = events_.begin()->first;
      offset_ = 0;

      // Update stuck count so we use the correct sync type on the next iteration.
      stuck_count_ = 0;

      keep_processing = (parse_result.state != ParseState::kEOS);
    } else {
      // We appended all events, which means we had a contiguous stream, with no missing events.
      // Find and erase events that have been fully processed.
      // Note that ParseResult seq_nums are based on events added to parser, not seq_nums from BPF.
      size_t num_events_consumed = parse_result.end_position.seq_num;
      auto erase_iter = events_.begin();
      std::advance(erase_iter, num_events_consumed);
      events_.erase(events_.begin(), erase_iter);
      next_seq_num_ += num_events_consumed;
      offset_ = parse_result.end_position.offset;

      keep_processing = false;
    }
  }

  // Check to see if we are blocked on parsing.
  // Note that missing events is handled separately (not considered stuck).
  bool events_but_no_progress =
      !events_.empty() && (next_seq_num_ == orig_seq_num) && (offset_ == orig_offset);
  stuck_count_ = (events_but_no_progress) ? stuck_count_ + 1 : 0;

  if (parse_result.state == ParseState::kEOS) {
    ECHECK(!events_but_no_progress);
  }
  last_parse_state_ = parse_result.state;

  // has_new_events_ should be false for the next transfer cycle.
  has_new_events_ = false;

  return typed_messages;
}

void DataStream::Reset() {
  events_.clear();
  messages_ = std::monostate();
  offset_ = 0;
  stuck_count_ = 0;
  // TODO(yzhao): It's likely the case that we'll want to preserve the inflater under the situations
  // where the HEADERS frames have not been lost. Detecting and responding to them probably will
  // change the semantic of Reset(), such that it will means different thing for different
  // protocols.
  inflater_.reset(nullptr);
}

template <typename TMessageType>
bool DataStream::Empty() const {
  return events_.empty() && (std::holds_alternative<std::monostate>(messages_) ||
                             std::get<std::deque<TMessageType>>(messages_).empty());
}

template bool DataStream::Empty<http::HTTPMessage>() const;
template bool DataStream::Empty<http2::Frame>() const;
template bool DataStream::Empty<mysql::Packet>() const;

template std::deque<http::HTTPMessage>& DataStream::ExtractMessages(MessageType type);
template std::deque<http2::Frame>& DataStream::ExtractMessages(MessageType type);
template std::deque<mysql::Packet>& DataStream::ExtractMessages(MessageType type);

}  // namespace stirling
}  // namespace pl
