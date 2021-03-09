#ifdef __linux__

#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"

#include <sys/types.h>
#include <unistd.h>

#include <filesystem>
#include <utility>

#include <absl/strings/match.h>
#include <google/protobuf/empty.pb.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/delimited_message_util.h>
#include <google/protobuf/util/json_util.h>
#include <magic_enum.hpp>

#include "src/common/base/base.h"
#include "src/common/base/utils.h"
#include "src/common/grpcutils/utils.h"
#include "src/common/json/json.h"
#include "src/common/system/socket_info.h"
#include "src/shared/metadata/metadata.h"
#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/obj_tools/dwarf_tools.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/go_grpc_types.hpp"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/symaddrs.h"
#include "src/stirling/source_connectors/socket_tracer/conn_stats.h"
#include "src/stirling/source_connectors/socket_tracer/proto/sock_event.pb.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http/utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/grpc.h"
#include "src/stirling/source_connectors/socket_tracer/uprobe_symaddrs.h"
#include "src/stirling/utils/linux_headers.h"
#include "src/stirling/utils/proc_path_tools.h"

DEFINE_int32(test_only_socket_trace_target_pid, kTraceAllTGIDs, "The process to trace.");
// TODO(yzhao): If we ever need to write all events from different perf buffers, then we need either
// write to different files for individual perf buffers, or create a protobuf message with an oneof
// field to include all supported message types.
DEFINE_string(perf_buffer_events_output_path, "",
              "If not empty, specifies the path & format to a file to which the socket tracer "
              "writes data events. If the filename ends with '.bin', the events are serialized in "
              "binary format; otherwise, text format.");

// PROTOCOL_LIST: Requires update on new protocols.
DEFINE_bool(stirling_enable_http_tracing, true,
            "If true, stirling will trace and process HTTP messages");
DEFINE_bool(stirling_enable_http2_tracing, true,
            "If true, stirling will trace and process gRPC RPCs.");
DEFINE_bool(stirling_enable_mysql_tracing, true,
            "If true, stirling will trace and process MySQL messages.");
DEFINE_bool(stirling_enable_pgsql_tracing, true,
            "If true, stirling will trace and process PostgreSQL messages.");
DEFINE_bool(stirling_enable_cass_tracing, true,
            "If true, stirling will trace and process Cassandra messages.");
DEFINE_bool(stirling_enable_dns_tracing, true,
            "If true, stirling will trace and process DNS messages.");
DEFINE_bool(stirling_enable_redis_tracing, true,
            "If true, stirling will trace and process Redis messages.");

DEFINE_bool(stirling_disable_self_tracing, true,
            "If true, stirling will not trace and process syscalls made by itself.");

BPF_SRC_STRVIEW(socket_trace_bcc_script, socket_trace);

namespace pl {
namespace stirling {

using ::pl::utils::ToJSONString;

SocketTraceConnector::SocketTraceConnector(std::string_view source_name)
    : SourceConnector(source_name, kTables), uprobe_mgr_(this) {
  proc_parser_ = std::make_unique<system::ProcParser>(system::Config::GetInstance());
  InitProtocolTransferSpecs();
}

void SocketTraceConnector::InitProtocolTransferSpecs() {
#define TRANSER_STREAM_PROTOCOL(protocol_name) \
  &SocketTraceConnector::TransferStream<protocols::protocol_name::ProtocolTraits>

  // PROTOCOL_LIST: Requires update on new protocols.

  // We popluate transfer_specs_by_protocol so that we guarantee the protocol_transfer_specs_
  // is stuffed in the *correct* order.
  // Also, this will fail fast (when we stuff the vector) if we forget a protocol.
  absl::flat_hash_map<TrafficProtocol, TransferSpec> transfer_specs_by_protocol = {
      {kProtocolHTTP, TransferSpec{FLAGS_stirling_enable_http_tracing,
                                   kHTTPTableNum,
                                   {kRoleClient, kRoleServer},
                                   TRANSER_STREAM_PROTOCOL(http)}},
      {kProtocolHTTP2, TransferSpec{FLAGS_stirling_enable_http2_tracing,
                                    kHTTPTableNum,
                                    {kRoleClient, kRoleServer},
                                    TRANSER_STREAM_PROTOCOL(http2)}},
      {kProtocolCQL, TransferSpec{FLAGS_stirling_enable_cass_tracing,
                                  kCQLTableNum,
                                  {kRoleClient, kRoleServer},
                                  TRANSER_STREAM_PROTOCOL(cass)}},
      {kProtocolMySQL, TransferSpec{FLAGS_stirling_enable_mysql_tracing,
                                    kMySQLTableNum,
                                    {kRoleClient, kRoleServer},
                                    TRANSER_STREAM_PROTOCOL(mysql)}},
      {kProtocolPGSQL, TransferSpec{FLAGS_stirling_enable_pgsql_tracing,
                                    kPGSQLTableNum,
                                    {kRoleClient, kRoleServer},
                                    TRANSER_STREAM_PROTOCOL(pgsql)}},
      {kProtocolDNS, TransferSpec{FLAGS_stirling_enable_dns_tracing,
                                  kDNSTableNum,
                                  {kRoleClient, kRoleServer},
                                  TRANSER_STREAM_PROTOCOL(dns)}},
      {kProtocolRedis, TransferSpec{FLAGS_stirling_enable_redis_tracing,
                                    kRedisTableNum,
                                    // Cannot infer endpoint role from Redis messages, so have to
                                    // allow such traffic transferred to user-space; and rely on
                                    // SocketInfo to infer the role.
                                    {kRoleUnknown, kRoleClient, kRoleServer},
                                    TRANSER_STREAM_PROTOCOL(redis)}},
      // Unknown protocols attached to HTTP table so that they run their cleanup functions,
      // but the use of nullptr transfer_fn means it won't actually transfer data to the HTTP table.
      {kProtocolUnknown, TransferSpec{false /*enabled*/,
                                      kHTTPTableNum,
                                      {kRoleUnknown, kRoleClient, kRoleServer},
                                      nullptr /*transfer_fn*/}}};
#undef TRANSER_STREAM_PROTOCOL

  for (uint64_t i = 0; i < kNumProtocols; ++i) {
    // First, we double check that we have a transfer spec for the protocol in question.
    // Next, we stuff the vector of transfer specs,
    // by indexing into the transfer_specs_by_protocol map.
    DCHECK(transfer_specs_by_protocol.contains(TrafficProtocol(i))) << absl::Substitute(
        "Protocol $0 is not mapped in transfer_specs_by_protocol.", TrafficProtocol(i));
    protocol_transfer_specs_.push_back(transfer_specs_by_protocol[TrafficProtocol(i)]);
  }
}

Status SocketTraceConnector::InitImpl() {
  constexpr uint64_t kNanosPerSecond = 1000 * 1000 * 1000;
  if (kNanosPerSecond % sysconfig_.KernelTicksPerSecond() != 0) {
    return error::Internal(
        "SC_CLK_TCK aka USER_HZ must be 100, otherwise our BPF code may not generate proper "
        "timestamps in a way that matches how /proc/stat does it");
  }

  PL_RETURN_IF_ERROR(InitBPFProgram(socket_trace_bcc_script));
  PL_RETURN_IF_ERROR(AttachKProbes(kProbeSpecs));
  LOG(INFO) << absl::Substitute("Number of kprobes deployed = $0", kProbeSpecs.size());
  LOG(INFO) << "Probes successfully deployed.";

  PL_RETURN_IF_ERROR(OpenPerfBuffers(kPerfBufferSpecs, this));
  LOG(INFO) << absl::Substitute("Number of perf buffers opened = $0", kPerfBufferSpecs.size());

  // Set trace role to BPF probes.
  for (const auto& p : TrafficProtocolEnumValues()) {
    if (protocol_transfer_specs_[p].enabled) {
      uint64_t role_mask = 0;
      for (auto role : protocol_transfer_specs_[p].trace_roles) {
        role_mask |= role;
      }
      PL_RETURN_IF_ERROR(UpdateBPFProtocolTraceRole(p, role_mask));
    }
  }

  PL_RETURN_IF_ERROR(TestOnlySetTargetPID(FLAGS_test_only_socket_trace_target_pid));
  if (FLAGS_stirling_disable_self_tracing) {
    PL_RETURN_IF_ERROR(DisableSelfTracing());
  }
  if (!FLAGS_perf_buffer_events_output_path.empty()) {
    SetupOutput(FLAGS_perf_buffer_events_output_path);
  }

  StatusOr<std::unique_ptr<system::SocketInfoManager>> s =
      system::SocketInfoManager::Create(system::Config::GetInstance().proc_path(),
                                        system::kTCPEstablishedState | system::kTCPListeningState);
  if (!s.ok()) {
    LOG(WARNING) << absl::Substitute("Failed to set up socket prober manager. Message: $0",
                                     s.msg());
  } else {
    socket_info_mgr_ = s.ConsumeValueOrDie();
  }

  conn_info_map_mgr_ = std::make_shared<ConnInfoMapManager>(this);
  ConnectionTracker::SetConnInfoMapManager(conn_info_map_mgr_);

  uprobe_mgr_.Init(protocol_transfer_specs_[kProtocolHTTP2].enabled,
                   FLAGS_stirling_disable_self_tracing);

  return Status::OK();
}

void SocketTraceConnector::InitContextImpl(ConnectorContext* ctx) {
  std::thread thread = RunDeployUProbesThread(ctx->GetUPIDs());

  // On the first context, we want to make sure all uprobes deploy before returning.
  if (thread.joinable()) {
    thread.join();
  }
}

Status SocketTraceConnector::StopImpl() {
  if (perf_buffer_events_output_stream_ != nullptr) {
    perf_buffer_events_output_stream_->close();
  }

  // Wait for all threads to finish.
  while (uprobe_mgr_.ThreadsRunning()) {
  }

  // Must call Close() after attach_uprobes_thread_ has joined,
  // otherwise the two threads will cause concurrent accesses to BCC,
  // that will cause races and undefined behavior.
  Close();
  return Status::OK();
}

std::thread SocketTraceConnector::RunDeployUProbesThread(
    const absl::flat_hash_set<md::UPID>& pids) {
  // The check that state is not uninitialized is required for socket_trace_connector_test,
  // which would otherwise try to deploy uprobes (for which it does not have permissions).
  // Also, we check that there is no other previous thread still running.
  // If an old thread is still running, then we don't deploy this one,
  // we can let the next one cover the deployment of new uprobes.
  // TODO(oazizi): Consider changing the model to a independently running thread that wakes up
  //               periodically. If doing so, be careful of impact on tests, since the uprobe
  //               deployment will become asynchronous to TransferData(), and this may
  //               lead to non-determinism.
  if (state() != State::kUninitialized && !uprobe_mgr_.ThreadsRunning()) {
    return uprobe_mgr_.RunDeployUProbesThread(pids);
  }
  return {};
}

void SocketTraceConnector::UpdateCommonState(ConnectorContext* ctx) {
  // Since events may be pushed into the perf buffer while reading it,
  // we establish a cutoff time before draining the perf buffer.
  perf_buffer_drain_time_ = AdjustedSteadyClockNowNS();

  // This drains all perf buffers, and causes Handle() callback functions to get called.
  // Note that it drains *all* perf buffers, not just those that are required for this table,
  // so raw data will be pushed to connection trackers more aggressively.
  // No data is lost, but this is a side-effect of sorts that affects timing of transfers.
  // It may be worth noting during debug.
  PollPerfBuffers();

  // Set-up current state for connection inference purposes.
  if (socket_info_mgr_ != nullptr) {
    socket_info_mgr_->Flush();
  }

  // Deploy uprobes on newly discovered PIDs.
  std::thread thread = RunDeployUProbesThread(ctx->GetUPIDs());
  // Let it run in the background.
  if (thread.joinable()) {
    thread.detach();
  }

  // Can call this less frequently if it becomes a performance issue.
  // Trade-off is just how quickly we release memory and BPF map entries.
  conn_trackers_.CleanupTrackers();
}

void SocketTraceConnector::CachedUpdateCommonState(ConnectorContext* ctx, uint32_t table_num) {
  // Check if UpdateCommonState() is required.
  // As a performance optimization, we skip this update if a TransferData on a previous table
  // has already made this call.
  // Note that a second call to TransferData() for any given table will trigger UpdateCommonState(),
  // so, in effect, UpdateCommonState() runs as frequently as the most frequently sampled table.
  if (table_access_history_.test(table_num) == 0) {
    UpdateCommonState(ctx);

    // After calling UpdateCommonState(), set bits for all tables to 1,
    // representing the fact that they can piggy-back on the UpdateCommonState() just called.
    table_access_history_.set();
  }

  // Reset the bit for the current table, so that a future call to TransferData() on this table
  // will trigger UpdateCommonState().
  table_access_history_.reset(table_num);
}

void SocketTraceConnector::TransferDataImpl(ConnectorContext* ctx, uint32_t table_num,
                                            DataTable* data_table) {
  DCHECK_LT(table_num, kTables.size())
      << absl::Substitute("Trying to access unexpected table: table_num=$0", table_num);
  DCHECK(data_table != nullptr);

  CachedUpdateCommonState(ctx, table_num);

  if (table_num == kConnStatsTableNum) {
    // Connection stats table does not follow the convention of tables for data streams.
    // So we handle it separately.
    TransferConnectionStats(ctx, data_table);
  } else {
    data_table->SetConsumeRecordsCutoffTime(perf_buffer_drain_time_);

    TransferStreams(ctx, table_num, data_table);
  }
}

template <typename TValueType>
Status UpdatePerCPUArrayValue(int idx, TValueType val, ebpf::BPFPercpuArrayTable<TValueType>* arr) {
  std::vector<TValueType> values(bpf_tools::BCCWrapper::kCPUCount, val);
  auto update_res = arr->update_value(idx, values);
  if (!update_res.ok()) {
    return error::Internal(absl::Substitute("Failed to set value on index: $0, error message: $1",
                                            idx, update_res.msg()));
  }
  return Status::OK();
}

Status SocketTraceConnector::UpdateBPFProtocolTraceRole(TrafficProtocol protocol,
                                                        uint64_t role_mask) {
  auto control_map_handle = GetPerCPUArrayTable<uint64_t>(kControlMapName);
  return UpdatePerCPUArrayValue(static_cast<int>(protocol), role_mask, &control_map_handle);
}

Status SocketTraceConnector::TestOnlySetTargetPID(int64_t pid) {
  auto control_map_handle = GetPerCPUArrayTable<int64_t>(kControlValuesArrayName);
  return UpdatePerCPUArrayValue(kTargetTGIDIndex, pid, &control_map_handle);
}

Status SocketTraceConnector::DisableSelfTracing() {
  auto control_map_handle = GetPerCPUArrayTable<int64_t>(kControlValuesArrayName);
  int64_t self_pid = getpid();
  return UpdatePerCPUArrayValue(kStirlingTGIDIndex, self_pid, &control_map_handle);
}

//-----------------------------------------------------------------------------
// Perf Buffer Polling and Callback functions.
//-----------------------------------------------------------------------------

void SocketTraceConnector::HandleDataEvent(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  auto data_event_ptr = std::make_unique<SocketDataEvent>(data);
  connector->AcceptDataEvent(std::move(data_event_ptr));
}

namespace {

std::string ProbeLossMessage(std::string_view perf_buffer_name, uint64_t lost) {
  return absl::Substitute("$0 lost $1 samples.", perf_buffer_name, lost);
}

}  // namespace

void SocketTraceConnector::HandleDataEventLoss(void* /*cb_cookie*/, uint64_t lost) {
  VLOG(1) << ProbeLossMessage("socket_data_events", lost);
}

void SocketTraceConnector::HandleControlEvent(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  connector->AcceptControlEvent(*static_cast<const socket_control_event_t*>(data));
}

void SocketTraceConnector::HandleControlEventLoss(void* /*cb_cookie*/, uint64_t lost) {
  VLOG(1) << ProbeLossMessage("socket_control_events", lost);
}

void SocketTraceConnector::HandleMMapEvent(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  connector->uprobe_mgr_.NotifyMMapEvent(*static_cast<upid_t*>(data));
}

void SocketTraceConnector::HandleMMapEventLoss(void* /*cb_cookie*/, uint64_t lost) {
  VLOG(1) << ProbeLossMessage("mmap_events", lost);
}

void SocketTraceConnector::HandleHTTP2HeaderEvent(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";

  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);

  auto event = std::make_unique<HTTP2HeaderEvent>(data);

  VLOG(3) << absl::Substitute(
      "t=$0 pid=$1 type=$2 fd=$3 tsid=$4 stream_id=$5 end_stream=$6 name=$7 value=$8",
      event->attr.timestamp_ns, event->attr.conn_id.upid.pid,
      magic_enum::enum_name(event->attr.type), event->attr.conn_id.fd, event->attr.conn_id.tsid,
      event->attr.stream_id, event->attr.end_stream, event->name, event->value);
  connector->AcceptHTTP2Header(std::move(event));
}

void SocketTraceConnector::HandleHTTP2HeaderEventLoss(void* /*cb_cookie*/, uint64_t lost) {
  VLOG(1) << ProbeLossMessage("go_grpc_header_events", lost);
}

void SocketTraceConnector::HandleHTTP2Data(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";

  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  // Directly access data through a go_grpc_data_event_t pointer results in mis-aligned access.
  // go_grpc_data_event_t is 8-bytes aligned, data is 4-bytes.
  auto event = std::make_unique<HTTP2DataEvent>(data);

  VLOG(3) << absl::Substitute(
      "t=$0 pid=$1 type=$2 fd=$3 tsid=$4 stream_id=$5 end_stream=$6 data=$7",
      event->attr.timestamp_ns, event->attr.conn_id.upid.pid,
      magic_enum::enum_name(event->attr.type), event->attr.conn_id.fd, event->attr.conn_id.tsid,
      event->attr.stream_id, event->attr.end_stream, event->payload);
  connector->AcceptHTTP2Data(std::move(event));
}

void SocketTraceConnector::HandleHTTP2DataLoss(void* /*cb_cookie*/, uint64_t lost) {
  VLOG(1) << ProbeLossMessage("go_grpc_data_events", lost);
}

//-----------------------------------------------------------------------------
// Connection Tracker Events
//-----------------------------------------------------------------------------

void SocketTraceConnector::AcceptDataEvent(std::unique_ptr<SocketDataEvent> event) {
  event->attr.timestamp_ns += ClockRealTimeOffset();

  if (perf_buffer_events_output_stream_ != nullptr) {
    WriteDataEvent(*event);
  }

  ConnectionTracker& tracker = conn_trackers_.GetOrCreateConnTracker(event->attr.conn_id);
  tracker.set_conn_stats(&connection_stats_);

  tracker.AddDataEvent(std::move(event));
}

void SocketTraceConnector::AcceptControlEvent(socket_control_event_t event) {
  // timestamp_ns is a common field of open and close fields.
  event.open.timestamp_ns += ClockRealTimeOffset();

  // conn_id is a common field of open & close.
  ConnectionTracker& tracker = conn_trackers_.GetOrCreateConnTracker(event.open.conn_id);
  tracker.set_conn_stats(&connection_stats_);

  tracker.AddControlEvent(event);
}

void SocketTraceConnector::AcceptHTTP2Header(std::unique_ptr<HTTP2HeaderEvent> event) {
  event->attr.timestamp_ns += ClockRealTimeOffset();

  ConnectionTracker& tracker = conn_trackers_.GetOrCreateConnTracker(event->attr.conn_id);
  tracker.set_conn_stats(&connection_stats_);

  tracker.AddHTTP2Header(std::move(event));
}

void SocketTraceConnector::AcceptHTTP2Data(std::unique_ptr<HTTP2DataEvent> event) {
  event->attr.timestamp_ns += ClockRealTimeOffset();

  ConnectionTracker& tracker = conn_trackers_.GetOrCreateConnTracker(event->attr.conn_id);
  tracker.set_conn_stats(&connection_stats_);

  tracker.AddHTTP2Data(std::move(event));
}

//-----------------------------------------------------------------------------
// Append-Related Functions
//-----------------------------------------------------------------------------

namespace {

int64_t CalculateLatency(int64_t req_timestamp_ns, int64_t resp_timestamp_ns) {
  int64_t latency_ns = 0;
  if (req_timestamp_ns > 0 && resp_timestamp_ns > 0) {
    latency_ns = resp_timestamp_ns - req_timestamp_ns;
    LOG_IF(WARNING, latency_ns < 0)
        << absl::Substitute("Negative latency implies req resp mismatch [t_req=$0, t_resp=$1].",
                            req_timestamp_ns, resp_timestamp_ns);
  }
  return latency_ns;
}

}  // namespace

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker,
                                         protocols::http::Record record, DataTable* data_table) {
  protocols::http::Message& req_message = record.req;
  protocols::http::Message& resp_message = record.resp;

  // Currently decompresses gzip content, but could handle other transformations too.
  // Note that we do this after filtering to avoid burning CPU cycles unnecessarily.
  protocols::http::PreProcessMessage(&resp_message);

  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  HTTPContentType content_type = HTTPContentType::kUnknown;
  if (protocols::http::IsJSONContent(resp_message)) {
    content_type = HTTPContentType::kJSON;
  }

  DataTable::RecordBuilder<&kHTTPTable> r(data_table, resp_message.timestamp_ns);
  r.Append<r.ColIndex("time_")>(resp_message.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  // Note that there is a string copy here,
  // But std::move is not allowed because we re-use conn object.
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.traffic_class().role);
  r.Append<r.ColIndex("http_major_version")>(1);
  r.Append<r.ColIndex("http_minor_version")>(resp_message.minor_version);
  r.Append<r.ColIndex("http_content_type")>(static_cast<uint64_t>(content_type));
  r.Append<r.ColIndex("http_req_headers"), kMaxHTTPHeadersBytes>(ToJSONString(req_message.headers));
  r.Append<r.ColIndex("http_req_method")>(std::move(req_message.req_method));
  r.Append<r.ColIndex("http_req_path")>(std::move(req_message.req_path));
  r.Append<r.ColIndex("http_req_body_size")>(req_message.body.size());
  r.Append<r.ColIndex("http_req_body"), kMaxBodyBytes>(std::move(req_message.body));
  r.Append<r.ColIndex("http_resp_headers"), kMaxHTTPHeadersBytes>(
      ToJSONString(resp_message.headers));
  r.Append<r.ColIndex("http_resp_status")>(resp_message.resp_status);
  r.Append<r.ColIndex("http_resp_message")>(std::move(resp_message.resp_message));
  r.Append<r.ColIndex("http_resp_body_size")>(resp_message.body.size());
  r.Append<r.ColIndex("http_resp_body"), kMaxBodyBytes>(std::move(resp_message.body));
  r.Append<r.ColIndex("http_resp_latency_ns")>(
      CalculateLatency(req_message.timestamp_ns, resp_message.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(ToString(conn_tracker.conn_id()));
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker,
                                         protocols::http2::Record record, DataTable* data_table) {
  using ::pl::grpc::MethodInputOutput;
  using ::pl::stirling::grpc::ParsePB;

  protocols::http2::HalfStream* req_stream;
  protocols::http2::HalfStream* resp_stream;

  // Depending on whether the traced entity was the requestor or responder,
  // we need to flip the interpretation of the half-streams.
  if (conn_tracker.traffic_class().role == kRoleClient) {
    req_stream = &record.send;
    resp_stream = &record.recv;
  } else {
    req_stream = &record.recv;
    resp_stream = &record.send;
  }

  // TODO(oazizi): Status should be in the trailers, not headers. But for now it is found in
  // headers. Fix when this changes.
  int64_t resp_status;
  ECHECK(absl::SimpleAtoi(resp_stream->headers.ValueByKey(":status", "-1"), &resp_status));

  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  std::string path = req_stream->headers.ValueByKey(protocols::http2::headers::kPath);

  if ((req_stream->HasGRPCContentType() || resp_stream->HasGRPCContentType())) {
    // TODO(yzhao): Support reflection to get message types instead of empty message.
    ::google::protobuf::Empty empty_message;
    req_stream->data = ParsePB(req_stream->data, &empty_message);
    resp_stream->data = ParsePB(resp_stream->data, &empty_message);
  }

  DataTable::RecordBuilder<&kHTTPTable> r(data_table, resp_stream->timestamp_ns);
  r.Append<r.ColIndex("time_")>(resp_stream->timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.traffic_class().role);
  r.Append<r.ColIndex("http_major_version")>(2);
  // HTTP2 does not define minor version.
  r.Append<r.ColIndex("http_minor_version")>(0);
  r.Append<r.ColIndex("http_req_headers"), kMaxHTTPHeadersBytes>(ToJSONString(req_stream->headers));
  r.Append<r.ColIndex("http_content_type")>(static_cast<uint64_t>(HTTPContentType::kGRPC));
  r.Append<r.ColIndex("http_resp_headers"), kMaxHTTPHeadersBytes>(
      ToJSONString(resp_stream->headers));
  r.Append<r.ColIndex("http_req_method")>(
      req_stream->headers.ValueByKey(protocols::http2::headers::kMethod));
  r.Append<r.ColIndex("http_req_path")>(req_stream->headers.ValueByKey(":path"));
  r.Append<r.ColIndex("http_resp_status")>(resp_status);
  // TODO(yzhao): Populate the following field from headers.
  r.Append<r.ColIndex("http_resp_message")>("OK");
  r.Append<r.ColIndex("http_req_body_size")>(req_stream->data.size());
  r.Append<r.ColIndex("http_req_body"), kMaxBodyBytes>(std::move(req_stream->data));
  r.Append<r.ColIndex("http_resp_body_size")>(resp_stream->data.size());
  r.Append<r.ColIndex("http_resp_body"), kMaxBodyBytes>(std::move(resp_stream->data));
  r.Append<r.ColIndex("http_resp_latency_ns")>(
      CalculateLatency(req_stream->timestamp_ns, resp_stream->timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(ToString(conn_tracker.conn_id()));
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker,
                                         protocols::mysql::Record entry, DataTable* data_table) {
  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  DataTable::RecordBuilder<&kMySQLTable> r(data_table, entry.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(entry.resp.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.traffic_class().role);
  r.Append<r.ColIndex("req_cmd")>(static_cast<uint64_t>(entry.req.cmd));
  r.Append<r.ColIndex("req_body"), kMaxBodyBytes>(std::move(entry.req.msg));
  r.Append<r.ColIndex("resp_status")>(static_cast<uint64_t>(entry.resp.status));
  r.Append<r.ColIndex("resp_body"), kMaxBodyBytes>(std::move(entry.resp.msg));
  r.Append<r.ColIndex("latency_ns")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(ToString(conn_tracker.conn_id()));
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker,
                                         protocols::cass::Record entry, DataTable* data_table) {
  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  DataTable::RecordBuilder<&kCQLTable> r(data_table, entry.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(entry.resp.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.traffic_class().role);
  r.Append<r.ColIndex("req_op")>(static_cast<uint64_t>(entry.req.op));
  r.Append<r.ColIndex("req_body"), kMaxBodyBytes>(std::move(entry.req.msg));
  r.Append<r.ColIndex("resp_op")>(static_cast<uint64_t>(entry.resp.op));
  r.Append<r.ColIndex("resp_body"), kMaxBodyBytes>(std::move(entry.resp.msg));
  r.Append<r.ColIndex("latency_ns")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(ToString(conn_tracker.conn_id()));
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker,
                                         protocols::dns::Record entry, DataTable* data_table) {
  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  DataTable::RecordBuilder<&kDNSTable> r(data_table, entry.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(entry.resp.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.traffic_class().role);
  r.Append<r.ColIndex("req_header")>(entry.req.header);
  r.Append<r.ColIndex("req_body")>(entry.req.query);
  r.Append<r.ColIndex("resp_header")>(entry.resp.header);
  r.Append<r.ColIndex("resp_body")>(entry.resp.msg);
  r.Append<r.ColIndex("latency_ns")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(ToString(conn_tracker.conn_id()));
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker,
                                         protocols::pgsql::Record entry, DataTable* data_table) {
  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  DataTable::RecordBuilder<&kPGSQLTable> r(data_table, entry.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(entry.resp.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.traffic_class().role);
  r.Append<r.ColIndex("req")>(std::move(entry.req.payload));
  r.Append<r.ColIndex("resp")>(std::move(entry.resp.payload));
  r.Append<r.ColIndex("latency_ns")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(ToString(conn_tracker.conn_id()));
#endif
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker,
                                         protocols::redis::Record entry, DataTable* data_table) {
  md::UPID upid(ctx->GetASID(), conn_tracker.conn_id().upid.pid,
                conn_tracker.conn_id().upid.start_time_ticks);

  DataTable::RecordBuilder<&kRedisTable> r(data_table, entry.resp.timestamp_ns);
  r.Append<r.ColIndex("time_")>(entry.resp.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(conn_tracker.remote_endpoint().AddrStr());
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_endpoint().port());
  r.Append<r.ColIndex("trace_role")>(conn_tracker.traffic_class().role);
  r.Append<r.ColIndex("cmd")>(std::string(entry.req.command));
  r.Append<r.ColIndex("cmd_args")>(std::string(entry.req.payload));
  r.Append<r.ColIndex("resp")>(std::string(entry.resp.payload));
  r.Append<r.ColIndex("latency_ns")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
#ifndef NDEBUG
  r.Append<r.ColIndex("px_info_")>(ToString(conn_tracker.conn_id()));
#endif
}

void SocketTraceConnector::SetupOutput(const std::filesystem::path& path) {
  DCHECK(!path.empty());

  std::filesystem::path abs_path = std::filesystem::absolute(path);
  perf_buffer_events_output_stream_ = std::make_unique<std::ofstream>(abs_path);
  std::string format = "text";
  constexpr char kBinSuffix[] = ".bin";
  if (absl::EndsWith(FLAGS_perf_buffer_events_output_path, kBinSuffix)) {
    perf_buffer_events_output_format_ = OutputFormat::kBin;
    format = "binary";
  }
  LOG(INFO) << absl::Substitute("Writing output to: $0 in $1 format.", abs_path.string(), format);
}

namespace {
void SocketDataEventToPB(const SocketDataEvent& event, sockeventpb::SocketDataEvent* pb) {
  pb->mutable_attr()->set_timestamp_ns(event.attr.timestamp_ns);
  pb->mutable_attr()->mutable_conn_id()->set_pid(event.attr.conn_id.upid.pid);
  pb->mutable_attr()->mutable_conn_id()->set_start_time_ns(
      event.attr.conn_id.upid.start_time_ticks);
  pb->mutable_attr()->mutable_conn_id()->set_fd(event.attr.conn_id.fd);
  pb->mutable_attr()->mutable_conn_id()->set_generation(event.attr.conn_id.tsid);
  pb->mutable_attr()->mutable_traffic_class()->set_protocol(event.attr.traffic_class.protocol);
  pb->mutable_attr()->mutable_traffic_class()->set_role(event.attr.traffic_class.role);
  pb->mutable_attr()->set_direction(event.attr.direction);
  pb->mutable_attr()->set_pos(event.attr.pos);
  pb->mutable_attr()->set_msg_size(event.attr.msg_size);
  pb->set_msg(event.msg);
}
}  // namespace

void SocketTraceConnector::WriteDataEvent(const SocketDataEvent& event) {
  using ::google::protobuf::TextFormat;
  using ::google::protobuf::util::SerializeDelimitedToOstream;

  DCHECK(perf_buffer_events_output_stream_ != nullptr);

  sockeventpb::SocketDataEvent pb;
  SocketDataEventToPB(event, &pb);
  std::string text;
  switch (perf_buffer_events_output_format_) {
    case OutputFormat::kTxt:
      // TextFormat::Print() can print to a stream. That complicates things a bit, and we opt not
      // to do that as this is for debugging.
      TextFormat::PrintToString(pb, &text);
      // TextFormat already output a \n, so no need to do it here.
      *perf_buffer_events_output_stream_ << text << std::flush;
      break;
    case OutputFormat::kBin:
      SerializeDelimitedToOstream(pb, perf_buffer_events_output_stream_.get());
      *perf_buffer_events_output_stream_ << std::flush;
      break;
  }
}

//-----------------------------------------------------------------------------
// TransferData Helpers
//-----------------------------------------------------------------------------

void SocketTraceConnector::TransferStreams(ConnectorContext* ctx, uint32_t table_num,
                                           DataTable* data_table) {
  std::vector<CIDRBlock> cluster_cidrs = ctx->GetClusterCIDRs();

  // Iterate through protocols to find protocols that belong to this table.
  // This is more efficient than going through all trackers, because
  // we can choose to only go through the relevant protocol tracker lists.
  for (size_t i = 0; i < protocol_transfer_specs_.size(); ++i) {
    const auto& transfer_spec = protocol_transfer_specs_[i];

    if (transfer_spec.table_num != table_num) {
      continue;
    }

    auto protocol = magic_enum::enum_cast<TrafficProtocol>(i);
    DCHECK(protocol.has_value());

    ConnTrackersManager::TrackersList conn_trackers_list =
        conn_trackers_.ConnTrackersForProtocol(protocol.value());

    for (auto iter = conn_trackers_list.begin(); iter != conn_trackers_list.end(); ++iter) {
      ConnectionTracker* tracker = *iter;

      VLOG(1) << absl::Substitute("Connection conn_id=$0 protocol=$1 state=$2\n",
                                  ToString(tracker->conn_id()),
                                  magic_enum::enum_name(tracker->traffic_class().protocol),
                                  magic_enum::enum_name(tracker->state()));

      tracker->IterationPreTick(cluster_cidrs, proc_parser_.get(), socket_info_mgr_.get());

      if (transfer_spec.transfer_fn && transfer_spec.enabled) {
        transfer_spec.transfer_fn(*this, ctx, tracker, data_table);
      }

      tracker->IterationPostTick();
    }
  }
}

template <typename TProtocolTraits>
void SocketTraceConnector::TransferStream(ConnectorContext* ctx, ConnectionTracker* tracker,
                                          DataTable* data_table) {
  VLOG(3) << absl::StrCat("Connection\n", DebugString<TProtocolTraits>(*tracker, ""));

  if (tracker->state() == ConnectionTracker::State::kTransferring) {
    // ProcessToRecords() parses raw events and produces messages in format that are expected by
    // table store. But those messages are not cached inside ConnectionTracker.
    //
    // TODO(yzhao): Consider caching produced messages if they are not transferred.
    auto result = tracker->ProcessToRecords<TProtocolTraits>();
    for (auto& msg : result) {
      AppendMessage(ctx, *tracker, std::move(msg), data_table);
    }
  }
}

void SocketTraceConnector::TransferConnectionStats(ConnectorContext* ctx, DataTable* data_table) {
  namespace idx = ::pl::stirling::conn_stats_idx;

  absl::flat_hash_set<md::UPID> upids = ctx->GetUPIDs();

  auto& agg_stats = connection_stats_.mutable_agg_stats();

  auto iter = agg_stats.begin();
  while (iter != agg_stats.end()) {
    const auto& key = iter->first;
    auto& stats = iter->second;

    if (stats.conn_open < stats.conn_close) {
      LOG_FIRST_N(WARNING, 10) << "Connection open should not be smaller than connection close.";
    }

    // Only export this record if there are actual changes.
    // TODO(yzhao): Exports these records after several iterations.
    if (!stats.prev_bytes_sent.has_value() || !stats.prev_bytes_recv.has_value() ||
        stats.bytes_sent != stats.prev_bytes_sent || stats.bytes_recv != stats.prev_bytes_recv) {
      uint64_t time = AdjustedSteadyClockNowNS();

      DataTable::RecordBuilder<&kConnStatsTable> r(data_table, time);

      r.Append<idx::kTime>(time);
      md::UPID upid(ctx->GetASID(), key.upid.tgid, key.upid.start_time_ticks);
      r.Append<idx::kUPID>(upid.value());
      r.Append<idx::kRemoteAddr>(key.remote_addr);
      r.Append<idx::kRemotePort>(key.remote_port);
      r.Append<idx::kAddrFamily>(static_cast<int>(stats.addr_family));
      r.Append<idx::kProtocol>(stats.traffic_class.protocol);
      r.Append<idx::kRole>(stats.traffic_class.role);
      r.Append<idx::kConnOpen>(stats.conn_open);
      r.Append<idx::kConnClose>(stats.conn_close);
      r.Append<idx::kConnActive>(stats.conn_open - stats.conn_close);
      // TODO(yzhao/oazizi): This is a bug, because the stats only reflect the bytes of BPF events
      //                     that made it through the perf buffer; lost events are not included.
      //                     A better approach is to have BPF events update the cumulative number of
      //                     bytes transferred,  and use that here directly. We already have a
      //                     ticket to convert seq numbers to bytes transferred, so we could kill
      //                     two birds with one stone.
      r.Append<idx::kBytesSent>(stats.bytes_sent);
      r.Append<idx::kBytesRecv>(stats.bytes_recv);
#ifndef NDEBUG
      r.Append<idx::kPxInfo>("");
#endif

      stats.prev_bytes_sent = stats.bytes_sent;
      stats.prev_bytes_recv = stats.bytes_recv;
    }

    // Remove data for exited upids. Do this after exporting data so that the last records are
    // exported.
    md::UPID current_upid(ctx->GetASID(), key.upid.tgid, key.upid.start_time_ticks);
    if (!upids.contains(current_upid)) {
      // NOTE: absl doesn't support iter = agg_stats.erase(iter), so must use this style.
      agg_stats.erase(iter++);
      continue;
    }

    // This is at the bottom, in order to avoid accidentally forgetting increment the iterator.
    ++iter;
  }
}

}  // namespace stirling
}  // namespace pl

#endif
