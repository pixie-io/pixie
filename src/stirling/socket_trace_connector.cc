#ifdef __linux__

#include <google/protobuf/util/json_util.h>
#include <deque>
#include <utility>

#include "absl/strings/match.h"
#include "src/common/base/base.h"
#include "src/common/base/utils.h"
#include "src/stirling/bpf_logging.h"
#include "src/stirling/event_parser.h"
#include "src/stirling/grpc.h"
#include "src/stirling/http2.h"
#include "src/stirling/mysql_parse.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/utils/linux_headers.h"

// TODO(yzhao): Consider simplify the semantic by filtering entirely on content type.
DEFINE_string(http_response_header_filters, "Content-Type:json",
              "Comma-separated strings to specify the substrings should be included for a header. "
              "The format looks like <header-1>:<substr-1>,...,<header-n>:<substr-n>. "
              "The substrings cannot include comma(s). The filters are conjunctive, "
              "therefore the headers can be duplicate. For example, "
              "'Content-Type:json,Content-Type:text' will select a HTTP response "
              "with a Content-Type header whose value contains 'json' *or* 'text'.");
DEFINE_bool(enable_parsing_protobufs, false,
            "If true, parses binary protobufs captured in gRPC messages. "
            "As of 2019-07, the parser can only handle protobufs defined in Hipster Shop.");

namespace pl {
namespace stirling {

using ::pl::grpc::MethodInputOutput;
using ::pl::stirling::grpc::ParseProtobuf;
using ::pl::stirling::http2::Frame;
using ::pl::stirling::http2::GRPCMessage;
using ::pl::stirling::http2::GRPCReqResp;
using ::pl::stirling::http2::MatchGRPCReqResp;

// TODO(yzhao): Read CPU count during runtime and set maxactive to Multiplier * N_CPU. That way, we
// can be relatively more secure against increase of CPU count. Note the default multiplier is 2,
// which is not sufficient, as indicated in Hipster shop.
//
// AWS offers VMs with 96 vCPUs. We bump the number from 2X to 4X, and round up to 2's exponential,
// which gives 4 * 96 == 384 => 512.
//
// The implication of this parameter is explained in the "How Does a Return Probe Work?" section of
// https://www.kernel.org/doc/Documentation/kprobes.txt. In short, this controls the memory space
// used for bookkeeping, which translate to equal number of struct kretprobe in memory.
constexpr int kKprobeMaxActive = 512;

Status SocketTraceConnector::InitImpl() {
  if (!IsRoot()) {
    return error::PermissionDenied("BCC currently only supported as the root user.");
  }

  PL_RETURN_IF_ERROR(utils::FindOrInstallLinuxHeaders());

  std::vector<std::string> cflags = {"-DNDEBUG"};
  if (FLAGS_enable_bpf_logging) {
    cflags.clear();
  }
  auto init_res = bpf_.init(std::string(kBCCScript), cflags);
  if (init_res.code() != 0) {
    return error::Internal(
        absl::StrCat("Failed to initialize BCC script, error message: ", init_res.msg()));
  }
  // TODO(yzhao): We need to clean the already attached probes after encountering a failure.
  for (const ProbeSpec& p : kProbeSpecs) {
    ebpf::StatusTuple attach_status = bpf_.attach_kprobe(
        bpf_.get_syscall_fnname(std::string(p.kernel_fn_short_name)), std::string(p.trace_fn_name),
        0 /* offset */, p.attach_type, kKprobeMaxActive);
    if (attach_status.code() != 0) {
      return error::Internal(
          absl::StrCat("Failed to attach kprobe to kernel function: ", p.kernel_fn_short_name,
                       ", error message: ", attach_status.msg()));
    }
    ++num_attached_probes_;
  }
  for (auto& perf_buffer_spec : kPerfBufferSpecs) {
    ebpf::StatusTuple open_status =
        bpf_.open_perf_buffer(std::string(perf_buffer_spec.name), perf_buffer_spec.probe_output_fn,
                              perf_buffer_spec.probe_loss_fn,
                              // TODO(yzhao): We sort of are not unified around how record_batch and
                              // cb_cookie is passed to the callback. Consider unifying them.
                              /*cb_cookie*/ this, perf_buffer_spec.num_pages);
    if (open_status.code() != 0) {
      return error::Internal(absl::StrCat("Failed to open perf buffer: ", perf_buffer_spec.name,
                                          ", error message: ", open_status.msg()));
    }
    ++num_open_perf_buffers_;
  }

  PL_RETURN_IF_ERROR(InitBPFLogging(&bpf_));
  PL_RETURN_IF_ERROR(Configure(kProtocolUnknown, kSocketTraceNothing));
  PL_RETURN_IF_ERROR(Configure(kProtocolHTTP, kSocketTraceSendReq | kSocketTraceRecvResp));
  PL_RETURN_IF_ERROR(Configure(kProtocolMySQL, kSocketTraceSendReq));
  // TODO(PL-659): connect() call might return non 0 value, making requester-side tracing
  // unreliable. Switch to server-side for now.
  PL_RETURN_IF_ERROR(Configure(kProtocolHTTP2, kSocketTraceSendResp | kSocketTraceRecvReq));
  PL_RETURN_IF_ERROR(TestOnlySetTargetPID(kTraceAllTGIDs));

  return Status::OK();
}

Status SocketTraceConnector::StopImpl() {
  // TODO(yzhao): We should continue to detach after encountering a failure.
  for (const ProbeSpec& p : kProbeSpecs) {
    ebpf::StatusTuple detach_status = bpf_.detach_kprobe(
        bpf_.get_syscall_fnname(std::string(p.kernel_fn_short_name)), p.attach_type);
    if (detach_status.code() != 0) {
      return error::Internal(
          absl::StrCat("Failed to detach kprobe to kernel function: ", p.kernel_fn_short_name,
                       ", error message: ", detach_status.msg()));
    }
    --num_attached_probes_;
  }

  for (auto& perf_buffer_spec : kPerfBufferSpecs) {
    ebpf::StatusTuple close_status = bpf_.close_perf_buffer(std::string(perf_buffer_spec.name));
    if (close_status.code() != 0) {
      return error::Internal(absl::StrCat("Failed to close perf buffer: ", perf_buffer_spec.name,
                                          ", error message: ", close_status.msg()));
    }
    --num_open_perf_buffers_;
  }

  return Status::OK();
}

void SocketTraceConnector::TransferDataImpl(uint32_t table_num,
                                            types::ColumnWrapperRecordBatch* record_batch) {
  CHECK_LT(table_num, kTables.size())
      << absl::Substitute("Trying to access unexpected table: table_num=$0", table_num);
  CHECK(record_batch != nullptr) << "record_batch cannot be nullptr";

  // TODO(oazizi): Should this run more frequently than TransferDataImpl?
  // This drains the relevant perf buffer, and causes Handle() callback functions to get called.
  record_batch_ = record_batch;
  ReadPerfBuffer(table_num);
  record_batch_ = nullptr;

  switch (table_num) {
    case kHTTPTableNum:
      TransferStreams<HTTPMessage>(kProtocolHTTP, record_batch);
      TransferStreams<Frame>(kProtocolHTTP2, record_batch);
      break;
    case kMySQLTableNum:
      // TODO(oazizi): Convert MySQL protocol to use streams.
      // TransferStreams<MySQLMessage>(kProtocolMySQL, record_batch);
      break;
    default:
      CHECK(false) << absl::StrFormat("Unknown table number: %d", table_num);
  }
  DumpBPFLog(&bpf_);
}

Status SocketTraceConnector::Configure(TrafficProtocol protocol, uint64_t config_mask) {
  auto control_map_handle = bpf_.get_percpu_array_table<uint64_t>(kControlMapName);
  std::vector<uint64_t> config_mask_allcpus(kCPUCount, config_mask);
  auto update_res =
      control_map_handle.update_value(static_cast<int>(protocol), config_mask_allcpus);
  if (update_res.code() != 0) {
    return error::Internal(
        absl::StrCat("Failed to update control map, error message: ", update_res.msg()));
  }
  config_mask_[protocol] = config_mask;

  return Status::OK();
}

Status SocketTraceConnector::TestOnlySetTargetPID(int64_t pid) {
  auto control_map_handle = bpf_.get_percpu_array_table<uint64_t>(kTargetTGIDArrayName);
  std::vector<uint64_t> target_pids(kCPUCount, pid);
  auto update_res = control_map_handle.update_value(/*index*/ 0, target_pids);
  if (update_res.code() != 0) {
    return error::Internal(
        absl::StrCat("Failed to set target PID, error message: ", update_res.msg()));
  }
  return Status::OK();
}

//-----------------------------------------------------------------------------
// Perf Buffer Polling and Callback functions.
//-----------------------------------------------------------------------------

void SocketTraceConnector::ReadPerfBuffer(uint32_t table_num) {
  DCHECK_LT(table_num, kTablePerfBufferMap.size())
      << "Index out of bound. Trying to read from perf buffer that doesn't exist.";
  auto buffer_names = kTablePerfBufferMap[table_num];
  for (auto& buffer_name : buffer_names) {
    auto perf_buffer = bpf_.get_perf_buffer(std::string(buffer_name));
    if (perf_buffer != nullptr) {
      perf_buffer->poll(1);
    }
  }
}

void SocketTraceConnector::HandleHTTPProbeOutput(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  connector->AcceptDataEvent(std::make_unique<SocketDataEvent>(data));
}

void SocketTraceConnector::HandleMySQLProbeOutput(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  // TODO(oazizi): Use AcceptDataEvent() to handle reorderings.
  connector->TransferMySQLEvent(SocketDataEvent(data), connector->record_batch_);
}

// This function is invoked by BCC runtime when a item in the perf buffer is not read and lost.
// For now we do nothing.
void SocketTraceConnector::HandleProbeLoss(void* /*cb_cookie*/, uint64_t lost) {
  VLOG(1) << "Possibly lost " << lost << " samples";
  // TODO(oazizi): Can we figure out which perf buffer lost the event?
}

void SocketTraceConnector::HandleOpenProbeOutput(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  const auto conn = CopyFromBPF<conn_info_t>(data);
  connector->AcceptOpenConnEvent(conn);
}

void SocketTraceConnector::HandleCloseProbeOutput(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  const auto conn = CopyFromBPF<conn_info_t>(data);
  connector->AcceptCloseConnEvent(conn);
}

//-----------------------------------------------------------------------------
// Connection Tracker Events
//-----------------------------------------------------------------------------

namespace {

uint64_t GetConnMapKey(struct conn_id_t conn_id) {
  return (static_cast<uint64_t>(conn_id.pid) << 32) | conn_id.fd;
}

}  // namespace

void SocketTraceConnector::AcceptDataEvent(std::unique_ptr<SocketDataEvent> event) {
  const uint64_t conn_map_key = GetConnMapKey(event->attr.conn_id);
  DCHECK(conn_map_key != 0) << "Connection map key cannot be 0, pid must be wrong";

  // Need to adjust the clocks to convert to real time.
  event->attr.timestamp_ns += ClockRealTimeOffset();

  switch (event->attr.traffic_class.protocol) {
    case kProtocolHTTP:
    case kProtocolHTTP2:
      break;
    default:
      LOG(WARNING) << absl::Substitute("AcceptDataEvent ignored due to unknown protocol: $0",
                                       event->attr.traffic_class.protocol);
      return;
  }

  ConnectionTracker& tracker = connection_trackers_[conn_map_key][event->attr.conn_id.generation];
  tracker.AddDataEvent(std::move(event));
}

void SocketTraceConnector::AcceptOpenConnEvent(conn_info_t conn_info) {
  const uint64_t conn_map_key = GetConnMapKey(conn_info.conn_id);
  DCHECK(conn_map_key != 0) << "Connection map key cannot be 0, pid must be wrong";

  // Need to adjust the clocks to convert to real time.
  conn_info.timestamp_ns += ClockRealTimeOffset();

  ConnectionTracker& tracker = connection_trackers_[conn_map_key][conn_info.conn_id.generation];
  tracker.AddConnOpenEvent(conn_info);
}

void SocketTraceConnector::AcceptCloseConnEvent(conn_info_t conn_info) {
  const uint64_t conn_map_key = GetConnMapKey(conn_info.conn_id);
  DCHECK(conn_map_key != 0) << "Connection map key cannot be 0, pid must be wrong";

  // Need to adjust the clocks to convert to real time.
  conn_info.timestamp_ns += ClockRealTimeOffset();

  ConnectionTracker& tracker = connection_trackers_[conn_map_key][conn_info.conn_id.generation];
  tracker.AddConnCloseEvent(conn_info);
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
// Generic/Templatized TransferData Helpers
//-----------------------------------------------------------------------------

template <class TMessageType>
void SocketTraceConnector::TransferStreams(TrafficProtocol protocol,
                                           types::ColumnWrapperRecordBatch* record_batch) {
  // TODO(oazizi): The single connection trackers model makes TransferStreams() inefficient,
  //               because it will get called multiple times, looping through all connection
  //               trackers, but selecting a mutually exclusive subset each time.
  //               Possible solutions: 1) different pools, 2) auxiliary pool of pointers.

  // Outer loop iterates through tracker sets (keyed by PID+FD),
  // while inner loop iterates through generations of trackers for that PID+FD pair.
  auto tracker_set_it = connection_trackers_.begin();
  while (tracker_set_it != connection_trackers_.end()) {
    auto& tracker_generations = tracker_set_it->second;

    auto generation_it = tracker_generations.begin();
    while (generation_it != tracker_generations.end()) {
      auto& tracker = generation_it->second;

      if (tracker.protocol() != protocol) {
        ++generation_it;
        continue;
      }

      DataStream* resp_data = tracker.resp_data();
      if (resp_data == nullptr) {
        LOG(ERROR) << "Unexpected nullptr for resp_data";
        continue;
      }
      auto& resp_messages =
          resp_data->template ExtractMessages<TMessageType>(MessageType::kResponses);

      DataStream* req_data = tracker.req_data();
      if (req_data == nullptr) {
        LOG(ERROR) << "Unexpected nullptr for req_data";
        continue;
      }
      auto& req_messages = req_data->template ExtractMessages<TMessageType>(MessageType::kRequests);

      ProcessMessages<TMessageType>(tracker, &req_messages, &resp_messages, record_batch);

      tracker.IterationTick();

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

template <class TMessageType>
void SocketTraceConnector::ProcessMessages(const ConnectionTracker& conn_tracker,
                                           std::deque<TMessageType>* req_messages,
                                           std::deque<TMessageType>* resp_messages,
                                           types::ColumnWrapperRecordBatch* record_batch) {
  // TODO(oazizi): If we stick with this approach, resp_data could be converted back to vector.
  for (TMessageType& msg : *resp_messages) {
    if (!req_messages->empty()) {
      TraceRecord<TMessageType> record{&conn_tracker, std::move(req_messages->front()),
                                       std::move(msg)};
      req_messages->pop_front();
      ConsumeMessage(std::move(record), record_batch);
    } else {
      TraceRecord<TMessageType> record{&conn_tracker, HTTPMessage(), std::move(msg)};
      ConsumeMessage(std::move(record), record_batch);
    }
  }
  resp_messages->clear();
}

template <>
void SocketTraceConnector::ProcessMessages(const ConnectionTracker& conn_tracker,
                                           std::deque<Frame>* req_messages,
                                           std::deque<Frame>* resp_messages,
                                           types::ColumnWrapperRecordBatch* record_batch) {
  std::map<uint32_t, std::vector<GRPCMessage>> reqs;
  std::map<uint32_t, std::vector<GRPCMessage>> resps;

  // First stitch all frames to form gRPC messages.
  Status s1 = StitchGRPCStreamFrames(*req_messages, &reqs);
  Status s2 = StitchGRPCStreamFrames(*resp_messages, &resps);

  LOG_IF(ERROR, !s1.ok()) << "Failed to stitch frames for requests, error: " << s1.msg();
  LOG_IF(ERROR, !s2.ok()) << "Failed to stitch frames for responses, error: " << s2.msg();

  std::vector<GRPCReqResp> records = MatchGRPCReqResp(std::move(reqs), std::move(resps));

  for (auto& r : records) {
    r.req.MarkFramesConsumed();
    r.resp.MarkFramesConsumed();
    TraceRecord<GRPCMessage> tmp{&conn_tracker, std::move(r.req), std::move(r.resp)};
    ConsumeMessage(std::move(tmp), record_batch);
  }

  EraseConsumedFrames(req_messages);
  EraseConsumedFrames(resp_messages);
}

template <class TMessageType>
void SocketTraceConnector::ConsumeMessage(TraceRecord<TMessageType> record,
                                          types::ColumnWrapperRecordBatch* record_batch) {
  // Only allow certain records to be transferred upstream.
  if (SelectMessage(record)) {
    // Currently decompresses gzip content, but could handle other transformations too.
    // Note that we do this after filtering to avoid burning CPU cycles unnecessarily.
    PreProcessMessage(&record.resp_message);

    // Push data to the TableStore.
    AppendMessage(std::move(record), record_batch);
  }
}

//-----------------------------------------------------------------------------
// HTTP TransferData Helpers
//-----------------------------------------------------------------------------

template <>
bool SocketTraceConnector::SelectMessage(const TraceRecord<HTTPMessage>& record) {
  const HTTPMessage& message = record.resp_message;

  // Rule: Exclude anything that doesn't specify its Content-Type.
  auto content_type_iter = message.http_headers.find(http_headers::kContentType);
  if (content_type_iter == message.http_headers.end()) {
    return false;
  }

  // Rule: Exclude anything that doesn't match the filter, if filter is active.
  if (message.type == HTTPEventType::kHTTPResponse &&
      (!http_response_header_filter_.inclusions.empty() ||
       !http_response_header_filter_.exclusions.empty())) {
    if (!MatchesHTTPTHeaders(message.http_headers, http_response_header_filter_)) {
      return false;
    }
  }

  return true;
}

template <>
bool SocketTraceConnector::SelectMessage(const TraceRecord<GRPCMessage>& grpc_record) {
  PL_UNUSED(grpc_record);
  return true;
}

namespace {

HTTPContentType DetectContentType(const HTTPMessage& message) {
  auto content_type_iter = message.http_headers.find(http_headers::kContentType);
  if (content_type_iter == message.http_headers.end()) {
    return HTTPContentType::kUnknown;
  }
  if (absl::StrContains(content_type_iter->second, "json")) {
    return HTTPContentType::kJSON;
  }
  return HTTPContentType::kUnknown;
}

}  // namespace

template <>
void SocketTraceConnector::AppendMessage(TraceRecord<HTTPMessage> record,
                                         types::ColumnWrapperRecordBatch* record_batch) {
  CHECK_EQ(kHTTPTable.elements().size(), record_batch->size());

  const ConnectionTracker& conn_tracker = *record.tracker;
  HTTPMessage& req_message = record.req_message;
  HTTPMessage& resp_message = record.resp_message;

  // Check for positive latencies.
  DCHECK_GE(resp_message.timestamp_ns, req_message.timestamp_ns);

  RecordBuilder<&kHTTPTable> r(record_batch);
  r.Append<r.ColIndex("time_")>(resp_message.timestamp_ns);
  r.Append<r.ColIndex("pid")>(conn_tracker.pid());
  r.Append<r.ColIndex("pid_start_time")>(conn_tracker.pid_start_time());
  r.Append<r.ColIndex("event_type")>(HTTPEventTypeToString(resp_message.type));
  // Note that there is a string copy here,
  // But std::move is not allowed because we re-use conn object.
  r.Append<r.ColIndex("remote_addr")>(std::string(conn_tracker.remote_addr()));
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_port());
  r.Append<r.ColIndex("http_major_version")>(1);
  r.Append<r.ColIndex("http_minor_version")>(resp_message.http_minor_version);
  r.Append<r.ColIndex("http_headers")>(
      absl::StrJoin(resp_message.http_headers, "\n", absl::PairFormatter(": ")));
  r.Append<r.ColIndex("http_content_type")>(static_cast<uint64_t>(DetectContentType(resp_message)));
  r.Append<r.ColIndex("http_req_method")>(std::move(req_message.http_req_method));
  r.Append<r.ColIndex("http_req_path")>(std::move(req_message.http_req_path));
  r.Append<r.ColIndex("http_resp_status")>(resp_message.http_resp_status);
  r.Append<r.ColIndex("http_resp_message")>(std::move(resp_message.http_resp_message));
  r.Append<r.ColIndex("http_resp_body")>(std::move(resp_message.http_msg_body));
  r.Append<r.ColIndex("http_resp_latency_ns")>(resp_message.timestamp_ns -
                                               req_message.timestamp_ns);
}

template <>
void SocketTraceConnector::AppendMessage(TraceRecord<GRPCMessage> record,
                                         types::ColumnWrapperRecordBatch* record_batch) {
  CHECK_EQ(kHTTPTable.elements().size(), record_batch->size());

  const ConnectionTracker& conn_tracker = *record.tracker;
  GRPCMessage& req_message = record.req_message;
  GRPCMessage& resp_message = record.resp_message;

  DCHECK_GE(resp_message.timestamp_ns, req_message.timestamp_ns);

  RecordBuilder<&kHTTPTable> r(record_batch);
  r.Append<r.ColIndex("time_")>(resp_message.timestamp_ns);
  r.Append<r.ColIndex("pid")>(conn_tracker.pid());
  r.Append<r.ColIndex("pid_start_time")>(conn_tracker.pid_start_time());
  r.Append<r.ColIndex("event_type")>("mixed");
  r.Append<r.ColIndex("remote_addr")>(std::string(conn_tracker.remote_addr()));
  r.Append<r.ColIndex("remote_port")>(conn_tracker.remote_port());
  r.Append<r.ColIndex("http_major_version")>(2);
  // HTTP2 does not define minor version.
  r.Append<r.ColIndex("http_minor_version")>(0);
  // TODO(yzhao): Populate resp_headers as well.
  // gRPC request headers are more interesting.
  r.Append<r.ColIndex("http_headers")>(
      absl::StrJoin(req_message.headers, "\n", absl::PairFormatter(": ")));
  r.Append<r.ColIndex("http_content_type")>(static_cast<uint64_t>(HTTPContentType::kGRPC));
  // TODO(yzhao): Populate the following 4 fields from headers.
  r.Append<r.ColIndex("http_req_method")>("GET");
  r.Append<r.ColIndex("http_req_path")>("PATH");
  r.Append<r.ColIndex("http_resp_status")>(200);
  r.Append<r.ColIndex("http_resp_message")>("OK");
  // TODO(yzhao): Populate this field with parsed text format protobufs.

  if (FLAGS_enable_parsing_protobufs) {
    MethodInputOutput in_out = GetProtobufMessages(req_message, &grpc_desc_db_);
    std::string json;
    Status s = ParseProtobuf(resp_message.message, in_out.output.get(), &json);
    if (s.ok()) {
      r.Append<r.ColIndex("http_resp_body")>(std::move(json));
    } else {
      std::string msg = s.ToString();
      LOG(ERROR) << msg;
      r.Append<r.ColIndex("http_resp_body")>(std::move(msg));
    }
  } else {
    r.Append<r.ColIndex("http_resp_body")>(std::move(resp_message.message));
  }
  r.Append<r.ColIndex("http_resp_latency_ns")>(resp_message.timestamp_ns -
                                               req_message.timestamp_ns);
}

//-----------------------------------------------------------------------------
// MySQL TransferData Helpers
//-----------------------------------------------------------------------------

void SocketTraceConnector::TransferMySQLEvent(SocketDataEvent event,
                                              types::ColumnWrapperRecordBatch* record_batch) {
  // TODO(chengruizhe/oazizi): Add connection info back, once MySQL uses a ConnectionTracker.
  int fd = -1;
  std::string ip = "-";
  int port = -1;

  RecordBuilder<&kMySQLTable> r(record_batch);
  r.Append<r.ColIndex("time_")>(event.attr.timestamp_ns + ClockRealTimeOffset());
  r.Append<r.ColIndex("pid")>(event.attr.conn_id.pid);
  r.Append<r.ColIndex("pid_start_time")>(event.attr.conn_id.pid_start_time_ns);
  r.Append<r.ColIndex("fd")>(fd);
  r.Append<r.ColIndex("bpf_event")>(event.attr.event_type);
  r.Append<r.ColIndex("remote_addr")>(std::move(ip));
  r.Append<r.ColIndex("remote_port")>(port);
  r.Append<r.ColIndex("body")>(std::move(event.msg));
}

}  // namespace stirling
}  // namespace pl

#endif
