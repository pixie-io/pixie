#include "src/stirling/connection_tracker.h"

#include <arpa/inet.h>
#include <netinet/in.h>

#include <experimental/filesystem>

#include <algorithm>
#include <chrono>
#include <numeric>
#include <vector>

#include "src/common/base/inet_utils.h"
#include "src/common/system/system.h"
#include "src/stirling/http2.h"
#include "src/stirling/mysql/mysql.h"
#include "src/stirling/mysql/mysql_stitcher.h"

// TODO(oazizi/yzhao): We also need the age-based purging, such that if there are a lot of
// connections slowly accumulating messages, we'll be purge them as well.
DEFINE_uint32(messages_size_limit_bytes, 1024 * 1024,
              "The limit of the size of the parsed messages, not the BPF events, "
              "for each direction, of each connection tracker. "
              "All cached messages are erased if this limit is breached.");

namespace pl {
namespace stirling {

template <>
void ConnectionTracker::InitState<mysql::Packet>() {
  DCHECK(std::holds_alternative<std::monostate>(state_) ||
         (std::holds_alternative<std::unique_ptr<mysql::State>>(state_)));
  if (std::holds_alternative<std::monostate>(state_)) {
    mysql::State s{std::map<int, mysql::ReqRespEvent>(), mysql::FlagStatus::kUnknown};
    state_ = std::make_unique<mysql::State>(std::move(s));
  }
}

//--------------------------------------------------------------
// ConnectionTracker
//--------------------------------------------------------------

void ConnectionTracker::AddConnOpenEvent(conn_info_t conn_info) {
  LOG_IF(ERROR, open_info_.timestamp_ns != 0) << "Clobbering existing ConnOpenEvent.";
  LOG_IF(WARNING, death_countdown_ >= 0 && death_countdown_ < kDeathCountdownIters - 1)
      << absl::Substitute(
             "Did not expect to receive Open event more than 1 sampling iteration after Close "
             "[pid=$0 fd=$1 gen=$2].",
             conn_info.conn_id.pid, conn_info.conn_id.fd, conn_info.conn_id.generation);

  UpdateTimestamps(conn_info.timestamp_ns);
  SetTrafficClass(conn_info.traffic_class);
  SetPID(conn_info.conn_id);

  open_info_.timestamp_ns = conn_info.timestamp_ns;
  Status s = ParseSockAddr(*reinterpret_cast<struct sockaddr*>(&conn_info.addr),
                           &open_info_.remote_addr, &open_info_.remote_port);
  if (!s.ok()) {
    LOG(WARNING) << "Could not parse IP address.";
    open_info_.remote_addr = "-'";
    open_info_.remote_port = 0;
  }
}

void ConnectionTracker::AddConnCloseEvent(conn_info_t conn_info) {
  LOG_IF(ERROR, close_info_.timestamp_ns != 0) << "Clobbering existing ConnCloseEvent";

  UpdateTimestamps(conn_info.timestamp_ns);
  SetPID(conn_info.conn_id);

  close_info_.timestamp_ns = conn_info.timestamp_ns;
  close_info_.send_seq_num = conn_info.wr_seq_num;
  close_info_.recv_seq_num = conn_info.rd_seq_num;

  MarkForDeath();
}

void ConnectionTracker::AddDataEvent(std::unique_ptr<SocketDataEvent> event) {
  // A disabled tracker doesn't collect data events.
  if (disabled_) {
    return;
  }

  LOG_IF(WARNING, death_countdown_ >= 0 && death_countdown_ < kDeathCountdownIters - 1)
      << absl::Substitute(
             "Did not expect to receive Data event more than 1 sampling iteration after Close "
             "[pid=$0 fd=$1 gen=$2].",
             event->attr.conn_id.pid, event->attr.conn_id.fd, event->attr.conn_id.generation);

  UpdateTimestamps(event->attr.timestamp_ns);
  SetPID(event->attr.conn_id);
  SetTrafficClass(event->attr.traffic_class);

  switch (event->attr.direction) {
    case TrafficDirection::kEgress: {
      send_data_.AddEvent(std::move(event));
      ++num_send_events_;
    } break;
    case TrafficDirection::kIngress: {
      recv_data_.AddEvent(std::move(event));
      ++num_recv_events_;
    } break;
  }
}

template <class TMessageType>
Status ConnectionTracker::ExtractReqResp() {
  DataStream* resp_data_ptr = resp_data();
  if (resp_data_ptr == nullptr) {
    return error::Internal("Unexpected nullptr for resp_data");
  }
  resp_data_ptr->template ExtractMessages<TMessageType>(MessageType::kResponse);

  DataStream* req_data_ptr = req_data();
  if (req_data_ptr == nullptr) {
    return error::Internal("Unexpected nullptr for req_data");
  }
  req_data_ptr->template ExtractMessages<TMessageType>(MessageType::kRequest);

  return Status::OK();
}

template <class TMessageType>
std::vector<TMessageType> ConnectionTracker::ProcessMessages() {
  if (disabled()) {
    return {};
  }
  return ProcessMessagesImpl<TMessageType>();
}

template <>
std::vector<ReqRespPair<http::HTTPMessage>> ConnectionTracker::ProcessMessagesImpl() {
  Status s = ExtractReqResp<http::HTTPMessage>();
  if (!s.ok()) {
    LOG(ERROR) << s.msg();
    return {};
  }

  auto& req_messages = req_data()->Messages<http::HTTPMessage>();
  auto& resp_messages = resp_data()->Messages<http::HTTPMessage>();

  // Match request response pairs.
  std::vector<ReqRespPair<http::HTTPMessage>> trace_records;
  for (auto req_iter = req_messages.begin(), resp_iter = resp_messages.begin();
       req_iter != req_messages.end() && resp_iter != resp_messages.end();) {
    http::HTTPMessage& req = *req_iter;
    http::HTTPMessage& resp = *resp_iter;

    if (resp.timestamp_ns < req.timestamp_ns) {
      // Oldest message is a response.
      // This means the corresponding request was not traced.
      // Push without request.
      ReqRespPair<http::HTTPMessage> record{http::HTTPMessage(), std::move(resp)};
      resp_messages.pop_front();
      trace_records.push_back(std::move(record));
      ++resp_iter;
    } else {
      // Found a response. It must be the match assuming:
      //  1) In-order messages (which is true for HTTP1).
      //  2) No missing messages.
      // With missing messages, there are no guarantees.
      // With no missing messages and pipelining, it's even more complicated.
      ReqRespPair<http::HTTPMessage> record{std::move(req), std::move(resp)};
      req_messages.pop_front();
      resp_messages.pop_front();
      trace_records.push_back(std::move(record));
      ++resp_iter;
      ++req_iter;
    }
  }

  // Any leftover responses must have lost their requests.
  for (auto resp_iter = resp_messages.begin(); resp_iter != resp_messages.end(); ++resp_iter) {
    http::HTTPMessage& resp = *resp_iter;
    ReqRespPair<http::HTTPMessage> record{http::HTTPMessage(), std::move(resp)};
    trace_records.push_back(std::move(record));
  }
  resp_messages.clear();

  // Any leftover requests are left around for the next iteration,
  // since the response may not have been traced yet.
  // TODO(oazizi): If we have seen the close event, then can assume the response is lost.
  //               We should push the event out in such cases.

  Cleanup<http::HTTPMessage>();

  return trace_records;
}

template <>
std::vector<ReqRespPair<http2::GRPCMessage>> ConnectionTracker::ProcessMessagesImpl() {
  Status s = ExtractReqResp<http2::Frame>();
  if (!s.ok()) {
    LOG(ERROR) << s.msg();
    return {};
  }

  std::map<uint32_t, http2::GRPCMessage> reqs;
  std::map<uint32_t, http2::GRPCMessage> resps;

  DataStream* req_stream = req_data();
  DataStream* resp_stream = resp_data();

  auto& req_messages = req_stream->Messages<http2::Frame>();
  auto& resp_messages = resp_stream->Messages<http2::Frame>();

  // First stitch all frames to form gRPC messages.
  ParseState req_stitch_state =
      StitchFramesToGRPCMessages(req_messages, req_stream->Inflater(), &reqs);
  ParseState resp_stitch_state =
      StitchFramesToGRPCMessages(resp_messages, resp_stream->Inflater(), &resps);

  std::vector<http2::GRPCReqResp> records = MatchGRPCReqResp(std::move(reqs), std::move(resps));

  std::vector<ReqRespPair<http2::GRPCMessage>> trace_records;
  for (auto& r : records) {
    r.req.MarkFramesConsumed();
    r.resp.MarkFramesConsumed();
    ReqRespPair<http2::GRPCMessage> tmp{std::move(r.req), std::move(r.resp)};
    trace_records.push_back(tmp);
  }

  http2::EraseConsumedFrames(&req_messages);
  http2::EraseConsumedFrames(&resp_messages);

  // Reset streams, if necessary, after erasing the consumed frames. Otherwise, frames will be
  // deleted twice.
  if (req_stitch_state != ParseState::kSuccess) {
    LOG(ERROR) << "Failed to stitch frames to gRPC messages, indicating fatal errors, "
                  "resetting the request stream ...";
    // TODO(PL-916): We observed that if messages are truncated, some repeating bytes sequence
    // in the http2 traffic is wrongly recognized as frame headers. We need to recognize truncated
    // events and try to recover from it, instead of relying stream recovery.
    // TODO(yzhao): Add an e2e test of the stream resetting behavior on SocketTraceConnector without
    // using the gRPC test fixtures, i.e., prepare raw events and feed into SocketTraceConnector.
    req_stream->Reset();
  }
  if (resp_stitch_state != ParseState::kSuccess) {
    LOG(ERROR) << "Failed to stitch frames to gRPC messages, indicating fatal errors, "
                  "resetting the response stream ...";
    resp_stream->Reset();
  }

  // TODO(yzhao): Template makes the type parameter not working for gRPC, as gRPC returns different
  // type than the type parameter. Figure out how to mitigate the conflicts, so this call can be
  // lifted to ProcessMessages().
  Cleanup<http2::Frame>();

  return trace_records;
}

template <>
std::vector<mysql::Entry> ConnectionTracker::ProcessMessagesImpl() {
  std::vector<mysql::Entry> entries;

  Status s = ExtractReqResp<mysql::Packet>();
  if (!s.ok()) {
    LOG(ERROR) << s.msg();
    return entries;
  }

  InitState<mysql::Packet>();

  auto& req_messages = req_data()->Messages<mysql::Packet>();
  auto& resp_messages = resp_data()->Messages<mysql::Packet>();

  auto state_ptr = state<mysql::State>();
  auto result = mysql::StitchMySQLPackets(&req_messages, &resp_messages, state_ptr);

  Cleanup<mysql::Packet>();

  return result;
}

// TODO(oazizi): Consider providing a reason field with the disable.
void ConnectionTracker::Disable() {
  disabled_ = true;
  send_data_.Reset();
  recv_data_.Reset();
}

bool ConnectionTracker::AllEventsReceived() const {
  return (close_info_.timestamp_ns != 0) && (num_send_events_ == close_info_.send_seq_num) &&
         (num_recv_events_ == close_info_.recv_seq_num);
}

void ConnectionTracker::SetPID(struct conn_id_t conn_id) {
  DCHECK(conn_id_.pid == 0 || conn_id_.pid == conn_id.pid);
  DCHECK(conn_id_.pid_start_time_ns == 0 ||
         conn_id_.pid_start_time_ns == conn_id.pid_start_time_ns);
  DCHECK(conn_id_.fd == 0 || conn_id_.fd == conn_id.fd);
  DCHECK(conn_id_.generation == 0 || conn_id_.generation == conn_id.generation);

  conn_id_.pid = conn_id.pid;
  conn_id_.pid_start_time_ns = conn_id.pid_start_time_ns;
  conn_id_.fd = conn_id.fd;
  conn_id_.generation = conn_id.generation;
}

void ConnectionTracker::SetTrafficClass(struct traffic_class_t traffic_class) {
  DCHECK((traffic_class_.protocol == kProtocolUnknown) == (traffic_class_.role == kRoleUnknown));

  if (traffic_class_.protocol == kProtocolUnknown) {
    traffic_class_ = traffic_class;
  } else if (traffic_class.protocol != kProtocolUnknown) {
    DCHECK_EQ(traffic_class.protocol, traffic_class.protocol)
        << "Not allowed to change the protocol of an active ConnectionTracker";
    DCHECK_EQ(traffic_class.role, traffic_class.role)
        << "Not allowed to change the role of an active ConnectionTracker";
  }
}

void ConnectionTracker::UpdateTimestamps(uint64_t bpf_timestamp) {
  last_bpf_timestamp_ns_ = std::max(last_bpf_timestamp_ns_, bpf_timestamp);

  last_update_timestamp_ = std::chrono::steady_clock::now();
}

DataStream* ConnectionTracker::req_data() {
  switch (traffic_class_.role) {
    case kRoleRequestor:
      return &send_data_;
    case kRoleResponder:
      return &recv_data_;
    default:
      return nullptr;
  }
}

DataStream* ConnectionTracker::resp_data() {
  switch (traffic_class_.role) {
    case kRoleRequestor:
      return &recv_data_;
    case kRoleResponder:
      return &send_data_;
    default:
      return nullptr;
  }
}

void ConnectionTracker::MarkForDeath(int32_t countdown) {
  // We received the close event.
  // Now give up to some more TransferData calls to receive trailing data events.
  // We do this for logging/debug purposes only.
  if (death_countdown_ >= 0) {
    death_countdown_ = std::min(death_countdown_, countdown);
  } else {
    death_countdown_ = countdown;
  }
}

bool ConnectionTracker::IsZombie() const { return death_countdown_ >= 0; }

bool ConnectionTracker::ReadyForDestruction() const {
  // We delay destruction time by a few iterations.
  // See also MarkForDeath().
  return death_countdown_ == 0;
}

void ConnectionTracker::IterationTick() {
  if (death_countdown_ > 0) {
    death_countdown_--;
  }

  if (std::chrono::steady_clock::now() > last_update_timestamp_ + InactivityDuration()) {
    HandleInactivity();
  }

  if (send_data().IsEOS() || recv_data().IsEOS()) {
    Disable();
  }
}

void ConnectionTracker::HandleInactivity() {
  static const auto& sysconfig = system::Config::GetInstance();
  std::experimental::filesystem::path fd_file =
      absl::StrCat(sysconfig.proc_path(), absl::Substitute("/$0/fd/$1", pid(), fd()));

  if (!std::experimental::filesystem::exists(fd_file)) {
    // Connection seems to be dead. Mark for immediate death.
    MarkForDeath(0);
  } else {
    // Connection may still be alive (though inactive), so flush the data buffers.
    // It is unlikely any new data is a continuation of existing data in in any meaningful way.
    send_data_.Reset();
    recv_data_.Reset();
  }
}

//--------------------------------------------------------------
// DataStream
//--------------------------------------------------------------

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
  LOG_IF(ERROR, !res.second) << "Clobbering data event";
  has_new_events_ = true;
}

template <class TMessageType>
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

template <class TMessageType>
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

    parser->Append(msg, event.timestamp_ns);

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
template <class TMessageType>
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
  // TODO(yzhao): It's likely the case that we'll want to preserve the infalter under the situations
  // where the HEADERS frames have not been lost. Detecting and responding to them probably will
  // change the semantic of Reset(), such that it will means different thing for different
  // protocols.
  inflater_.reset(nullptr);
}

template <class TMessageType>
bool DataStream::Empty() const {
  return events_.empty() && (std::holds_alternative<std::monostate>(messages_) ||
                             std::get<std::deque<TMessageType>>(messages_).empty());
}

template std::vector<ReqRespPair<http::HTTPMessage>> ConnectionTracker::ProcessMessages();
template std::vector<ReqRespPair<http2::GRPCMessage>> ConnectionTracker::ProcessMessages();
template std::vector<mysql::Entry> ConnectionTracker::ProcessMessages();

template bool DataStream::Empty<http::HTTPMessage>() const;
template bool DataStream::Empty<http2::Frame>() const;
template bool DataStream::Empty<mysql::Packet>() const;

}  // namespace stirling
}  // namespace pl
