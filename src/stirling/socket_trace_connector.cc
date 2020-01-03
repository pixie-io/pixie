#ifdef __linux__

#include <sys/types.h>
#include <unistd.h>

#include <deque>
#include <experimental/filesystem>
#include <utility>

#include <absl/strings/match.h>
#include <google/protobuf/empty.pb.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>
#include <magic_enum.hpp>

#include "src/common/base/base.h"
#include "src/common/base/utils.h"
#include "src/common/grpcutils/utils.h"
#include "src/common/protobufs/recordio.h"
#include "src/shared/metadata/metadata.h"
#include "src/stirling/bcc_bpf_interface/socket_trace.h"
#include "src/stirling/common/event_parser.h"
#include "src/stirling/common/go_grpc.h"
#include "src/stirling/http2/grpc.h"
#include "src/stirling/http2/http2.h"
#include "src/stirling/mysql/mysql_parse.h"
#include "src/stirling/obj_tools/obj_tools.h"
#include "src/stirling/proto/sock_event.pb.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/utils/json.h"
#include "src/stirling/utils/linux_headers.h"

// TODO(yzhao): Consider simplify the semantic by filtering entirely on content type.
DEFINE_string(http_response_header_filters, "Content-Type:json",
              "Comma-separated strings to specify the substrings should be included for a header. "
              "The format looks like <header-1>:<substr-1>,...,<header-n>:<substr-n>. "
              "The substrings cannot include comma(s). The filters are conjunctive, "
              "therefore the headers can be duplicate. For example, "
              "'Content-Type:json,Content-Type:text' will select a HTTP response "
              "with a Content-Type header whose value contains 'json' *or* 'text'.");
DEFINE_bool(stirling_enable_parsing_protobufs, false,
            "If true, parses binary protobufs captured in gRPC messages. "
            "As of 2019-07, the parser can only handle protobufs defined in Hipster Shop.");
DEFINE_int32(test_only_socket_trace_target_pid, kTraceAllTGIDs, "The process to trace.");
// TODO(oazizi): Consolidate with dynamic sampling period though SetSamplingPeriod().
DEFINE_uint32(stirling_socket_trace_sampling_period_millis, 100,
              "The sampling period, in milliseconds, at which Stirling reads the BPF perf buffers "
              "for events.");
// TODO(yzhao): If we ever need to write all events from different perf buffers, then we need either
// write to different files for individual perf buffers, or create a protobuf message with an oneof
// field to include all supported message types.
DEFINE_string(perf_buffer_events_output_path, "",
              "If not empty, specifies the path & format to a file to which the socket tracer "
              "writes data events. If the filename ends with '.bin', the events are serialized in "
              "binary format; otherwise, text format.");

// TODO(oazizi/yzhao): Re-enable grpc and mysql tracing once stable.
DEFINE_bool(stirling_enable_http_tracing, true,
            "If true, stirling will trace and process HTTP messages");
DEFINE_bool(stirling_enable_grpc_kprobe_tracing, false,
            "If true, stirling will trace and process gRPC RPCs.");
DEFINE_bool(stirling_enable_grpc_uprobe_tracing, false,
            "If true, stirling will trace and process gRPC RPCs.");
DEFINE_bool(stirling_enable_mysql_tracing, true,
            "If true, stirling will trace and process MySQL messages.");
DEFINE_bool(stirling_disable_self_tracing, true,
            "If true, stirling will trace and process syscalls made by itself.");

// This flag is for survivability only, in case the host's located headers don't work.
DEFINE_bool(stirling_use_packaged_headers, false, "Force use of packaged kernel headers for BCC.");

namespace pl {
namespace stirling {

using ::google::protobuf::Empty;
using ::google::protobuf::Message;
using ::google::protobuf::TextFormat;
using ::pl::grpc::MethodInputOutput;
using ::pl::stirling::kHTTPTable;
using ::pl::stirling::kMySQLTable;
using ::pl::stirling::grpc::ParsePB;
using ::pl::stirling::grpc::PBTextFormat;
using ::pl::stirling::grpc::PBWireToText;
using ::pl::stirling::http2::HTTP2Message;
using ::pl::stirling::obj_tools::GetActiveBinaries;
using ::pl::stirling::obj_tools::GetSymAddrs;
using ::pl::stirling::utils::WriteMapAsJSON;

namespace fs = std::experimental::filesystem;

SocketTraceConnector::SocketTraceConnector(std::string_view source_name)
    : SourceConnector(source_name, kTables,
                      std::chrono::milliseconds(FLAGS_stirling_socket_trace_sampling_period_millis),
                      kDefaultPushPeriod),
      bpf_tools::BCCWrapper(kBCCScript) {
  // TODO(yzhao): Is there a better place/time to grab the flags?
  http_response_header_filter_ = http::ParseHTTPHeaderFilters(FLAGS_http_response_header_filters);
  proc_parser_ = std::make_unique<system::ProcParser>(system::Config::GetInstance());
  netlink_socket_prober_ = std::make_unique<system::NetlinkSocketProber>();

  protocol_transfer_specs_[kProtocolHTTP].enabled = FLAGS_stirling_enable_http_tracing;
  protocol_transfer_specs_[kProtocolHTTP2].enabled = FLAGS_stirling_enable_grpc_kprobe_tracing;
  protocol_transfer_specs_[kProtocolHTTP2Uprobe].enabled = true;
  protocol_transfer_specs_[kProtocolMySQL].enabled = FLAGS_stirling_enable_mysql_tracing;
}

Status SocketTraceConnector::InitImpl() {
  std::vector<utils::LinuxHeaderStrategy> linux_header_search_order =
      utils::kDefaultHeaderSearchOrder;
  if (FLAGS_stirling_use_packaged_headers) {
    linux_header_search_order = {utils::LinuxHeaderStrategy::kInstallPackagedHeaders};
  }
  PL_RETURN_IF_ERROR(utils::FindOrInstallLinuxHeaders(linux_header_search_order));

  constexpr uint64_t kNanosPerSecond = 1000 * 1000 * 1000;
  if (kNanosPerSecond % sysconfig_.KernelTicksPerSecond() != 0) {
    return error::Internal(
        "SC_CLK_TCK aka USER_HZ must be 100, otherwise our BPF code may not generate proper "
        "timestamps in a way that matches how /proc/stat does it");
  }

  if (FLAGS_stirling_enable_grpc_kprobe_tracing && FLAGS_stirling_enable_grpc_uprobe_tracing) {
    LOG(DFATAL) << "--stirling_enable_grpc_kprobe_tracing and "
                   "--stirling_enable_grpc_uprobe_tracing cannot be both true. "
                   "--stirling_enable_grpc_kprobe_tracing is forced to false.";
    FLAGS_stirling_enable_grpc_kprobe_tracing = false;
  }

  std::vector<std::string> cflags;
  if (FLAGS_stirling_bpf_enable_logging) {
    cflags.emplace_back("-DENABLE_BPF_LOGGING");
  }
  PL_RETURN_IF_ERROR(InitBPFCode(cflags));
  PL_RETURN_IF_ERROR(AttachKProbes(kProbeSpecs));

  if (FLAGS_stirling_enable_grpc_uprobe_tracing) {
    // TODO(yzhao): Factor line 133-149 to a helper function.
    std::map<std::string, std::vector<int>> binaries =
        GetActiveBinaries(system::Config::GetInstance().proc_path());
    ebpf::BPFHashTable<uint32_t, struct conn_symaddrs_t> symaddrs_map =
        bpf().get_hash_table<uint32_t, struct conn_symaddrs_t>("symaddrs_map");
    for (const auto& [pid, symaddrs] : GetSymAddrs(binaries)) {
      symaddrs_map.update_value(pid, symaddrs);
    }
    PL_ASSIGN_OR_RETURN(const std::vector<bpf_tools::UProbeSpec> specs,
                        ResolveUProbeTmpls(binaries, kUProbeTmpls));
    PL_RETURN_IF_ERROR(AttachUProbes(ToArrayView(specs)));
  }
  PL_RETURN_IF_ERROR(OpenPerfBuffers(kPerfBufferSpecs, this));
  LOG(INFO) << "Probes successfully deployed";
  // TODO(yzhao): Consider adding a flag to switch the role to trace, i.e., between kRoleRequestor &
  // kRoleResponder.
  if (protocol_transfer_specs_[kProtocolHTTP].enabled) {
    PL_RETURN_IF_ERROR(Configure(kProtocolHTTP, kRoleRequestor));
  }
  if (protocol_transfer_specs_[kProtocolHTTP2].enabled) {
    PL_RETURN_IF_ERROR(Configure(kProtocolHTTP2, kRoleRequestor));
  }
  if (protocol_transfer_specs_[kProtocolMySQL].enabled) {
    PL_RETURN_IF_ERROR(Configure(kProtocolMySQL, kRoleRequestor));
  }
  PL_RETURN_IF_ERROR(TestOnlySetTargetPID(FLAGS_test_only_socket_trace_target_pid));
  if (FLAGS_stirling_disable_self_tracing) {
    PL_RETURN_IF_ERROR(DisableSelfTracing());
  }
  if (!FLAGS_perf_buffer_events_output_path.empty()) {
    fs::path output_path(FLAGS_perf_buffer_events_output_path);
    fs::path abs_path = fs::absolute(output_path);
    perf_buffer_events_output_stream_ = std::make_unique<std::ofstream>(abs_path);
    std::string format = "text";
    constexpr char kBinSuffix[] = ".bin";
    if (absl::EndsWith(FLAGS_perf_buffer_events_output_path, kBinSuffix)) {
      perf_buffer_events_output_format_ = OutputFormat::kBin;
      format = "binary";
    }
    LOG(INFO) << absl::Substitute("Writing output to: $0 in $1 format.", abs_path.string(), format);
  }

  netlink_socket_prober_ = std::make_unique<system::NetlinkSocketProber>();

  return Status::OK();
}

Status SocketTraceConnector::StopImpl() {
  bpf_tools::BCCWrapper::Stop();
  if (perf_buffer_events_output_stream_ != nullptr) {
    perf_buffer_events_output_stream_->close();
  }
  return Status::OK();
}

void SocketTraceConnector::UpdateActiveConnections() {
  // Grab a list of active connections, in case we need to infer the endpoints of any connections
  // with missing endpoints.
  // TODO(oazizi): Optimization is to skip this if we don't have any connection trackers with
  // unknown remote endpoints.
  socket_connections_ = std::make_unique<std::map<int, system::SocketInfo>>();

  Status s;

  s = netlink_socket_prober_->InetConnections(socket_connections_.get());
  LOG_IF(ERROR, !s.ok()) << absl::Substitute("Failed to probe InetConnections [msg=$0]", s.msg());

  s = netlink_socket_prober_->UnixConnections(socket_connections_.get());
  LOG_IF(ERROR, !s.ok()) << absl::Substitute("Failed to probe UnixConnections [msg=$0]", s.msg());
}

void SocketTraceConnector::TransferDataImpl(ConnectorContext* ctx, uint32_t table_num,
                                            DataTable* data_table) {
  DCHECK_LT(table_num, kTables.size())
      << absl::Substitute("Trying to access unexpected table: table_num=$0", table_num);
  DCHECK(data_table != nullptr);

  // TODO(oazizi): Should this run more frequently than TransferDataImpl?
  // This drains the relevant perf buffer, and causes Handle() callback functions to get called.
  ReadPerfBuffer(table_num);

  // Set-up current state for connection inference purposes.
  UpdateActiveConnections();

  TransferStreams(ctx, table_num, data_table);

  DumpBPFLog();
}

template <typename TValueType>
Status UpdatePerCPUArrayValue(int idx, TValueType val, ebpf::BPFPercpuArrayTable<TValueType>* arr) {
  std::vector<TValueType> values(bpf_tools::BCCWrapper::kCPUCount, val);
  auto update_res = arr->update_value(idx, values);
  if (update_res.code() != 0) {
    return error::Internal(absl::Substitute("Failed to set value on index: $0, error message: $1",
                                            idx, update_res.msg()));
  }
  return Status::OK();
}

Status SocketTraceConnector::Configure(TrafficProtocol protocol, uint64_t config_mask) {
  auto control_map_handle = bpf().get_percpu_array_table<uint64_t>(kControlMapName);
  return UpdatePerCPUArrayValue(static_cast<int>(protocol), config_mask, &control_map_handle);
}

Status SocketTraceConnector::TestOnlySetTargetPID(int64_t pid) {
  auto control_map_handle = bpf().get_percpu_array_table<int64_t>(kControlValuesArrayName);
  return UpdatePerCPUArrayValue(kTargetTGIDIndex, pid, &control_map_handle);
}

Status SocketTraceConnector::DisableSelfTracing() {
  auto control_map_handle = bpf().get_percpu_array_table<int64_t>(kControlValuesArrayName);
  int64_t my_pid = getpid();
  return UpdatePerCPUArrayValue(kStirlingTGIDIndex, my_pid, &control_map_handle);
}

//-----------------------------------------------------------------------------
// Perf Buffer Polling and Callback functions.
//-----------------------------------------------------------------------------

void SocketTraceConnector::ReadPerfBuffer(uint32_t table_num) {
  DCHECK_LT(table_num, kTablePerfBufferMap.size())
      << "Index out of bound. Trying to read from perf buffer that doesn't exist.";
  auto buffer_names = kTablePerfBufferMap[table_num];
  for (auto& buffer_name : buffer_names) {
    PollPerfBuffer(buffer_name);
  }
}

void SocketTraceConnector::HandleDataEvent(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  auto data_event_ptr = std::make_unique<SocketDataEvent>(data);
  data_event_ptr->attr.entry_timestamp_ns += system::Config::GetInstance().ClockRealTimeOffset();
  data_event_ptr->attr.return_timestamp_ns += system::Config::GetInstance().ClockRealTimeOffset();
  connector->AcceptDataEvent(std::move(data_event_ptr));
}

namespace {

std::string ProbeLossMessage(std::string_view perf_buffer_name, uint64_t lost) {
  return absl::Substitute("$0 lost $1 samples.", perf_buffer_name, lost);
}

}  // namespace

void SocketTraceConnector::HandleDataEventsLoss(void* /*cb_cookie*/, uint64_t lost) {
  LOG(WARNING) << ProbeLossMessage("socket_data_events", lost);
}

void SocketTraceConnector::HandleControlEvent(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  auto event = *static_cast<const socket_control_event_t*>(data);
  // timestamp_ns is a common field of open and close fields.
  event.open.timestamp_ns += system::Config::GetInstance().ClockRealTimeOffset();
  connector->AcceptControlEvent(event);
}

void SocketTraceConnector::HandleControlEventsLoss(void* /*cb_cookie*/, uint64_t lost) {
  LOG(WARNING) << ProbeLossMessage("socket_control_events", lost);
}

void SocketTraceConnector::HandleHTTP2HeaderEvent(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";

  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);

  auto event = std::make_unique<HTTP2HeaderEvent>(data);

  VLOG(3) << absl::Substitute(
      "t=$0 pid=$1 type=$2 fd=$3 generation=$4 stream_id=$5 end_stream=$6 name=$7 value=$8",
      event->attr.timestamp_ns, event->attr.conn_id.upid.pid,
      magic_enum::enum_name(event->attr.type), event->attr.conn_id.fd,
      event->attr.conn_id.generation, event->attr.stream_id, event->attr.end_stream, event->name,
      event->value);
  event->attr.timestamp_ns += system::Config::GetInstance().ClockRealTimeOffset();
  connector->AcceptHTTP2Header(std::move(event));
}

void SocketTraceConnector::HandleHTTP2HeaderEventLoss(void* /*cb_cookie*/, uint64_t lost) {
  LOG(WARNING) << ProbeLossMessage("go_grpc_header_events", lost);
}

void SocketTraceConnector::HandleHTTP2Data(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";

  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  // Directly access data through a go_grpc_data_event_t pointer results in mis-aligned access.
  // go_grpc_data_event_t is 8-bytes aligned, data is 4-bytes.
  auto event = std::make_unique<HTTP2DataEvent>(data);

  VLOG(3) << absl::Substitute(
      "t=$0 pid=$1 type=$2 fd=$3 generation=$4 stream_id=$5 end_stream=$6 data=$7",
      event->attr.timestamp_ns, event->attr.conn_id.upid.pid,
      magic_enum::enum_name(event->attr.type), event->attr.conn_id.fd,
      event->attr.conn_id.generation, event->attr.stream_id, event->attr.end_stream,
      event->payload);
  event->attr.timestamp_ns += system::Config::GetInstance().ClockRealTimeOffset();
  connector->AcceptHTTP2Data(std::move(event));
}

void SocketTraceConnector::HandleHTTP2DataLoss(void* /*cb_cookie*/, uint64_t lost) {
  LOG(WARNING) << ProbeLossMessage("go_grpc_data_events", lost);
}

//-----------------------------------------------------------------------------
// Connection Tracker Events
//-----------------------------------------------------------------------------

namespace {

uint64_t GetConnMapKey(struct conn_id_t conn_id) {
  return (static_cast<uint64_t>(conn_id.upid.pid) << 32) | conn_id.fd;
}

void SocketDataEventToPB(const SocketDataEvent& event, sockeventpb::SocketDataEvent* pb) {
  pb->mutable_attr()->set_entry_timestamp_ns(event.attr.entry_timestamp_ns);
  pb->mutable_attr()->set_return_timestamp_ns(event.attr.return_timestamp_ns);
  pb->mutable_attr()->mutable_conn_id()->set_pid(event.attr.conn_id.upid.pid);
  pb->mutable_attr()->mutable_conn_id()->set_start_time_ns(
      event.attr.conn_id.upid.start_time_ticks);
  pb->mutable_attr()->mutable_conn_id()->set_fd(event.attr.conn_id.fd);
  pb->mutable_attr()->mutable_conn_id()->set_generation(event.attr.conn_id.generation);
  pb->mutable_attr()->mutable_traffic_class()->set_protocol(event.attr.traffic_class.protocol);
  pb->mutable_attr()->mutable_traffic_class()->set_role(event.attr.traffic_class.role);
  pb->mutable_attr()->set_direction(event.attr.direction);
  pb->mutable_attr()->set_seq_num(event.attr.seq_num);
  pb->mutable_attr()->set_msg_size(event.attr.msg_size);
  pb->set_msg(event.msg);
}

}  // namespace

void SocketTraceConnector::AcceptDataEvent(std::unique_ptr<SocketDataEvent> event) {
  if (perf_buffer_events_output_stream_ != nullptr) {
    sockeventpb::SocketDataEvent pb;
    SocketDataEventToPB(*event, &pb);
    std::string text;
    switch (perf_buffer_events_output_format_) {
      case OutputFormat::kTxt:
        // TextFormat::Print() can print to a stream. That complicates things a bit, and we opt not
        // to do that as this is for debugging.
        TextFormat::PrintToString(pb, &text);
        // TextFormat already output a \n, so no need to do it here.
        *perf_buffer_events_output_stream_ << text;
        break;
      case OutputFormat::kBin:
        rio::SerializeToStream(pb, perf_buffer_events_output_stream_.get());
        *perf_buffer_events_output_stream_ << std::flush;
        break;
    }
  }

  const uint64_t conn_map_key = GetConnMapKey(event->attr.conn_id);
  DCHECK(conn_map_key != 0) << "Connection map key cannot be 0, pid must be wrong";
  DCHECK(event->attr.traffic_class.protocol == kProtocolHTTP ||
         event->attr.traffic_class.protocol == kProtocolHTTP2 ||
         event->attr.traffic_class.protocol == kProtocolMySQL)
      << absl::Substitute("AcceptDataEvent ignored due to unknown protocol: $0",
                          event->attr.traffic_class.protocol);

  ConnectionTracker& tracker = connection_trackers_[conn_map_key][event->attr.conn_id.generation];
  tracker.AddDataEvent(std::move(event));
}

void SocketTraceConnector::AcceptControlEvent(const socket_control_event_t& event) {
  // conn_id is a common field of open & close.
  const uint64_t conn_map_key = GetConnMapKey(event.open.conn_id);
  DCHECK(conn_map_key != 0) << "Connection map key cannot be 0, pid must be wrong";
  ConnectionTracker& tracker = connection_trackers_[conn_map_key][event.open.conn_id.generation];
  tracker.AddControlEvent(event);
}

void SocketTraceConnector::AcceptHTTP2Header(std::unique_ptr<HTTP2HeaderEvent> event) {
  const uint64_t conn_map_key = GetConnMapKey(event->attr.conn_id);
  DCHECK(conn_map_key != 0) << "Connection map key cannot be 0, pid must be wrong";
  ConnectionTracker& tracker = connection_trackers_[conn_map_key][event->attr.conn_id.generation];
  tracker.AddHTTP2Header(std::move(event));
}

void SocketTraceConnector::AcceptHTTP2Data(std::unique_ptr<HTTP2DataEvent> event) {
  const uint64_t conn_map_key = GetConnMapKey(event->attr.conn_id);
  DCHECK(conn_map_key != 0) << "Connection map key cannot be 0, pid must be wrong";
  ConnectionTracker& tracker = connection_trackers_[conn_map_key][event->attr.conn_id.generation];
  tracker.AddHTTP2Data(std::move(event));
}

const ConnectionTracker* SocketTraceConnector::GetConnectionTracker(
    struct conn_id_t conn_id) const {
  const uint64_t conn_map_key = GetConnMapKey(conn_id);

  auto tracker_set_it = connection_trackers_.find(conn_map_key);
  if (tracker_set_it == connection_trackers_.end()) {
    return nullptr;
  }

  const auto& tracker_generations = tracker_set_it->second;
  auto tracker_it = tracker_generations.find(conn_id.generation);
  if (tracker_it == tracker_generations.end()) {
    return nullptr;
  }

  return &tracker_it->second;
}

//-----------------------------------------------------------------------------
// Append-Related Functions
//-----------------------------------------------------------------------------

namespace {

HTTPContentType DetectContentType(const http::HTTPMessage& message) {
  auto content_type_iter = message.http_headers.find(http::kContentType);
  if (content_type_iter == message.http_headers.end()) {
    return HTTPContentType::kUnknown;
  }
  if (absl::StrContains(content_type_iter->second, "json")) {
    return HTTPContentType::kJSON;
  }
  return HTTPContentType::kUnknown;
}

int64_t CalculateLatency(int64_t req_timestamp_ns, int64_t resp_timestamp_ns) {
  int64_t latency_ns = 0;
  if (req_timestamp_ns > 0) {
    latency_ns = resp_timestamp_ns - req_timestamp_ns;
    // TODO(oazizi): Change to DFATAL once req-resp matching algorithms are stable and tested.
    LOG_IF(WARNING, latency_ns < 0)
        << absl::Substitute("Negative latency implies req resp mismatch [t_req=$0, t_resp=$1].",
                            req_timestamp_ns, resp_timestamp_ns);
  }
  return latency_ns;
}

}  // namespace

bool SocketTraceConnector::SelectMessage(const http::Record& record) {
  const http::HTTPMessage& message = record.resp;

  // Rule: Exclude anything that doesn't specify its Content-Type.
  auto content_type_iter = message.http_headers.find(http::kContentType);
  if (content_type_iter == message.http_headers.end()) {
    return false;
  }

  // Rule: Exclude anything that doesn't match the filter, if filter is active.
  if (message.type == MessageType::kResponse &&
      (!http_response_header_filter_.inclusions.empty() ||
       !http_response_header_filter_.exclusions.empty())) {
    if (!MatchesHTTPTHeaders(message.http_headers, http_response_header_filter_)) {
      return false;
    }
  }

  return true;
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker, http::Record record,
                                         DataTable* data_table) {
  // Only allow certain records to be transferred upstream.
  if (!SelectMessage(record)) {
    return;
  }

  // Currently decompresses gzip content, but could handle other transformations too.
  // Note that we do this after filtering to avoid burning CPU cycles unnecessarily.
  PreProcessMessage(&record.resp);

  DCHECK_EQ(kHTTPTable.elements().size(), data_table->ActiveRecordBatch()->size());

  http::HTTPMessage& req_message = record.req;
  http::HTTPMessage& resp_message = record.resp;

  md::UPID upid(ctx->AgentMetadataState()->asid(), conn_tracker.pid(),
                conn_tracker.pid_start_time_ticks());

  RecordBuilder<&kHTTPTable> r(data_table);
  r.Append<r.ColIndex("time_")>(resp_message.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  // Note that there is a string copy here,
  // But std::move is not allowed because we re-use conn object.
  r.Append<r.ColIndex("remote_addr")>(std::string(conn_tracker.remote_addr()));
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_port());
  r.Append<r.ColIndex("http_major_version")>(1);
  r.Append<r.ColIndex("http_minor_version")>(resp_message.http_minor_version);
  r.Append<r.ColIndex("http_content_type")>(static_cast<uint64_t>(DetectContentType(resp_message)));
  r.Append<r.ColIndex("http_req_headers")>(WriteMapAsJSON(req_message.http_headers));
  r.Append<r.ColIndex("http_req_method")>(std::move(req_message.http_req_method));
  r.Append<r.ColIndex("http_req_path")>(std::move(req_message.http_req_path));
  r.Append<r.ColIndex("http_req_body")>("-");
  r.Append<r.ColIndex("http_resp_headers")>(WriteMapAsJSON(resp_message.http_headers));
  r.Append<r.ColIndex("http_resp_status")>(resp_message.http_resp_status);
  r.Append<r.ColIndex("http_resp_message")>(std::move(resp_message.http_resp_message));
  r.Append<r.ColIndex("http_resp_body")>(std::move(resp_message.http_msg_body));
  r.Append<r.ColIndex("http_resp_latency_ns")>(
      CalculateLatency(req_message.timestamp_ns, resp_message.timestamp_ns));
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker,
                                         http2::Record record, DataTable* data_table) {
  DCHECK_EQ(kHTTPTable.elements().size(), data_table->ActiveRecordBatch()->size());

  HTTP2Message& req_message = record.req;
  HTTP2Message& resp_message = record.resp;

  int64_t resp_status;
  ECHECK(absl::SimpleAtoi(resp_message.headers.ValueByKey(":status", "-1"), &resp_status));

  md::UPID upid(ctx->AgentMetadataState()->asid(), conn_tracker.pid(),
                conn_tracker.pid_start_time_ticks());

  std::string method = req_message.headers.ValueByKey(":method");

  if (FLAGS_stirling_enable_parsing_protobufs) {
    MethodInputOutput rpc = grpc_desc_db_.GetMethodInputOutput(::pl::grpc::MethodPath(method));
    req_message.message = ParsePB(req_message.message, rpc.input.get());
    resp_message.message = ParsePB(resp_message.message, rpc.output.get());
  }

  RecordBuilder<&kHTTPTable> r(data_table);
  r.Append<r.ColIndex("time_")>(resp_message.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(std::string(conn_tracker.remote_addr()));
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_port());
  r.Append<r.ColIndex("http_major_version")>(2);
  // HTTP2 does not define minor version.
  r.Append<r.ColIndex("http_minor_version")>(0);
  r.Append<r.ColIndex("http_req_headers")>(WriteMapAsJSON(req_message.headers));
  r.Append<r.ColIndex("http_content_type")>(static_cast<uint64_t>(HTTPContentType::kGRPC));
  r.Append<r.ColIndex("http_resp_headers")>(WriteMapAsJSON(resp_message.headers));
  r.Append<r.ColIndex("http_req_method")>(method);
  r.Append<r.ColIndex("http_req_path")>(req_message.headers.ValueByKey(":path"));
  r.Append<r.ColIndex("http_resp_status")>(resp_status);
  // TODO(yzhao): Populate the following field from headers.
  r.Append<r.ColIndex("http_resp_message")>("-");
  r.Append<r.ColIndex("http_req_body")>(std::move(req_message.message));
  r.Append<r.ColIndex("http_resp_body")>(std::move(resp_message.message));
  r.Append<r.ColIndex("http_resp_latency_ns")>(
      CalculateLatency(req_message.timestamp_ns, resp_message.timestamp_ns));
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker,
                                         http2::NewRecord record, DataTable* data_table) {
  DCHECK_EQ(kHTTPTable.elements().size(), data_table->ActiveRecordBatch()->size());

  // With three pieces of information, we can classify a frame as part of request or response
  // stream:
  // ROLE, STREAM_ID, EVENT_TYPE -> classification
  // client, client-initiated (odd) stream, write  -> request
  // client, client-initiated (odd) stream, read   -> response
  // client, server-initiated (even) stream, write -> response
  // client, server-initiated (even) stream, read  -> request
  // server, client-initiated (odd) stream, write  -> response
  // server, client-initiated (odd) stream, read   -> request
  // server, server-initiated (even) stream, write -> request
  // server, server-initiated (even) stream, read  -> response
  //
  // While the STREAM_ID and EVENT_TYPE are easy to figure out, the ROLE is a bit harder
  // in cases where the beginning of the connection was not recorded.
  //
  // So instead of this truth table, we instead just examine the headers to classify
  // request/response streams. In particular, we look for :method and :status.

  bool send_has_method = !record.send.headers.ValueByKey(":method", "").empty();
  bool recv_has_method = !record.recv.headers.ValueByKey(":method", "").empty();
  bool send_has_status = !record.send.headers.ValueByKey(":status", "").empty();
  bool recv_has_status = !record.recv.headers.ValueByKey(":status", "").empty();

  LOG_IF(WARNING, send_has_method == recv_has_method)
      << absl::Substitute(":method only expected in only one direction (total = $0).",
                          send_has_method + recv_has_method);
  LOG_IF(WARNING, send_has_status == recv_has_status)
      << absl::Substitute(":status only expected in only one direction (total = $0).",
                          send_has_status + recv_has_status);
  LOG_IF(WARNING, send_has_method && send_has_status)
      << ":method and :status not expected in same direction (send).";
  LOG_IF(WARNING, recv_has_method && recv_has_status)
      << ":method and :status not expected in same direction (recv).";

  // Check for either :method in sender or :status in recv, for robustness.
  // Since we can capture data mid-stream, the more robust option is looking for :status.
  // But keep both in case of dropped perf buffer events.
  bool requestor_role = send_has_method || recv_has_status;

  http2::HalfStream* req_stream;
  http2::HalfStream* resp_stream;

  // Depending on whether the traced entity was the requestor or responder,
  // we need to flip the interpretation of the half-streams.
  if (requestor_role) {
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

  md::UPID upid(ctx->AgentMetadataState()->asid(), conn_tracker.pid(),
                conn_tracker.pid_start_time_ticks());

  std::string method = req_stream->headers.ValueByKey(":method");

  if (FLAGS_stirling_enable_parsing_protobufs) {
    MethodInputOutput rpc = grpc_desc_db_.GetMethodInputOutput(::pl::grpc::MethodPath(method));
    req_stream->data = ParsePB(req_stream->data, rpc.input.get());
    resp_stream->data = ParsePB(resp_stream->data, rpc.output.get());
  }

  RecordBuilder<&kHTTPTable> r(data_table);
  r.Append<r.ColIndex("time_")>(req_stream->timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("remote_addr")>(std::string(conn_tracker.remote_addr()));
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_port());
  r.Append<r.ColIndex("http_major_version")>(2);
  // HTTP2 does not define minor version.
  r.Append<r.ColIndex("http_minor_version")>(0);
  r.Append<r.ColIndex("http_req_headers")>(WriteMapAsJSON(req_stream->headers));
  r.Append<r.ColIndex("http_content_type")>(static_cast<uint64_t>(HTTPContentType::kGRPC));
  r.Append<r.ColIndex("http_resp_headers")>(WriteMapAsJSON(resp_stream->headers));
  r.Append<r.ColIndex("http_req_method")>(method);
  r.Append<r.ColIndex("http_req_path")>(req_stream->headers.ValueByKey(":path"));
  r.Append<r.ColIndex("http_resp_status")>(resp_status);
  // TODO(yzhao): Populate the following field from headers.
  r.Append<r.ColIndex("http_resp_message")>("OK");
  r.Append<r.ColIndex("http_req_body")>(std::move(req_stream->data));
  r.Append<r.ColIndex("http_resp_body")>(std::move(resp_stream->data));
  r.Append<r.ColIndex("http_resp_latency_ns")>(
      CalculateLatency(req_stream->timestamp_ns, resp_stream->timestamp_ns));
}

template <>
void SocketTraceConnector::AppendMessage(ConnectorContext* ctx,
                                         const ConnectionTracker& conn_tracker, mysql::Record entry,
                                         DataTable* data_table) {
  DCHECK_EQ(kMySQLTable.elements().size(), data_table->ActiveRecordBatch()->size());

  md::UPID upid(ctx->AgentMetadataState()->asid(), conn_tracker.pid(),
                conn_tracker.pid_start_time_ticks());

  RecordBuilder<&kMySQLTable> r(data_table);
  r.Append<r.ColIndex("time_")>(entry.req.timestamp_ns);
  r.Append<r.ColIndex("upid")>(upid.value());
  r.Append<r.ColIndex("pid_start_time")>(conn_tracker.pid_start_time_ticks());
  r.Append<r.ColIndex("remote_addr")>(std::string(conn_tracker.remote_addr()));
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_port());
  r.Append<r.ColIndex("req_cmd")>(static_cast<uint64_t>(entry.req.cmd));
  r.Append<r.ColIndex("req_body")>(std::move(entry.req.msg));
  r.Append<r.ColIndex("resp_status")>(static_cast<uint64_t>(entry.resp.status));
  r.Append<r.ColIndex("resp_body")>(std::move(entry.resp.msg));
  r.Append<r.ColIndex("latency_ns")>(
      CalculateLatency(entry.req.timestamp_ns, entry.resp.timestamp_ns));
}

//-----------------------------------------------------------------------------
// TransferData Helpers
//-----------------------------------------------------------------------------

void SocketTraceConnector::TransferStreams(ConnectorContext* ctx, uint32_t table_num,
                                           DataTable* data_table) {
  // TODO(oazizi): TransferStreams() is slightly inefficient because it loops through all
  //               connection trackers, but processing a mutually exclusive subset each time.
  //               This is because trackers for different tables are mixed together
  //               in a single pool. This is not a big concern as long as the number of tables
  //               is small (currently only 2).
  //               Possible solutions: 1) different pools, 2) auxiliary pool of pointers.

  // Outer loop iterates through tracker sets (keyed by PID+FD),
  // while inner loop iterates through generations of trackers for that PID+FD pair.
  auto tracker_set_it = connection_trackers_.begin();
  while (tracker_set_it != connection_trackers_.end()) {
    auto& tracker_generations = tracker_set_it->second;

    auto generation_it = tracker_generations.begin();
    while (generation_it != tracker_generations.end()) {
      auto& tracker = generation_it->second;

      VLOG(3) << absl::Substitute("Connection pid=$0 fd=$1 generation=$2 protocol=$3\n",
                                  tracker.pid(), tracker.fd(), tracker.generation(),
                                  tracker.protocol());

      const TransferSpec& transfer_spec = protocol_transfer_specs_[tracker.protocol()];

      // Don't process trackers meant for a different table_num.
      if (transfer_spec.table_num != table_num) {
        ++generation_it;
        continue;
      }

      tracker.IterationPreTick(proc_parser_.get(), socket_connections_.get());

      if (transfer_spec.transfer_fn && transfer_spec.enabled) {
        transfer_spec.transfer_fn(*this, ctx, &tracker, data_table);
      }

      tracker.IterationPostTick();

      // Only the most recent generation of a connection on a PID+FD should be active.
      // Mark all others for death (after having their data processed, of course).
      if (generation_it != --tracker_generations.end()) {
        tracker.MarkForDeath();
      }

      // Update iterator, handling deletions as we go. This must be the last line in the loop.
      generation_it = tracker.ReadyForDestruction() ? tracker_generations.erase(generation_it)
                                                    : ++generation_it;
    }

    tracker_set_it =
        tracker_generations.empty() ? connection_trackers_.erase(tracker_set_it) : ++tracker_set_it;
  }
}

template <typename TEntryType>
void SocketTraceConnector::TransferStream(ConnectorContext* ctx, ConnectionTracker* tracker,
                                          DataTable* data_table) {
  VLOG(3) << absl::StrCat("Connection\n", tracker->DebugString<TEntryType>());

  auto messages = tracker->ProcessMessages<TEntryType>();

  for (auto& msg : messages) {
    AppendMessage(ctx, *tracker, msg, data_table);
  }
}

template void SocketTraceConnector::TransferStream<http::Record>(ConnectorContext* ctx,
                                                                 ConnectionTracker* tracker,
                                                                 DataTable* data_table);
template void SocketTraceConnector::TransferStream<http2::Record>(ConnectorContext* ctx,
                                                                  ConnectionTracker* tracker,
                                                                  DataTable* data_table);
template void SocketTraceConnector::TransferStream<http2::NewRecord>(ConnectorContext* ctx,
                                                                     ConnectionTracker* tracker,
                                                                     DataTable* data_table);
template void SocketTraceConnector::TransferStream<mysql::Record>(ConnectorContext* ctx,
                                                                  ConnectionTracker* tracker,
                                                                  DataTable* data_table);

}  // namespace stirling
}  // namespace pl

#endif
