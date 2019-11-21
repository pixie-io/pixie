#include "src/stirling/connection_tracker.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include <experimental/filesystem>

#include <algorithm>
#include <chrono>
#include <numeric>
#include <vector>

#include "absl/strings/numbers.h"

#include "src/common/base/inet_utils.h"
#include "src/common/system/socket_info.h"
#include "src/common/system/system.h"
#include "src/stirling/message_types.h"
#include "src/stirling/mysql/mysql_stitcher.h"

DEFINE_bool(enable_unix_domain_sockets, false, "Whether Unix domain sockets are traced or not.");
DEFINE_bool(infer_conn_info, true,
            "Whether to attempt connection information inference when remote endpoint information "
            "is missing.");

namespace pl {
namespace stirling {

//--------------------------------------------------------------
// ConnectionTracker
//--------------------------------------------------------------

template <>
void ConnectionTracker::InitState<mysql::Packet>() {
  DCHECK(std::holds_alternative<std::monostate>(state_) ||
         (std::holds_alternative<std::unique_ptr<mysql::State>>(state_)));
  if (std::holds_alternative<std::monostate>(state_)) {
    mysql::State s{std::map<int, mysql::PreparedStatement>()};
    state_ = std::make_unique<mysql::State>(std::move(s));
  }
}

void ConnectionTracker::AddControlEvent(const socket_control_event_t& event) {
  switch (event.type) {
    case kConnOpen:
      AddConnOpenEvent(event.open);
      break;
    case kConnClose:
      AddConnCloseEvent(event.close);
      break;
    default:
      LOG(DFATAL) << "Unknown control event type: " << event.type;
  }
}

void ConnectionTracker::AddConnOpenEvent(const conn_event_t& conn_event) {
  if (open_info_.timestamp_ns != 0) {
    LOG_FIRST_N(WARNING, 20) << absl::Substitute(
        "[PL-985] Clobbering existing ConnOpenEvent [pid=$0 fd=$1 gen=$2].", conn_event.conn_id.pid,
        conn_event.conn_id.fd, conn_event.conn_id.generation);
  }
  LOG_IF(WARNING, death_countdown_ >= 0 && death_countdown_ < kDeathCountdownIters - 1)
      << absl::Substitute(
             "Did not expect to receive Open event more than 1 sampling iteration after Close "
             "[pid=$0 fd=$1 gen=$2].",
             conn_event.conn_id.pid, conn_event.conn_id.fd, conn_event.conn_id.generation);

  UpdateTimestamps(conn_event.timestamp_ns);
  SetTrafficClass(conn_event.traffic_class);
  SetPID(conn_event.conn_id);

  open_info_.timestamp_ns = conn_event.timestamp_ns;
  Status s = ParseSockAddr(*reinterpret_cast<const struct sockaddr*>(&conn_event.addr),
                           &open_info_.remote_addr, &open_info_.remote_port);
  if (!s.ok()) {
    LOG(WARNING) << absl::Substitute("Could not parse IP address, msg: $0", s.msg());
    open_info_.remote_addr = "-'";
    open_info_.remote_port = 0;
  }
}

void ConnectionTracker::AddConnCloseEvent(const close_event_t& close_event) {
  if (close_info_.timestamp_ns != 0) {
    LOG_FIRST_N(ERROR, 20) << absl::Substitute(
        "Clobbering existing ConnCloseEvent [pid=$0 fd=$1 gen=$2].", close_event.conn_id.pid,
        close_event.conn_id.fd, close_event.conn_id.generation);
  }

  UpdateTimestamps(close_event.timestamp_ns);
  SetPID(close_event.conn_id);

  close_info_.timestamp_ns = close_event.timestamp_ns;
  close_info_.send_seq_num = close_event.wr_seq_num;
  close_info_.recv_seq_num = close_event.rd_seq_num;

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

  UpdateTimestamps(event->attr.return_timestamp_ns);
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

template <typename TMessageType>
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

template <typename TEntryType>
std::vector<TEntryType> ConnectionTracker::ProcessMessages() {
  if (disabled()) {
    return {};
  }
  return ProcessMessagesImpl<TEntryType>();
}

template <>
std::vector<http::Record> ConnectionTracker::ProcessMessagesImpl() {
  Status s = ExtractReqResp<http::HTTPMessage>();
  if (!s.ok()) {
    LOG(ERROR) << s.msg();
    return {};
  }

  auto& req_messages = req_data()->Messages<http::HTTPMessage>();
  auto& resp_messages = resp_data()->Messages<http::HTTPMessage>();

  // Match request response pairs.
  std::vector<http::Record> trace_records;
  for (auto req_iter = req_messages.begin(), resp_iter = resp_messages.begin();
       req_iter != req_messages.end() && resp_iter != resp_messages.end();) {
    http::HTTPMessage& req = *req_iter;
    http::HTTPMessage& resp = *resp_iter;

    if (resp.timestamp_ns < req.timestamp_ns) {
      // Oldest message is a response.
      // This means the corresponding request was not traced.
      // Push without request.
      http::Record record{http::HTTPMessage(), std::move(resp)};
      resp_messages.pop_front();
      trace_records.push_back(std::move(record));
      ++resp_iter;
    } else {
      // Found a response. It must be the match assuming:
      //  1) In-order messages (which is true for HTTP1).
      //  2) No missing messages.
      // With missing messages, there are no guarantees.
      // With no missing messages and pipelining, it's even more complicated.
      http::Record record{std::move(req), std::move(resp)};
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
    http::Record record{http::HTTPMessage(), std::move(resp)};
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
std::vector<http2::Record> ConnectionTracker::ProcessMessagesImpl() {
  Status s = ExtractReqResp<http2::Frame>();
  if (!s.ok()) {
    LOG(ERROR) << s.msg();
    return {};
  }

  std::map<uint32_t, http2::HTTP2Message> reqs;
  std::map<uint32_t, http2::HTTP2Message> resps;

  DataStream* req_stream = req_data();
  DataStream* resp_stream = resp_data();

  auto& req_messages = req_stream->Messages<http2::Frame>();
  auto& resp_messages = resp_stream->Messages<http2::Frame>();

  StitchAndInflateHeaderBlocks(req_stream->Inflater()->inflater(), &req_messages);
  StitchAndInflateHeaderBlocks(resp_stream->Inflater()->inflater(), &resp_messages);

  // First stitch all frames to form gRPC messages.
  ParseState req_stitch_state = StitchFramesToGRPCMessages(req_messages, &reqs);
  ParseState resp_stitch_state = StitchFramesToGRPCMessages(resp_messages, &resps);

  std::vector<http2::Record> records = MatchGRPCReqResp(std::move(reqs), std::move(resps));

  std::vector<http2::Record> trace_records;
  for (auto& r : records) {
    r.req.MarkFramesConsumed();
    r.resp.MarkFramesConsumed();
    http2::Record tmp{std::move(r.req), std::move(r.resp)};
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
std::vector<mysql::Record> ConnectionTracker::ProcessMessagesImpl() {
  Status s = ExtractReqResp<mysql::Packet>();
  if (!s.ok()) {
    LOG(ERROR) << s.msg();
    return {};
  }

  InitState<mysql::Packet>();

  auto& req_messages = req_data()->Messages<mysql::Packet>();
  auto& resp_messages = resp_data()->Messages<mysql::Packet>();

  auto state_ptr = state<mysql::State>();

  // ProcessMySQLPackets handles errors internally.
  std::vector<mysql::Record> result =
      mysql::ProcessMySQLPackets(&req_messages, &resp_messages, state_ptr);

  Cleanup<mysql::Packet>();

  return result;
}

void ConnectionTracker::Disable(std::string_view reason) {
  VLOG(1) << absl::Substitute("Disabling connection pid=$0 fd=$1 gen=$2 dest=$3:$4 reason=$5",
                              pid(), fd(), generation(), open_info_.remote_addr,
                              open_info_.remote_port, reason);
  disabled_ = true;
  // TODO(oazizi): Consider storing the reason field.

  send_data_.Reset();
  recv_data_.Reset();

  // TODO(oazizi): Propagate the disable back to BPF, so it doesn't even send the data.
}

bool ConnectionTracker::AllEventsReceived() const {
  return (close_info_.timestamp_ns != 0) && (num_send_events_ == close_info_.send_seq_num) &&
         (num_recv_events_ == close_info_.recv_seq_num);
}

void ConnectionTracker::SetPID(struct conn_id_t conn_id) {
  DCHECK(conn_id_.pid == 0 || conn_id_.pid == conn_id.pid);
  DCHECK(conn_id_.fd == 0 || conn_id_.fd == conn_id.fd);
  DCHECK(conn_id_.generation == 0 || conn_id_.generation == conn_id.generation)
      << absl::Substitute("Mismatched generation pid=$0 fd=$1 old gen: $2 new gen: $3",
                          conn_id_.pid, conn_id_.fd, conn_id_.generation, conn_id.generation);
  DCHECK(conn_id_.pid_start_time_ticks == 0 ||
         conn_id_.pid_start_time_ticks == conn_id.pid_start_time_ticks)
      << absl::Substitute(
             "Mismatched PID start time ticks pid=$0 fd=$1 "
             "old start time ticks: $2 new start time ticks: $3",
             conn_id_.pid, conn_id_.fd, conn_id_.pid_start_time_ticks,
             conn_id.pid_start_time_ticks);

  conn_id_.pid = conn_id.pid;
  conn_id_.pid_start_time_ticks = conn_id.pid_start_time_ticks;
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

void ConnectionTracker::IterationPreTick(system::ProcParser* proc_parser,
                                         const std::map<int, system::SocketInfo>* connections) {
  // If remote_addr is missing, it means the connect/accept was not traced.
  // Attempt to infer the connection information, to populate remote_addr.
  if (open_info_.remote_addr == "-" && FLAGS_infer_conn_info && connections != nullptr) {
    InferConnInfo(proc_parser, connections);
  }
}

void ConnectionTracker::IterationPostTick() {
  if (death_countdown_ > 0) {
    death_countdown_--;
  }

  if (std::chrono::steady_clock::now() > last_update_timestamp_ + InactivityDuration()) {
    HandleInactivity();
  }

  if (!disabled() && (send_data().IsEOS() || recv_data().IsEOS())) {
    Disable("End-of-stream");
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

void ConnectionTracker::InferConnInfo(system::ProcParser* proc_parser,
                                      const std::map<int, system::SocketInfo>* connections) {
  DCHECK(proc_parser != nullptr);
  DCHECK(connections != nullptr);

  if (conn_resolution_failed_) {
    // We've previously tried and failed to perform connection inference,
    // so don't waste any time...a connection only gets one chance.
    VLOG(2) << absl::Substitute(
        "Skipping connection inference (previous inference attempt failed, and won't try again) "
        "pid=$0 fd=$1 gen=$2",
        conn_id_.pid, conn_id_.fd, conn_id_.generation);
    return;
  }

  VLOG(2) << absl::Substitute("Attempting connection inference pid=$0 fd=$1 gen=$2", conn_id_.pid,
                              conn_id_.fd, conn_id_.generation);

  if (conn_resolver_ == nullptr) {
    conn_resolver_ = std::make_unique<SocketResolver>(proc_parser, conn_id_.pid, conn_id_.fd);
    bool success = conn_resolver_->Setup();
    if (!success) {
      conn_resolver_ = nullptr;
      conn_resolution_failed_ = true;
      VLOG(2) << "Can't infer remote endpoint. Setup failed.";
    }
    // Return after Setup(), since we won't be able to infer until some time has elapsed.
    // This is because file descriptors can be re-used, and if we sample the FD just once,
    // we don't know which generation of the FD we've captured.
    // Waiting some time allows us to build some understanding of whether the FD is stable
    // at the time that we're interested at.
    return;
  }

  bool success = conn_resolver_->Update();
  if (!success) {
    conn_resolver_ = nullptr;
    conn_resolution_failed_ = true;
    VLOG(2) << "Can't infer remote endpoint. Could not determine socket inode number of FD.";
    return;
  }

  std::optional<int> inode_num_or_nullopt = conn_resolver_->InferSocket(last_update_timestamp_);
  if (!inode_num_or_nullopt) {
    return;
  }
  int inode_num = *inode_num_or_nullopt;

  // We found the inode number, now lets see if it maps to a known connection.
  auto iter = connections->find(inode_num);
  if (iter == connections->end()) {
    VLOG(2) << "Can't infer remote endpoint. No inode match (possibly not a TCP connection).";
    return;
  }

  // Success! Now copy the inferred socket information into the ConnectionTracker.
  const system::SocketInfo& socket_info = iter->second;

  bool unix_domain_socket = false;
  Status s;
  switch (socket_info.family) {
    case AF_INET:
      s = ParseIPv4Addr(socket_info.remote_addr, &open_info_.remote_addr);
      open_info_.remote_port = ntohs(socket_info.remote_port);
      if (!s.ok()) {
        LOG(ERROR) << absl::Substitute("IP parsing failed [msg=$0]", s.msg());
        return;
      }
      break;
    case AF_INET6:
      s = ParseIPv6Addr(socket_info.remote_addr, &open_info_.remote_addr);
      open_info_.remote_port = ntohs(socket_info.remote_port);
      if (!s.ok()) {
        LOG(ERROR) << absl::Substitute("IP parsing failed [msg=$0]", s.msg());
        return;
      }
      break;
    case AF_UNIX:
      // TODO(oazizi): This actually records the source inode number. Should actually populate the
      // remote peer.
      open_info_.remote_addr = "unix_socket";
      open_info_.remote_port = inode_num;
      unix_domain_socket = true;
      break;
    default:
      LOG(DFATAL) << absl::Substitute("Unexpected family $0", socket_info.family);
  }

  LOG(INFO) << absl::Substitute("Inferred connection pid=$0 fd=$1 gen=$2 dest=$3:$4", pid(), fd(),
                                generation(), open_info_.remote_addr, open_info_.remote_port);

  // TODO(oazizi): Move this out of this function, since it is not a part of the inference.
  // I don't like side-effects.
  if (unix_domain_socket && !FLAGS_enable_unix_domain_sockets) {
    // Turns out we've been tracing a Unix domain socket, so disable tracker to stop tracing it.
    Disable("Unix domain socket");
  }

  // No need for the resolver anymore, so free its memory.
  conn_resolver_.reset();
}

template <typename TEntryType>
std::string ConnectionTracker::DebugString(std::string_view prefix) const {
  std::string info;
  info += absl::Substitute("$0pid=$1 fd=$2 gen=$3\n", prefix, pid(), fd(), generation());
  info += absl::Substitute("$0remote_addr=$1:$2\n", prefix, remote_addr(), remote_port());
  info += absl::Substitute("$0protocol=$1\n", prefix, ProtocolName(protocol()));
  info += absl::Substitute("$0recv queue\n", prefix);
  info += recv_data().DebugString<typename GetMessageType<TEntryType>::type>(
      absl::StrCat(prefix, "  "));
  info += absl::Substitute("$0send queue\n", prefix);
  info += send_data().DebugString<typename GetMessageType<TEntryType>::type>(
      absl::StrCat(prefix, "  "));
  return info;
}

template std::vector<http::Record> ConnectionTracker::ProcessMessages();
template std::vector<http2::Record> ConnectionTracker::ProcessMessages();
template std::vector<mysql::Record> ConnectionTracker::ProcessMessages();

template std::string ConnectionTracker::DebugString<http::Record>(std::string_view prefix) const;
template std::string ConnectionTracker::DebugString<http2::Record>(std::string_view prefix) const;
template std::string ConnectionTracker::DebugString<mysql::Record>(std::string_view prefix) const;

}  // namespace stirling
}  // namespace pl
