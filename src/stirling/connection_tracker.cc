#include "src/stirling/connection_tracker.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <numeric>
#include <vector>

#include <absl/strings/numbers.h>
#include <magic_enum.hpp>

#include "src/common/base/inet_utils.h"
#include "src/common/system/socket_info.h"
#include "src/common/system/system.h"
#include "src/stirling/common/go_grpc.h"
#include "src/stirling/message_types.h"
#include "src/stirling/mysql/mysql_stitcher.h"

DEFINE_bool(enable_unix_domain_sockets, false, "Whether Unix domain sockets are traced or not.");
DEFINE_bool(infer_conn_info, true,
            "Whether to attempt connection inference when remote endpoint information is missing.");
DEFINE_uint32(stirling_http2_stream_id_gap_threshold, 100,
              "If a stream ID jumps by this many spots or more, an error is assumed and the entire "
              "connection info is cleared.");

namespace pl {
namespace stirling {

namespace {
std::string ToString(const conn_id_t& conn_id) {
  return absl::Substitute("[pid=$0 start_time_ticks=$1 fd=$2 gen=$3]", conn_id.upid.pid,
                          conn_id.upid.start_time_ticks, conn_id.fd, conn_id.generation);
}
}  // namespace

//--------------------------------------------------------------
// ConnectionTracker
//--------------------------------------------------------------

template <>
void ConnectionTracker::InitProtocolState<mysql::Packet>() {
  DCHECK(std::holds_alternative<std::monostate>(protocol_state_) ||
         (std::holds_alternative<std::unique_ptr<mysql::State>>(protocol_state_)));
  if (std::holds_alternative<std::monostate>(protocol_state_)) {
    mysql::State s{std::map<int, mysql::PreparedStatement>()};
    protocol_state_ = std::make_unique<mysql::State>(std::move(s));
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
    LOG_FIRST_N(WARNING, 20) << absl::Substitute("[PL-985] Clobbering existing ConnOpenEvent $0.",
                                                 ToString(conn_event.conn_id));
  }

  SetConnID(conn_event.conn_id);
  SetTrafficClass(conn_event.traffic_class);

  CheckTracker();

  UpdateTimestamps(conn_event.timestamp_ns);

  open_info_.timestamp_ns = conn_event.timestamp_ns;

  Status s = ParseSockAddr(reinterpret_cast<const struct sockaddr*>(&conn_event.addr),
                           &open_info_.remote_addr);
  if (!s.ok()) {
    LOG(WARNING) << absl::Substitute("Could not parse IP address, msg: $0", s.msg());
  }
}

void ConnectionTracker::AddConnCloseEvent(const close_event_t& close_event) {
  if (close_info_.timestamp_ns != 0) {
    LOG_FIRST_N(ERROR, 20) << absl::Substitute("Clobbering existing ConnCloseEvent $0.",
                                               ToString(close_event.conn_id));
  }

  SetConnID(close_event.conn_id);

  CheckTracker();

  UpdateTimestamps(close_event.timestamp_ns);

  close_info_.timestamp_ns = close_event.timestamp_ns;
  close_info_.send_seq_num = close_event.wr_seq_num;
  close_info_.recv_seq_num = close_event.rd_seq_num;

  MarkForDeath();
}

void ConnectionTracker::AddDataEvent(std::unique_ptr<SocketDataEvent> event) {
  SetConnID(event->attr.conn_id);
  SetTrafficClass(event->attr.traffic_class);

  // A disabled tracker doesn't collect data events.
  if (disabled()) {
    return;
  }

  CheckTracker();
  UpdateTimestamps(event->attr.return_timestamp_ns);

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

http2::HalfStream* ConnectionTracker::HalfStreamPtr(uint32_t stream_id, bool write_event) {
  // Check for both client-initiated (odd stream_ids) and server-initiated (even stream_ids)
  // streams.
  std::deque<http2::Stream>* streams_deque_ptr;
  uint32_t* oldest_active_stream_id_ptr;

  bool client_stream = (stream_id % 2 == 1);

  if (client_stream) {
    streams_deque_ptr = &(client_streams_.Messages<http2::Stream>());
    oldest_active_stream_id_ptr = &oldest_active_client_stream_id_;
  } else {
    streams_deque_ptr = &(server_streams_.Messages<http2::Stream>());
    oldest_active_stream_id_ptr = &oldest_active_server_stream_id_;
  }

  // Update the head index.
  if (streams_deque_ptr->empty()) {
    *oldest_active_stream_id_ptr = stream_id;
  }

  size_t index;
  if (stream_id < *oldest_active_stream_id_ptr) {
    LOG(WARNING) << absl::Substitute(
        "Stream ID ($0) is lower than the current head stream ID ($1). "
        "Not expected, but will handle it anyways. If not a data race, "
        "this could be indicative of a bug that could result in a memory leak.",
        stream_id, *oldest_active_stream_id_ptr);
    // Need to grow the deque at the front.
    size_t new_size = streams_deque_ptr->size() +
                      ((*oldest_active_stream_id_ptr - stream_id) / kHTTP2StreamIDIncrement);
    // Reset everything for now.
    if (new_size - streams_deque_ptr->size() > FLAGS_stirling_http2_stream_id_gap_threshold) {
      LOG(ERROR) << absl::Substitute(
          "Encountered a stream ID $0 that is too far from the last known stream ID $1. Resetting "
          "all streams on this connection.",
          stream_id, *oldest_active_stream_id_ptr + streams_deque_ptr->size() * 2);
      streams_deque_ptr->clear();
      streams_deque_ptr->resize(1);
      index = 0;
      *oldest_active_stream_id_ptr = stream_id;
    } else {
      streams_deque_ptr->insert(streams_deque_ptr->begin(), new_size - streams_deque_ptr->size(),
                                http2::Stream());
      index = 0;
      *oldest_active_stream_id_ptr = stream_id;
    }
  } else {
    // Stream ID is after the front. We may or may not need to grow the deque,
    // depending on its current size.
    index = (stream_id - *oldest_active_stream_id_ptr) / kHTTP2StreamIDIncrement;
    size_t new_size = std::max(streams_deque_ptr->size(), index + 1);
    // If we are to grow by more than some threshold, then something appears wrong.
    // Reset everything for now.
    if (new_size - streams_deque_ptr->size() > FLAGS_stirling_http2_stream_id_gap_threshold) {
      LOG(ERROR) << absl::Substitute(
          "Encountered a stream ID $0 that is too far from the last known stream ID $1. Resetting "
          "all streams on this connection",
          stream_id, *oldest_active_stream_id_ptr + streams_deque_ptr->size() * 2);
      streams_deque_ptr->clear();
      streams_deque_ptr->resize(1);
      index = 0;
      *oldest_active_stream_id_ptr = stream_id;
    } else {
      streams_deque_ptr->resize(new_size);
    }
  }

  auto& stream = (*streams_deque_ptr)[index];

  // TODO(yzhao): This is really tedious. But we do not want to create another bool flag inside
  // Stream either. Investigate if there is easier way to check if creation_timestamp is initialized
  // or not.
  if (stream.creation_timestamp.time_since_epoch().count() == 0) {
    stream.creation_timestamp = std::chrono::steady_clock::now();
  }

  http2::HalfStream* half_stream_ptr = write_event ? &stream.send : &stream.recv;
  return half_stream_ptr;
}

void ConnectionTracker::AddHTTP2Header(std::unique_ptr<HTTP2HeaderEvent> hdr) {
  SetConnID(hdr->attr.conn_id);
  traffic_class_.protocol = kProtocolHTTP2Uprobe;

  if (conn_id_.fd == 0) {
    Disable(
        "FD of zero is usually not valid. One reason for could be that net.Conn could not be "
        "accessed because of incorrect type information. If this is the case, the dwarf feature "
        "should fix this.");
  }

  // A disabled tracker doesn't collect data events.
  if (disabled()) {
    return;
  }

  CheckTracker();

  // Don't trace any control messages.
  if (hdr->attr.stream_id == 0) {
    return;
  }

  UpdateTimestamps(hdr->attr.timestamp_ns);

  bool write_event = false;
  switch (hdr->attr.type) {
    case HeaderEventType::kHeaderEventWrite:
      write_event = true;
      break;
    case HeaderEventType::kHeaderEventRead:
      write_event = false;
      break;
    default:
      LOG(WARNING) << "Unexpected event type";
      return;
  }

  http2::HalfStream* half_stream_ptr = HalfStreamPtr(hdr->attr.stream_id, write_event);

  // End stream flag is on a dummy header, so just record the end_stream, but don't add the headers.
  if (hdr->attr.end_stream) {
    // Expecting a dummy (empty) header since this is how it is done in BPF.
    ECHECK(hdr->name.empty());
    ECHECK(hdr->value.empty());

    // Only expect one end_stream signal per stream direction.
    ECHECK(!half_stream_ptr->end_stream) << absl::Substitute(
        "stream_id: $0, conn_id: $1", hdr->attr.stream_id, ToString(hdr->attr.conn_id));

    half_stream_ptr->end_stream = true;
    return;
  }

  half_stream_ptr->headers.emplace(std::move(hdr->name), std::move(hdr->value));
  half_stream_ptr->UpdateTimestamp(hdr->attr.timestamp_ns);
}

void ConnectionTracker::AddHTTP2Data(std::unique_ptr<HTTP2DataEvent> data) {
  SetConnID(data->attr.conn_id);
  traffic_class_.protocol = kProtocolHTTP2Uprobe;

  if (conn_id_.fd == 0) {
    Disable(
        "FD of zero is usually not valid. One reason for could be that net.Conn could not be "
        "accessed because of incorrect type information. If this is the case, the dwarf feature "
        "should fix this.");
  }

  // A disabled tracker doesn't collect data events.
  if (disabled()) {
    return;
  }

  CheckTracker();

  // Don't trace any control messages.
  if (data->attr.stream_id == 0) {
    return;
  }

  UpdateTimestamps(data->attr.timestamp_ns);

  bool write_event = false;
  switch (data->attr.type) {
    case DataFrameEventType::kDataFrameEventWrite:
      write_event = true;
      break;
    case DataFrameEventType::kDataFrameEventRead:
      write_event = false;
      break;
    default:
      LOG(WARNING) << "Unexpected event type";
      return;
  }

  http2::HalfStream* half_stream_ptr = HalfStreamPtr(data->attr.stream_id, write_event);
  half_stream_ptr->data += data->payload;
  half_stream_ptr->end_stream |= data->attr.end_stream;
  half_stream_ptr->UpdateTimestamp(data->attr.timestamp_ns);
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

namespace {
void ProcessHTTP2Streams(DataStream* stream_ptr, uint32_t* oldest_active_stream_id_ptr,
                         std::vector<http2::NewRecord>* trace_records) {
  auto& http2_streams = stream_ptr->Messages<http2::Stream>();

  int count_head_consumed = 0;
  bool skipped = false;
  for (auto& stream : http2_streams) {
    if (stream.StreamEnded() && !stream.consumed) {
      trace_records->emplace_back(http2::NewRecord{std::move(stream)});
      stream.consumed = true;
    }

    // TODO(oazizi): If a stream is not ended, but looks stuck,
    // we should force process it and mark it as consumed.
    // Otherwise we will have a memory leak.

    if (!stream.consumed) {
      skipped = true;
    }

    if (!skipped && stream.consumed) {
      ++count_head_consumed;
    }
  }

  // Erase contiguous set of consumed streams at head.
  http2_streams.erase(http2_streams.begin(), http2_streams.begin() + count_head_consumed);
  *oldest_active_stream_id_ptr += 2 * count_head_consumed;
}
}  // namespace

template <>
std::vector<http2::NewRecord> ConnectionTracker::ProcessMessagesImpl() {
  // TODO(oazizi): ECHECK that raw events are empty.

  std::vector<http2::NewRecord> trace_records;

  ProcessHTTP2Streams(&client_streams_, &oldest_active_client_stream_id_, &trace_records);
  ProcessHTTP2Streams(&server_streams_, &oldest_active_server_stream_id_, &trace_records);

  Cleanup<http2::Stream>();

  return trace_records;
}

template <>
std::vector<mysql::Record> ConnectionTracker::ProcessMessagesImpl() {
  Status s = ExtractReqResp<mysql::Packet>();
  if (!s.ok()) {
    LOG(ERROR) << s.msg();
    return {};
  }

  InitProtocolState<mysql::Packet>();

  auto& req_messages = req_data()->Messages<mysql::Packet>();
  auto& resp_messages = resp_data()->Messages<mysql::Packet>();

  auto state_ptr = protocol_state<mysql::State>();

  // ProcessMySQLPackets handles errors internally.
  std::vector<mysql::Record> result =
      mysql::ProcessMySQLPackets(&req_messages, &resp_messages, state_ptr);

  Cleanup<mysql::Packet>();

  return result;
}

void ConnectionTracker::Disable(std::string_view reason) {
  set_state(State::kDisabled, reason);

  send_data_.Reset();
  recv_data_.Reset();

  // TODO(oazizi): Propagate the disable back to BPF, so it doesn't even send the data.
}

void ConnectionTracker::set_state(State state, std::string_view reason) {
  LOG_IF(INFO, state_ != state) << absl::Substitute(
      "Changing the state of connection=$0 dest=$1:$2, from $3 to $4, reason=$5",
      ToString(conn_id_), open_info_.remote_addr.addr_str, open_info_.remote_addr.port, state_,
      state, reason);
  // TODO(oazizi/yzhao): Consider storing the reason field.
  state_ = state;
}

bool ConnectionTracker::AllEventsReceived() const {
  return (close_info_.timestamp_ns != 0) && (num_send_events_ == close_info_.send_seq_num) &&
         (num_recv_events_ == close_info_.recv_seq_num);
}

void ConnectionTracker::SetConnID(struct conn_id_t conn_id) {
  DCHECK(conn_id_.upid.pid == 0 || conn_id_.upid.pid == conn_id.upid.pid) << absl::Substitute(
      "Mismatched conn info: tracker=$0 event=$1", ToString(conn_id_), ToString(conn_id));
  DCHECK(conn_id_.fd == 0 || conn_id_.fd == conn_id.fd) << absl::Substitute(
      "Mismatched conn info: tracker=$0 event=$1", ToString(conn_id_), ToString(conn_id));
  DCHECK(conn_id_.generation == 0 || conn_id_.generation == conn_id.generation) << absl::Substitute(
      "Mismatched conn info: tracker=$0 event=$1", ToString(conn_id_), ToString(conn_id));
  DCHECK(conn_id_.upid.start_time_ticks == 0 ||
         conn_id_.upid.start_time_ticks == conn_id.upid.start_time_ticks)
      << absl::Substitute("Mismatched conn info: tracker=$0 event=$1", ToString(conn_id_),
                          ToString(conn_id));

  conn_id_ = conn_id;
}

void ConnectionTracker::SetTrafficClass(struct traffic_class_t traffic_class) {
  // Don't allow changing active protocols or roles, unless it is from unknown to something else.

  if (traffic_class_.protocol == kProtocolUnknown) {
    traffic_class_.protocol = traffic_class.protocol;
  } else if (traffic_class.protocol != kProtocolUnknown) {
    DCHECK_EQ(traffic_class_.protocol, traffic_class.protocol)
        << "Not allowed to change the protocol of an active ConnectionTracker";
  }

  if (traffic_class_.role == kRoleUnknown) {
    traffic_class_.role = traffic_class.role;
  } else if (traffic_class.role != kRoleUnknown) {
    DCHECK_EQ(traffic_class_.role, traffic_class.role)
        << "Not allowed to change the role of an active ConnectionTracker";
  }
}

void ConnectionTracker::UpdateTimestamps(uint64_t bpf_timestamp) {
  last_bpf_timestamp_ns_ = std::max(last_bpf_timestamp_ns_, bpf_timestamp);

  last_update_timestamp_ = std::chrono::steady_clock::now();
}

void ConnectionTracker::CheckTracker() {
  LOG_IF(WARNING, death_countdown_ >= 0 && death_countdown_ < kDeathCountdownIters - 1)
      << absl::Substitute(
             "Did not expect new event more than 1 sampling iteration after Close. Connection=$0.",
             ToString(conn_id_));

  LOG_IF(ERROR, conn_id_.fd == 0 && !disabled()) << absl::Substitute(
      "FD==0, which usually means the FD was not captured correctly. Connection=$0.",
      ToString(conn_id_));
}

DataStream* ConnectionTracker::req_data() {
  switch (traffic_class_.role) {
    case kRoleClient:
      return &send_data_;
    case kRoleServer:
      return &recv_data_;
    default:
      return nullptr;
  }
}

DataStream* ConnectionTracker::resp_data() {
  switch (traffic_class_.role) {
    case kRoleClient:
      return &recv_data_;
    case kRoleServer:
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
  if (open_info_.remote_addr.addr_str == "-" && FLAGS_infer_conn_info && connections != nullptr) {
    InferConnInfo(proc_parser, connections);
  }
  if (state() == State::kCollecting && role() == EndpointRole::kRoleClient) {
    set_state(State::kTransferring, "Always transfer data from client side.");
  }
  // TODO(yzhao): If InferConnInfo() failed, we'd disable this tracker if it's on server side.
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
  std::filesystem::path fd_file =
      absl::StrCat(sysconfig.proc_path(), absl::Substitute("/$0/fd/$1", pid(), fd()));

  if (!std::filesystem::exists(fd_file)) {
    // Connection seems to be dead. Mark for immediate death.
    MarkForDeath(0);
  } else {
    // Connection may still be alive (though inactive), so flush the data buffers.
    // It is unlikely any new data is a continuation of existing data in in any meaningful way.
    send_data_.Reset();
    recv_data_.Reset();
  }
}

namespace {

Status ParseRemoteIPAddress(const system::SocketInfo& socket_info, IPAddress* addr) {
  switch (socket_info.family) {
    case AF_INET:
      // This goes first so that the result argument won't be mutated when failed.
      PL_RETURN_IF_ERROR(ParseIPv4Addr(socket_info.remote_addr, &addr->addr_str));
      addr->addr = reinterpret_cast<const struct in_addr&>(socket_info.remote_addr);
      addr->port = socket_info.remote_port;
      return Status::OK();
    case AF_INET6:
      PL_RETURN_IF_ERROR(ParseIPv6Addr(socket_info.remote_addr, &addr->addr_str));
      addr->addr = socket_info.remote_addr;
      addr->port = socket_info.remote_port;
      return Status::OK();
  }
  return error::InvalidArgument("Unexpected family $0", socket_info.family);
}

}  // namespace

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
        conn_id_.upid.pid, conn_id_.fd, conn_id_.generation);
    return;
  }

  VLOG(2) << absl::Substitute("Attempting connection inference pid=$0 fd=$1 gen=$2",
                              conn_id_.upid.pid, conn_id_.fd, conn_id_.generation);

  if (conn_resolver_ == nullptr) {
    conn_resolver_ = std::make_unique<SocketResolver>(proc_parser, conn_id_.upid.pid, conn_id_.fd);
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
    case AF_INET6:
      s = ParseRemoteIPAddress(socket_info, &open_info_.remote_addr);
      if (!s.ok()) {
        LOG(ERROR) << absl::Substitute("IP parsing failed [msg=$0]", s.msg());
        return;
      }
      break;
    case AF_UNIX:
      // TODO(oazizi): This actually records the source inode number. Should actually populate the
      // remote peer.
      open_info_.remote_addr.addr_str = "unix_socket";
      open_info_.remote_addr.port = inode_num;
      unix_domain_socket = true;
      break;
    default:
      LOG(DFATAL) << absl::Substitute("Unexpected family $0", socket_info.family);
  }

  LOG(INFO) << absl::Substitute("Inferred connection pid=$0 fd=$1 gen=$2 dest=$3:$4", pid(), fd(),
                                generation(), open_info_.remote_addr.addr_str,
                                open_info_.remote_addr.port);

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
  info += absl::Substitute("$0protocol=$1\n", prefix, magic_enum::enum_name(protocol()));
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
template std::vector<http2::NewRecord> ConnectionTracker::ProcessMessages();

template std::string ConnectionTracker::DebugString<http::Record>(std::string_view prefix) const;
template std::string ConnectionTracker::DebugString<http2::Record>(std::string_view prefix) const;
template std::string ConnectionTracker::DebugString<http2::NewRecord>(
    std::string_view prefix) const;
template std::string ConnectionTracker::DebugString<mysql::Record>(std::string_view prefix) const;

}  // namespace stirling
}  // namespace pl
