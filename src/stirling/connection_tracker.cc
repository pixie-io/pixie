#include <experimental/filesystem>

#include <algorithm>
#include <chrono>
#include <vector>

#include "src/common/system/system.h"
#include "src/stirling/connection_tracker.h"
#include "src/stirling/http2.h"

namespace pl {
namespace stirling {

void ConnectionTracker::AddConnOpenEvent(conn_info_t conn_info) {
  LOG_IF(ERROR, open_info_.timestamp_ns != 0) << "Clobbering existing ConnOpenEvent.";
  LOG_IF(WARNING, death_countdown_ >= 0 && death_countdown_ <= kDeathCountdownIters)
      << absl::Substitute(
             "Did not expect to receive Open event after Close [PID=$0, FD=$1, generation=$2].",
             conn_info.conn_id.pid, conn_info.conn_id.fd, conn_info.conn_id.generation);

  UpdateTimestamps(conn_info.timestamp_ns);
  SetTrafficClass(conn_info.traffic_class);
  SetPID(conn_info.conn_id);

  open_info_.timestamp_ns = conn_info.timestamp_ns;
  auto ip_endpoint_or = ParseSockAddr(conn_info);
  if (ip_endpoint_or.ok()) {
    open_info_.remote_addr = std::move(ip_endpoint_or.ValueOrDie().ip);
    open_info_.remote_port = ip_endpoint_or.ValueOrDie().port;
  } else {
    LOG(WARNING) << "Could not parse IP address.";
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
  LOG_IF(WARNING, death_countdown_ >= 0 && death_countdown_ <= kDeathCountdownIters)
      << absl::Substitute(
             "Did not expect to receive Data event after Close [PID=$0, FD=$1, generation=$2].",
             event->attr.conn_id.pid, event->attr.conn_id.fd, event->attr.conn_id.generation);

  UpdateTimestamps(event->attr.timestamp_ns);
  SetPID(event->attr.conn_id);
  SetTrafficClass(event->attr.traffic_class);

  const uint64_t seq_num = event->attr.seq_num;

  switch (event->attr.direction) {
    case TrafficDirection::kEgress: {
      send_data_.AddEvent(seq_num, std::move(event));
      ++num_send_events_;
    } break;
    case TrafficDirection::kIngress: {
      recv_data_.AddEvent(seq_num, std::move(event));
      ++num_recv_events_;
    } break;
  }
}

template <class TMessageType>
Status ConnectionTracker::ExtractMessages() {
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

void DataStream::AddEvent(uint64_t seq_num, std::unique_ptr<SocketDataEvent> event) {
  if (seq_num < next_seq_num_) {
    LOG(WARNING) << absl::Substitute(
        "Ignoring event has already been skipped [event seq_num=$0, current seq_num=$1].", seq_num,
        next_seq_num_);
    return;
  }
  auto res = events_.emplace(seq_num, TimestampedData(std::move(event)));
  LOG_IF(ERROR, !res.second) << "Clobbering data event";
  has_new_events_ = true;
}

template <class TMessageType>
std::deque<TMessageType>& DataStream::Messages() {
  CHECK(std::holds_alternative<std::monostate>(messages_) ||
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
std::deque<TMessageType>& DataStream::ExtractMessages(MessageType type) {
  auto& typed_messages = Messages<TMessageType>();

  // If no new raw data, then nothing extra to extract. Exit early.
  if (!has_new_events_) {
    return typed_messages;
  }

  EventParser<TMessageType> parser;

  const size_t orig_offset = offset_;

  // Prepare all recorded events for parsing.
  std::vector<std::string_view> msgs;
  size_t next_seq_num = next_seq_num_;
  for (const auto& [seq_num, event] : events_) {
    // Not at expected seq_num. Stop submitting events to parser.
    if (seq_num != next_seq_num) {
      break;
    }

    // The main message to submit to parser.
    std::string_view msg = event.msg;

    // First message may have been partially processed by a previous call to this function.
    // In such cases, the offset will be non-zero, and we need a sub-string of the first event.
    if (offset_ != 0) {
      CHECK(offset_ < event.msg.size());
      msg = msg.substr(offset_, event.msg.size() - offset_);
      offset_ = 0;
    }

    parser.Append(msg, event.timestamp_ns);
    msgs.push_back(msg);
    ++next_seq_num;
  }

  // Now parse all the appended events.
  ParseResult<BufferPosition> parse_result = parser.ParseMessages(type, &typed_messages);

  // If we weren't able to process anything new, then the offset should be the same as last time.
  if (offset_ != 0 && parse_result.end_position.seq_num == 0) {
    CHECK_EQ(parse_result.end_position.offset, orig_offset);
  }

  // Find and erase events that have been fully processed.
  // Note that parse_result seq_nums are based on events added to parser,
  // not seq_nums from BPF.
  auto erase_iter = events_.begin();
  std::advance(erase_iter, parse_result.end_position.seq_num);
  events_.erase(events_.begin(), erase_iter);
  next_seq_num_ += parse_result.end_position.seq_num;
  offset_ = parse_result.end_position.offset;

  has_new_events_ = false;

  return typed_messages;
}

void DataStream::Reset() {
  events_.clear();
  messages_ = std::monostate();
  offset_ = 0;
}

template <class TMessageType>
bool DataStream::Empty() const {
  return events_.empty() && (std::holds_alternative<std::monostate>(messages_) ||
                             std::get<std::deque<TMessageType>>(messages_).empty());
}

// Explicit instantiation different message types.
template Status ConnectionTracker::ExtractMessages<HTTPMessage>();
template Status ConnectionTracker::ExtractMessages<http2::Frame>();

template std::deque<HTTPMessage>& DataStream::ExtractMessages<HTTPMessage>(MessageType type);
template std::deque<http2::Frame>& DataStream::ExtractMessages<http2::Frame>(MessageType type);

template std::deque<HTTPMessage>& DataStream::Messages<HTTPMessage>();
template std::deque<http2::Frame>& DataStream::Messages<http2::Frame>();

template bool DataStream::Empty<HTTPMessage>() const;
template bool DataStream::Empty<http2::Frame>() const;

}  // namespace stirling
}  // namespace pl
