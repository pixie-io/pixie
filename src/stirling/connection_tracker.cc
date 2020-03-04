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
#include "src/stirling/common/go_grpc_types.h"
#include "src/stirling/common/protocol_traits.h"
#include "src/stirling/cql/cql_stitcher.h"
#include "src/stirling/cql/types.h"
#include "src/stirling/http/http_stitcher.h"
#include "src/stirling/http/types.h"
#include "src/stirling/http2u/stitcher.h"
#include "src/stirling/http2u/types.h"
#include "src/stirling/mysql/mysql_stitcher.h"
#include "src/stirling/mysql/types.h"

DEFINE_bool(enable_unix_domain_sockets, false, "Whether Unix domain sockets are traced or not.");
DEFINE_uint32(stirling_http2_stream_id_gap_threshold, 100,
              "If a stream ID jumps by this many spots or more, an error is assumed and the entire "
              "connection info is cleared.");

// Parse failure rate threshold, after which a connection tracker will be disabled.
constexpr double kParseFailureRateThreshold = 0.4;

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

  Status s = PopulateSockAddr(reinterpret_cast<const struct sockaddr*>(&conn_event.addr),
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
  if (state() == State::kDisabled) {
    return;
  }

  CheckTracker();
  UpdateTimestamps(event->attr.return_timestamp_ns);

  switch (event->attr.direction) {
    case TrafficDirection::kEgress: {
      send_data_.AddData(std::move(event));
      ++num_send_events_;
    } break;
    case TrafficDirection::kIngress: {
      recv_data_.AddData(std::move(event));
      ++num_recv_events_;
    } break;
  }
}

http2u::HalfStream* ConnectionTracker::HalfStreamPtr(uint32_t stream_id, bool write_event) {
  // Check for both client-initiated (odd stream_ids) and server-initiated (even stream_ids)
  // streams.
  std::deque<http2u::Stream>* streams_deque_ptr;
  uint32_t* oldest_active_stream_id_ptr;

  bool client_stream = (stream_id % 2 == 1);

  if (client_stream) {
    streams_deque_ptr = &(client_streams_.http2_streams());
    oldest_active_stream_id_ptr = &oldest_active_client_stream_id_;
  } else {
    streams_deque_ptr = &(server_streams_.http2_streams());
    oldest_active_stream_id_ptr = &oldest_active_server_stream_id_;
  }

  // Update the head index.
  if (streams_deque_ptr->empty()) {
    *oldest_active_stream_id_ptr = stream_id;
  }

  size_t index;
  if (stream_id < *oldest_active_stream_id_ptr) {
    LOG_EVERY_N(WARNING, 100) << absl::Substitute(
        "Stream ID ($0) is lower than the current head stream ID ($1). "
        "Not expected, but will handle it anyways. If not a data race, "
        "this could be indicative of a bug that could result in a memory leak.",
        stream_id, *oldest_active_stream_id_ptr);
    // Need to grow the deque at the front.
    size_t new_size = streams_deque_ptr->size() +
                      ((*oldest_active_stream_id_ptr - stream_id) / kHTTP2StreamIDIncrement);
    // Reset everything for now.
    if (new_size - streams_deque_ptr->size() > FLAGS_stirling_http2_stream_id_gap_threshold) {
      LOG_EVERY_N(ERROR, 100) << absl::Substitute(
          "Encountered a stream ID $0 that is too far from the last known stream ID $1. Resetting "
          "all streams on this connection.",
          stream_id, *oldest_active_stream_id_ptr + streams_deque_ptr->size() * 2);
      streams_deque_ptr->clear();
      streams_deque_ptr->resize(1);
      index = 0;
      *oldest_active_stream_id_ptr = stream_id;
    } else {
      streams_deque_ptr->insert(streams_deque_ptr->begin(), new_size - streams_deque_ptr->size(),
                                http2u::Stream());
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
      LOG_EVERY_N(ERROR, 100) << absl::Substitute(
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

  http2u::HalfStream* half_stream_ptr = write_event ? &stream.send : &stream.recv;
  return half_stream_ptr;
}

EndpointRole InferHTTP2Role(bool write_event, const std::unique_ptr<HTTP2HeaderEvent>& hdr) {
  // Look for standard headers to infer role.
  // Could look at others (:scheme, :path, :authority), but this seems sufficient.

  if (hdr->name == ":method") {
    return (write_event) ? kRoleClient : kRoleServer;
  }
  if (hdr->name == ":status") {
    return (write_event) ? kRoleServer : kRoleClient;
  }

  return kRoleUnknown;
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
  if (state() == State::kDisabled) {
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

  // TODO(oazizi): Once we have more confidence that the ECHECK below doesn't fire, restructure this
  // code so it only calls InferHTTP2Role when the current role is Unknown.
  EndpointRole role = InferHTTP2Role(write_event, hdr);
  if (traffic_class_.role == kRoleUnknown) {
    traffic_class_.role = role;
  } else {
    ECHECK(role == kRoleUnknown || role == traffic_class_.role) << absl::Substitute(
        "The role of an active ConnectionTracker was changed: $0 role: $1 -> $2",
        ToString(conn_id_), magic_enum::enum_name(traffic_class_.role),
        magic_enum::enum_name(role));
  }

  http2u::HalfStream* half_stream_ptr = HalfStreamPtr(hdr->attr.stream_id, write_event);

  // End stream flag is on a dummy header, so just record the end_stream, but don't add the headers.
  if (hdr->attr.end_stream) {
    // Expecting a dummy (empty) header since this is how it is done in BPF.
    ECHECK(hdr->name.empty());
    ECHECK(hdr->value.empty());

    // Only expect one end_stream signal per stream direction.
    // Note: Duplicate calls to the writeHeaders (calls with same arguments) have been observed.
    // It happens rarely, and root cause is unknown. It does not appear to be a retransmission due
    // to dropped packets (as that is handled by the TCP layer).
    // For now, just print a warning. The only harm is duplicated headers in the tables.
    // Note that the duplicates are not necessarily restricted to headers which have the end_stream
    // flag set; the end_stream cases are just the easiest to detect.
    LOG_IF(WARNING, half_stream_ptr->end_stream)
        << absl::Substitute("Duplicate end_stream flag in header. stream_id: $0, conn_id: $1",
                            hdr->attr.stream_id, ToString(hdr->attr.conn_id));

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
  if (state() == State::kDisabled) {
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

  http2u::HalfStream* half_stream_ptr = HalfStreamPtr(data->attr.stream_id, write_event);

  // Note: Duplicate calls to the writeHeaders have been observed (though they are rare).
  // It is not yet known if duplicate data also occurs. This log will help us figure out if such
  // cases exist. Note that the duplicates are not related to the end_stream flag being set;
  // the end_stream cases are just the easiest to detect.
  LOG_IF(WARNING, half_stream_ptr->end_stream && data->attr.end_stream)
      << absl::Substitute("Duplicate end_stream flag in data. stream_id: $0, conn_id: $1",
                          data->attr.stream_id, ToString(data->attr.conn_id));

  half_stream_ptr->data += data->payload;
  half_stream_ptr->end_stream |= data->attr.end_stream;
  half_stream_ptr->UpdateTimestamp(data->attr.timestamp_ns);
}

template <typename TFrameType>
Status ConnectionTracker::ExtractReqResp() {
  DataStream* resp_data_ptr = resp_data();
  DCHECK(resp_data_ptr != nullptr);
  resp_data_ptr->template ProcessBytesToFrames<TFrameType>(MessageType::kResponse);

  DataStream* req_data_ptr = req_data();
  DCHECK(req_data_ptr != nullptr);
  req_data_ptr->template ProcessBytesToFrames<TFrameType>(MessageType::kRequest);

  return Status::OK();
}

template <typename TProtocolTraits>
std::vector<typename TProtocolTraits::record_type> ConnectionTracker::ProcessToRecords() {
  using TRecordType = typename TProtocolTraits::record_type;
  using TFrameType = typename TProtocolTraits::frame_type;
  using TStateType = typename TProtocolTraits::state_type;

  Status s = ExtractReqResp<TFrameType>();
  if (!s.ok()) {
    LOG(ERROR) << s.msg();
    return {};
  }

  InitProtocolState<TStateType>();

  auto& req_frames = req_data()->Frames<TFrameType>();
  auto& resp_frames = resp_data()->Frames<TFrameType>();
  auto state_ptr = protocol_state<TStateType>();

  std::vector<TRecordType> result = ProcessFrames(&req_frames, &resp_frames, state_ptr);

  Cleanup<TProtocolTraits>();

  return result;
}

template std::vector<typename http::ProtocolTraits::record_type>
ConnectionTracker::ProcessToRecords<http::ProtocolTraits>();
template std::vector<typename http2::ProtocolTraits::record_type>
ConnectionTracker::ProcessToRecords<http2::ProtocolTraits>();
template std::vector<typename mysql::ProtocolTraits::record_type>
ConnectionTracker::ProcessToRecords<mysql::ProtocolTraits>();
template std::vector<typename cass::ProtocolTraits::record_type>
ConnectionTracker::ProcessToRecords<cass::ProtocolTraits>();

template <>
std::vector<http2u::Record> ConnectionTracker::ProcessToRecords<http2u::ProtocolTraits>() {
  // TODO(oazizi): ECHECK that raw events are empty.

  std::vector<http2u::Record> trace_records;

  http2u::ProcessHTTP2Streams(&client_streams_.http2_streams(), &oldest_active_client_stream_id_,
                              &trace_records);
  http2u::ProcessHTTP2Streams(&server_streams_.http2_streams(), &oldest_active_server_stream_id_,
                              &trace_records);

  Cleanup<http2u::ProtocolTraits>();

  return trace_records;
}

void ConnectionTracker::Reset() {
  send_data_.Reset();
  recv_data_.Reset();

  protocol_state_.reset();
}

void ConnectionTracker::Disable(std::string_view reason) {
  VLOG_IF(1, state_ != State::kDisabled)
      << absl::Substitute("Disabling connection=$0 dest=$1:$2, reason=$3", ToString(conn_id_),
                          open_info_.remote_addr.AddrStr(), open_info_.remote_addr.port, reason);

  // TODO(oazizi/yzhao): Consider storing the reason field.

  state_ = State::kDisabled;

  Reset();

  // TODO(oazizi): Propagate the disable back to BPF, so it doesn't even send the data.
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
    DCHECK_EQ(traffic_class_.protocol, traffic_class.protocol) << absl::Substitute(
        "Not allowed to change the protocol of an active ConnectionTracker: $0, old protocol: $1, "
        "new protocol $2",
        ToString(conn_id_), magic_enum::enum_name(traffic_class_.protocol),
        magic_enum::enum_name(traffic_class.protocol));
  }

  if (traffic_class_.role == kRoleUnknown) {
    traffic_class_.role = traffic_class.role;
  } else if (traffic_class.role != kRoleUnknown) {
    DCHECK_EQ(traffic_class_.role, traffic_class.role) << absl::Substitute(
        "Not allowed to change the role of an active ConnectionTracker: $0, old role: $1, new "
        "role: $2",
        ToString(conn_id_), magic_enum::enum_name(traffic_class_.role),
        magic_enum::enum_name(traffic_class.role));
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

  LOG_IF(ERROR, conn_id_.fd == 0 && state() != State::kDisabled) << absl::Substitute(
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

void ConnectionTracker::UpdateState(const std::vector<CIDRBlock>& cluster_cidrs) {
  switch (role()) {
    case EndpointRole::kRoleServer:
      if (state() == State::kCollecting) {
        state_ = State::kTransferring;
      }
      break;
    case EndpointRole::kRoleClient: {
      // Workaround: Server-side MySQL tracing seems to be busted, likely because of inference code.
      // TODO(oazizi/PL-1498): Remove this once service-side MySQL tracing is fixed.
      if (protocol() == kProtocolMySQL) {
        state_ = State::kTransferring;
        break;
      }

      if (cluster_cidrs.empty()) {
        Disable("No client-side tracing: Internal CIDR not specified.");
        break;
      }

      if (conn_resolution_failed_) {
        Disable("No client-side tracing: Can't infer remote endpoint address.");
        break;
      }

      if (open_info_.remote_addr.family == SockAddrFamily::kUninitialized) {
        // Don't disable because we are still trying to resolve the remote endpoint.
        // If resolution fails, then conn_resolution_failed_ will get set, and
        // this tracker should become disabled.
        break;
      }

      if (open_info_.remote_addr.family == SockAddrFamily::kUnix) {
        Disable("No client-side tracing: Unix socket.");
        break;
      }

      bool disabled = false;
      for (const auto& cluster_cidr : cluster_cidrs) {
        if (CIDRContainsIPAddr(cluster_cidr, open_info_.remote_addr)) {
          Disable("No client-side tracing: Remote endpoint is inside the cluster.");
          disabled = true;
          break;
        }
      }
      if (disabled) {
        break;
      }

      // Remote endpoint appears to be outside the cluster, so trace it.
      state_ = State::kTransferring;
    } break;
    case EndpointRole::kRoleUnknown:
      LOG_IF(DFATAL, !send_data_.events().empty() || !recv_data_.events().empty())
          << "Role has not been inferred from BPF events yet. conn_id: " << ToString(conn_id_);
      break;
    case EndpointRole::kRoleAll:
      LOG(DFATAL) << "kRoleAll should not be used.";
  }
}

void ConnectionTracker::IterationPreTick(const std::vector<CIDRBlock>& cluster_cidrs,
                                         system::ProcParser* proc_parser,
                                         system::SocketInfoManager* socket_info_mgr) {
  // Might not always be true, but for now there's nothing IterationPreTick does that
  // should be applied to a disabled tracker. This is in contrast to IterationPostTick.
  if (state_ == State::kDisabled) {
    return;
  }

  // If remote_addr is missing, it means the connect/accept was not traced.
  // Attempt to infer the connection information, to populate remote_addr.
  if (open_info_.remote_addr.family == SockAddrFamily::kUninitialized &&
      socket_info_mgr != nullptr) {
    InferConnInfo(proc_parser, socket_info_mgr);
  }

  // Normally, we don't want to trace Unix domain sockets.
  if (open_info_.remote_addr.family == SockAddrFamily::kUnix && !FLAGS_enable_unix_domain_sockets) {
    Disable("Unix domain socket");
  }

  UpdateState(cluster_cidrs);
}

void ConnectionTracker::IterationPostTick() {
  if (death_countdown_ > 0) {
    death_countdown_--;
  }

  if (std::chrono::steady_clock::now() > last_update_timestamp_ + InactivityDuration()) {
    HandleInactivity();
  }

  if (state() != State::kDisabled && (send_data().IsEOS() || recv_data().IsEOS())) {
    Disable("End-of-stream");
  }

  VLOG(1) << absl::Substitute(
      "$0 protocol=$1 state=$2 send_invalid_frames=$3 send_valid_frames=$4 send_raw_data_gaps=$5 "
      "recv_invalid_frames=$6 recv_valid_frames=$7, recv_raw_data_gaps=$8\n",
      ToString(conn_id_), magic_enum::enum_name(protocol()), magic_enum::enum_name(state()),
      send_data().stat_invalid_frames(), send_data().stat_valid_frames(),
      send_data().stat_raw_data_gaps(), recv_data().stat_invalid_frames(),
      recv_data().stat_valid_frames(), recv_data().stat_raw_data_gaps());

  if ((send_data().ParseFailureRate() > kParseFailureRateThreshold) ||
      (recv_data().ParseFailureRate() > kParseFailureRateThreshold)) {
    Disable(absl::Substitute("Connection does not appear parseable as protocol $0",
                             magic_enum::enum_name(protocol())));
  }
}

void ConnectionTracker::HandleInactivity() {
  static const auto& sysconfig = system::Config::GetInstance();
  std::filesystem::path fd_file =
      sysconfig.proc_path() / std::to_string(pid()) / "fd" / std::to_string(fd());

  if (!fs::Exists(fd_file).ok()) {
    // Connection seems to be dead. Mark for immediate death.
    MarkForDeath(0);
  } else {
    // Connection may still be alive (though inactive), so flush the data buffers.
    // It is unlikely any new data is a continuation of existing data in in any meaningful way.
    Reset();
  }
}

namespace {
Status ParseSocketInfoRemoteAddr(const system::SocketInfo& socket_info, SockAddr* addr) {
  switch (socket_info.family) {
    case AF_INET:
      PopulateInetAddr(std::get<struct in_addr>(socket_info.remote_addr), socket_info.remote_port,
                       addr);
      break;
    case AF_INET6:
      PopulateInet6Addr(std::get<struct in6_addr>(socket_info.remote_addr), socket_info.remote_port,
                        addr);
      break;
    case AF_UNIX:
      PopulateUnixAddr(std::get<struct un_path_t>(socket_info.remote_addr).path,
                       socket_info.remote_port, addr);
      break;
    default:
      return error::Internal("Unknown socket_info family: $0", socket_info.family);
  }

  return Status::OK();
}
}  // namespace

void ConnectionTracker::InferConnInfo(system::ProcParser* proc_parser,
                                      system::SocketInfoManager* socket_info_mgr) {
  DCHECK(proc_parser != nullptr);
  DCHECK(socket_info_mgr != nullptr);

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

  std::optional<uint32_t> inode_num_or_nullopt =
      conn_resolver_->InferSocket(last_update_timestamp_);
  if (!inode_num_or_nullopt) {
    // This is not a conn resolution failure. We just need more time to figure out the right inode.
    return;
  }
  uint32_t inode_num = *inode_num_or_nullopt;

  // We found the inode number, now lets see if it maps to a known connection.
  StatusOr<const system::SocketInfo*> socket_info_status =
      socket_info_mgr->Lookup(pid(), inode_num);
  if (!socket_info_status.ok()) {
    conn_resolver_ = nullptr;
    conn_resolution_failed_ = true;
    LOG(WARNING) << absl::Substitute("Could not map inode to a connection. Message = $0",
                                     socket_info_status.msg());
    return;
  }
  const system::SocketInfo* socket_info = socket_info_status.ValueOrDie();

  // Success! Now copy the inferred socket information into the ConnectionTracker.

  Status s = ParseSocketInfoRemoteAddr(*socket_info, &open_info_.remote_addr);
  if (!s.ok()) {
    conn_resolver_ = nullptr;
    conn_resolution_failed_ = true;
    LOG(ERROR) << absl::Substitute("Remote address (type=$0) parsing failed. Message: $1",
                                   socket_info->family, s.msg());
    return;
  }

  LOG(INFO) << absl::Substitute("Inferred connection pid=$0 fd=$1 gen=$2 dest=$3:$4", pid(), fd(),
                                generation(), open_info_.remote_addr.AddrStr(),
                                open_info_.remote_addr.port);

  // No need for the resolver anymore, so free its memory.
  conn_resolver_.reset();
}

template <typename TProtocolTraits>
std::string DebugString(const ConnectionTracker& c, std::string_view prefix) {
  using TFrameType = typename TProtocolTraits::frame_type;

  std::string info;
  info += absl::Substitute("$0pid=$1 fd=$2 gen=$3\n", prefix, c.pid(), c.fd(), c.generation());
  info += absl::Substitute("state=$0\n", magic_enum::enum_name(c.state()));
  info += absl::Substitute("$0remote_addr=$1:$2\n", prefix, c.remote_endpoint().AddrStr(),
                           c.remote_endpoint().port);
  info += absl::Substitute("$0protocol=$1\n", prefix, magic_enum::enum_name(c.protocol()));
  info += absl::Substitute("$0recv queue\n", prefix);
  info += DebugString<TFrameType>(c.recv_data(), absl::StrCat(prefix, "  "));
  info += absl::Substitute("$0send queue\n", prefix);
  info += DebugString<TFrameType>(c.send_data(), absl::StrCat(prefix, "  "));
  return info;
}

template std::string DebugString<http::ProtocolTraits>(const ConnectionTracker& c,
                                                       std::string_view prefix);
template std::string DebugString<http2::ProtocolTraits>(const ConnectionTracker& c,
                                                        std::string_view prefix);
template std::string DebugString<http2u::ProtocolTraits>(const ConnectionTracker& c,
                                                         std::string_view prefix);
template std::string DebugString<mysql::ProtocolTraits>(const ConnectionTracker& c,
                                                        std::string_view prefix);
template std::string DebugString<cass::ProtocolTraits>(const ConnectionTracker& c,
                                                       std::string_view prefix);

}  // namespace stirling
}  // namespace pl
