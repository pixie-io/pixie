#ifdef __linux__

#include <algorithm>
#include <vector>

#include "absl/strings/match.h"
#include "src/common/base/base.h"
#include "src/common/base/utils.h"
#include "src/stirling/event_parser.h"
#include "src/stirling/socket_trace_connector.h"

// TODO(yzhao): Consider simplify the semantic by filtering entirely on content type.
DEFINE_string(http_response_header_filters, "Content-Type:json",
              "Comma-separated strings to specify the substrings should be included for a header. "
              "The format looks like <header-1>:<substr-1>,...,<header-n>:<substr-n>. "
              "The substrings cannot include comma(s). The filters are conjunctive, "
              "therefore the headers can be duplicate. For example, "
              "'Content-Type:json,Content-Type:text' will select a HTTP response "
              "with a Content-Type header whose value contains 'json' *or* 'text'.");

namespace pl {
namespace stirling {

Status SocketTraceConnector::InitImpl() {
  if (!IsRoot()) {
    return error::PermissionDenied("BCC currently only supported as the root user.");
  }
  auto init_res = bpf_.init(std::string(kBCCScript));
  if (init_res.code() != 0) {
    return error::Internal(
        absl::StrCat("Failed to initialize BCC script, error message: ", init_res.msg()));
  }
  // TODO(yzhao): We need to clean the already attached probes after encountering a failure.
  for (const ProbeSpec& p : kProbeSpecs) {
    ebpf::StatusTuple attach_status =
        bpf_.attach_kprobe(bpf_.get_syscall_fnname(p.kernel_fn_short_name), p.trace_fn_name,
                           p.kernel_fn_offset, p.attach_type);
    if (attach_status.code() != 0) {
      return error::Internal(
          absl::StrCat("Failed to attach kprobe to kernel function: ", p.kernel_fn_short_name,
                       ", error message: ", attach_status.msg()));
    }
  }
  for (auto& perf_buffer_spec : kPerfBufferSpecs) {
    ebpf::StatusTuple open_status = bpf_.open_perf_buffer(
        perf_buffer_spec.name, perf_buffer_spec.probe_output_fn, perf_buffer_spec.probe_loss_fn,
        // TODO(yzhao): We sort of are not unified around how record_batch and
        // cb_cookie is passed to the callback. Consider unifying them.
        /*cb_cookie*/ this, perf_buffer_spec.num_pages);
    if (open_status.code() != 0) {
      return error::Internal(absl::StrCat("Failed to open perf buffer: ", perf_buffer_spec.name,
                                          ", error message: ", open_status.msg()));
    }
  }

  PL_RETURN_IF_ERROR(Configure(kProtocolHTTP, kSocketTraceSendReq | kSocketTraceRecvResp));
  PL_RETURN_IF_ERROR(Configure(kProtocolMySQL, kSocketTraceSendReq));
  PL_RETURN_IF_ERROR(Configure(kProtocolHTTP2, kSocketTraceSendReq | kSocketTraceRecvResp));

  // TODO(oazizi): if machine is ever suspended, this would have to be called again.
  InitClockRealTimeOffset();

  return Status::OK();
}

Status SocketTraceConnector::StopImpl() {
  // TODO(yzhao): We should continue to detach after encountering a failure.
  for (const ProbeSpec& p : kProbeSpecs) {
    ebpf::StatusTuple detach_status =
        bpf_.detach_kprobe(bpf_.get_syscall_fnname(p.kernel_fn_short_name), p.attach_type);
    if (detach_status.code() != 0) {
      return error::Internal(
          absl::StrCat("Failed to detach kprobe to kernel function: ", p.kernel_fn_short_name,
                       ", error message: ", detach_status.msg()));
    }
  }

  for (auto& perf_buffer_spec : kPerfBufferSpecs) {
    ebpf::StatusTuple close_status = bpf_.close_perf_buffer(perf_buffer_spec.name);
    if (close_status.code() != 0) {
      return error::Internal(absl::StrCat("Failed to close perf buffer: ", perf_buffer_spec.name,
                                          ", error message: ", close_status.msg()));
    }
  }

  return Status::OK();
}

void SocketTraceConnector::TransferDataImpl(uint32_t table_num,
                                            types::ColumnWrapperRecordBatch* record_batch) {
  CHECK_LT(table_num, kTables.size())
      << absl::StrFormat("Trying to access unexpected table: table_num=%d", table_num);
  CHECK(record_batch != nullptr) << "record_batch cannot be nullptr";

  // TODO(oazizi): Should this run more frequently than TransferDataImpl?
  // This drains the relevant perf buffer, and causes Handle() callback functions to get called.
  record_batch_ = record_batch;
  ReadPerfBuffer(table_num);
  record_batch_ = nullptr;

  // ReadPerfBuffer copies data into a reorder buffer called the write_stream_map_.
  // This call transfers the data from the
  TransferStreamData(table_num, record_batch);
}

Status SocketTraceConnector::Configure(uint32_t protocol, uint64_t config_mask) {
  auto control_map_handle = bpf_.get_array_table<uint64_t>("control_map");

  auto update_res = control_map_handle.update_value(protocol, config_mask);
  if (update_res.code() != 0) {
    return error::Internal("Failed to set control map");
  }

  config_mask_[protocol] = config_mask;

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
    auto perf_buffer = bpf_.get_perf_buffer(buffer_name.get());
    if (perf_buffer != nullptr) {
      perf_buffer->poll(1);
    }
  }
}

void SocketTraceConnector::HandleHTTPProbeOutput(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  auto* event = static_cast<socket_data_event_t*>(data);

  connector->AcceptDataEvent(*event);
}

void SocketTraceConnector::HandleMySQLProbeOutput(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  auto* event = static_cast<socket_data_event_t*>(data);

  // TODO(oazizi): Use AcceptDataEvent() to handle reorderings.
  connector->TransferMySQLEvent(*event, connector->record_batch_);
}

void SocketTraceConnector::HandleHTTP2ProbeOutput(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  auto* event = static_cast<socket_data_event_t*>(data);
  connector->AcceptDataEvent(*event);
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
// Stream Functions
//-----------------------------------------------------------------------------

namespace {

static uint64_t GetStreamId(uint32_t tgid, uint32_t conn_id) {
  return (static_cast<uint64_t>(tgid) << 32) | conn_id;
}

}  // namespace

template <typename StreamType>
void SocketTraceConnector::AppendToStream(socket_data_event_t event,
                                          std::map<uint64_t, StreamType>* streams) {
  const uint64_t stream_id = GetStreamId(event.attr.tgid, event.attr.conn_id);
  const uint64_t seq_num = event.attr.seq_num;
  auto iter = streams->find(stream_id);

  if (iter == streams->end()) {
    const auto conn_iter = connections_.find(stream_id);
    // If connection exists and stream doesn't, this is the first event.
    if (conn_iter != connections_.end()) {
      iter = RegisterStream(conn_iter->second, streams, stream_id);
    } else {
      // TODO(chengruizhe): Handle missing connect/accept in a more robust way.
      LOG(WARNING) << "Did not record connect/accept for stream " << stream_id;
      return;
    }
  }
  StreamType& stream = iter->second;
  switch (event.attr.event_type) {
    case kEventTypeSyscallSendEvent:
    case kEventTypeSyscallWriteEvent:
      stream.send_data.events.emplace(seq_num, std::move(event));
      break;
    case kEventTypeSyscallRecvEvent:
    case kEventTypeSyscallReadEvent:
      stream.recv_data.events.emplace(seq_num, std::move(event));
      break;
    default:
      LOG(ERROR) << "AppendToStream(): unrecognized event type";
  }
}

template <typename StreamType>
auto SocketTraceConnector::RegisterStream(const conn_info_t& conn_info,
                                          std::map<uint64_t, StreamType>* streams,
                                          uint64_t stream_id) {
  StreamType new_stream;
  new_stream.conn.timestamp_ns = conn_info.timestamp_ns + ClockRealTimeOffset();
  new_stream.conn.tgid = conn_info.tgid;
  new_stream.conn.fd = conn_info.fd;
  auto ip_endpoint_or = ParseSockAddr(conn_info);
  if (ip_endpoint_or.ok()) {
    new_stream.conn.remote_addr = std::move(ip_endpoint_or.ValueOrDie().ip);
    new_stream.conn.remote_port = ip_endpoint_or.ValueOrDie().port;
  } else {
    LOG(WARNING) << "Could not parse IP address.";
  }
  ip_endpoints_.emplace(stream_id, ip_endpoint_or);
  const auto new_stream_ret = streams->emplace(stream_id, std::move(new_stream));
  DCHECK(new_stream_ret.second) << absl::StrFormat(
      "Tried to insert, but stream_id exists [stream_id = %d].", stream_id);
  return new_stream_ret.first;
}

void SocketTraceConnector::AcceptDataEvent(socket_data_event_t event) {
  // Need to adjust the clocks to convert to real time.
  event.attr.timestamp_ns += ClockRealTimeOffset();
  // Event has protocol in case conn_info happened before deployment or was dropped by perf buffer.
  switch (event.attr.protocol) {
    case kProtocolHTTP:
      AppendToStream(std::move(event), &http_streams_);
      break;
    case kProtocolHTTP2:
      AppendToStream(std::move(event), &http2_streams_);
      break;
    default:
      // TODO(oazizi/yzhao): Add MySQL when it goes through streams.
      LOG(WARNING) << "AcceptDataEvent ignored due to unknown protocol: " << event.attr.protocol;
  }
}

void SocketTraceConnector::TransferStreamData(uint32_t table_num,
                                              types::ColumnWrapperRecordBatch* record_batch) {
  switch (table_num) {
    case kHTTPTableNum:
      TransferHTTPStreams(record_batch);
      break;
    case kMySQLTableNum:
      // TODO(oazizi): Convert MySQL protocol to use streams.
      // TransferMySQLStreams(record_batch);
      break;
    default:
      CHECK(false) << absl::StrFormat("Unknown table number: %d", table_num);
  }
}

void SocketTraceConnector::AcceptOpenConnEvent(const conn_info_t& conn_info) {
  const uint64_t stream_id = GetStreamId(conn_info.tgid, conn_info.conn_id);
  connections_.emplace(stream_id, conn_info);
}

void SocketTraceConnector::AcceptCloseConnEvent(const conn_info_t& conn_info) {
  const uint64_t stream_id = GetStreamId(conn_info.tgid, conn_info.conn_id);
  connections_.erase(stream_id);
  ip_endpoints_.erase(stream_id);
}

conn_info_t* SocketTraceConnector::GetConn(const socket_data_event_t& event) {
  // TODO(chengruizhe): Might want to merge tgid and conn_id into a single field (eg. tgid +
  // conn_id)
  const uint64_t stream_id = GetStreamId(event.attr.tgid, event.attr.conn_id);
  auto stream_pair = connections_.find(stream_id);
  if (stream_pair == connections_.end()) {
    return nullptr;
  }
  return &stream_pair->second;
}

//-----------------------------------------------------------------------------
// HTTP Specific TransferImpl Helpers
//-----------------------------------------------------------------------------

void SocketTraceConnector::ParseEventStream(TrafficMessageType type, DataStream* data) {
  // TODO(oazizi): Continue to propagate the templating through this function.
  EventParser<HTTPMessage> parser;

  const size_t orig_offset = data->offset;

  // Prepare all recorded events for parsing.
  std::vector<std::string_view> msgs;
  uint64_t next_seq_num = data->events.begin()->first;
  for (const auto& [seq_num, event] : data->events) {
    // Found a discontinuity in sequence numbers. Stop submitting events to parser.
    if (seq_num != next_seq_num) {
      break;
    }

    // The main message to submit to parser.
    std::string_view msg(event.msg, event.attr.msg_size);

    // First message may have been partially processed by a previous call to this function.
    // In such cases, the offset will be non-zero, and we need a sub-string of the first event.
    if (data->offset != 0) {
      CHECK(data->offset < event.attr.msg_size);
      msg = msg.substr(data->offset, event.attr.msg_size - data->offset);
      data->offset = 0;
    }

    parser.Append(msg, event.attr.timestamp_ns + ClockRealTimeOffset());
    msgs.push_back(msg);
    next_seq_num++;
  }

  // Now parse all the appended events.
  ParseResult<BufferPosition> parse_result = parser.ParseMessages(type, &data->messages);

  // If we weren't able to process anything new, then the offset should be the same as last time.
  if (data->offset != 0 && parse_result.end_position.seq_num == 0) {
    CHECK_EQ(parse_result.end_position.offset, orig_offset);
  }

  // Find and erase events that have been fully processed.
  auto erase_iter = data->events.begin();
  std::advance(erase_iter, parse_result.end_position.seq_num);
  data->events.erase(data->events.begin(), erase_iter);
  data->offset = parse_result.end_position.offset;
}

void SocketTraceConnector::TransferHTTPStreams(types::ColumnWrapperRecordBatch* record_batch) {
  for (auto& [id, stream] : http_streams_) {
    PL_UNUSED(id);

    // TODO(oazizi): I don't like this way of detecting requestor vs responder. But works for now.
    bool is_requestor_side = (config_mask_[stream.protocol] & kSocketTraceSendReq) ||
                             (config_mask_[stream.protocol] & kSocketTraceRecvResp);
    bool is_responder_side = (config_mask_[stream.protocol] & kSocketTraceSendResp) ||
                             (config_mask_[stream.protocol] & kSocketTraceRecvReq);
    CHECK(is_requestor_side ^ is_responder_side)
        << absl::StrFormat("Must be either requestor or responder (and not both)");

    auto& resp_data = is_requestor_side ? stream.recv_data : stream.send_data;
    ParseEventStream(kMessageTypeResponses, &resp_data);

    auto& req_data = is_requestor_side ? stream.send_data : stream.recv_data;
    ParseEventStream(kMessageTypeRequests, &req_data);

    // TODO(oazizi): If we stick with this approach, resp_data could be converted back to vector.
    for (HTTPMessage& msg : resp_data.messages) {
      if (!req_data.messages.empty()) {
        HTTPTraceRecord record{stream.conn, std::move(req_data.messages.front()), std::move(msg)};
        req_data.messages.pop_front();
        ConsumeHTTPMessage(std::move(record), record_batch);
      } else {
        HTTPTraceRecord record{stream.conn, HTTPMessage(), std::move(msg)};
        ConsumeHTTPMessage(std::move(record), record_batch);
      }
    }
    resp_data.messages.clear();
  }

  // TODO(yzhao): Add the capability to remove events that are too old.
  // TODO(yzhao): Consider change the data structure to a vector, and use sorting to order events
  // before stitching. That might be faster (verify with benchmark).
}

void SocketTraceConnector::ConsumeHTTPMessage(HTTPTraceRecord record,
                                              types::ColumnWrapperRecordBatch* record_batch) {
  // Only allow certain records to be transferred upstream.
  if (SelectHTTPMessage(record)) {
    // Currently decompresses gzip content, but could handle other transformations too.
    // Note that we do this after filtering to avoid burning CPU cycles unnecessarily.
    PreProcessMessage(&record.resp_message);

    // Push data to the TableStore.
    AppendHTTPMessage(std::move(record), record_batch);
  }
}

bool SocketTraceConnector::SelectHTTPMessage(const HTTPTraceRecord& record) {
  // Some of this function is currently a placeholder for the demo.
  // TODO(oazizi/yzhao): update this function further.

  const HTTPMessage& message = record.resp_message;

  // Rule: Exclude any HTTP requests.
  // TODO(oazizi): Think about how requests should be handled by this function.
  if (message.type == SocketTraceEventType::kHTTPRequest) {
    return false;
  }

  // Rule: Exclude anything that doesn't specify its Content-Type.
  auto content_type_iter = message.http_headers.find(http_headers::kContentType);
  if (content_type_iter == message.http_headers.end()) {
    return false;
  }

  // Rule: Exclude anything that doesn't match the filter, if filter is active.
  if (message.type == SocketTraceEventType::kHTTPResponse &&
      (!http_response_header_filter_.inclusions.empty() ||
       !http_response_header_filter_.exclusions.empty())) {
    if (!MatchesHTTPTHeaders(message.http_headers, http_response_header_filter_)) {
      return false;
    }
  }

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
  if (absl::StrContains(content_type_iter->second, "grpc")) {
    return HTTPContentType::kGRPC;
  }
  return HTTPContentType::kUnknown;
}

}  // namespace

void SocketTraceConnector::AppendHTTPMessage(HTTPTraceRecord record,
                                             types::ColumnWrapperRecordBatch* record_batch) {
  CHECK_EQ(kHTTPTable.elements().size(), record_batch->size());

  const SocketConnection& conn = record.conn;
  HTTPMessage& req_message = record.req_message;
  HTTPMessage& resp_message = record.resp_message;

  // Check for positive latencies.
  DCHECK_GE(resp_message.timestamp_ns, conn.timestamp_ns);

  RecordBuilder<&kHTTPTable> r(record_batch);
  r.Append<r.ColIndex("time_")>(resp_message.timestamp_ns);
  r.Append<r.ColIndex("tgid")>(conn.tgid);
  r.Append<r.ColIndex("fd")>(conn.fd);
  // TODO(oazizi): Kill this?
  r.Append<r.ColIndex("event_type")>(EventTypeToString(resp_message.type));
  // Note that there is a string copy here,
  // But std::move is not allowed because we re-use conn object.
  // TODO(oazizi): Long-term need to make remote_addr a uint128.
  r.Append<r.ColIndex("remote_addr")>(std::string(conn.remote_addr));
  r.Append<r.ColIndex("remote_port")>(conn.remote_port);
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
  r.Append<r.ColIndex("http_resp_latency_ns")>(resp_message.timestamp_ns - conn.timestamp_ns);
  // TODO(oazizi): Change to req timestamp when it exists.
}

//-----------------------------------------------------------------------------
// MySQL Specific TransferImpl Helpers
//-----------------------------------------------------------------------------

void SocketTraceConnector::TransferMySQLEvent(const socket_data_event_t& event,
                                              types::ColumnWrapperRecordBatch* record_batch) {
  // TODO(oazizi): Enable the below to only capture requestor-side messages.
  //  if (event.attr.event_type != kEventTypeSyscallWriteEvent &&
  //      event.attr.event_type != kEventTypeSyscallSendEvent) {
  //    return;
  //  }
  // TODO(chengruizhe): Get stream_id only once, instead of twice
  conn_info_t* conn_info = GetConn(event);
  uint64_t stream_id = GetStreamId(event.attr.tgid, event.attr.conn_id);
  int fd = -1;
  std::string ip = "-";
  int port = -1;
  if (conn_info) {
    auto s_iter = ip_endpoints_.find(stream_id);
    if (s_iter != ip_endpoints_.end()) {
      auto s = s_iter->second;
      IPEndpoint remote_sockaddr = s.ok() ? s.ConsumeValueOrDie() : IPEndpoint();
      fd = conn_info->fd;
      ip = remote_sockaddr.ip;
      port = remote_sockaddr.port;
    } else {
      LOG(WARNING) << "Could not find ipEndpoint for stream: " << stream_id;
    }
  }

  RecordBuilder<&kMySQLTable> r(record_batch);
  r.Append<r.ColIndex("time_")>(event.attr.timestamp_ns + ClockRealTimeOffset());
  r.Append<r.ColIndex("tgid")>(event.attr.tgid);
  r.Append<r.ColIndex("fd")>(fd);
  r.Append<r.ColIndex("bpf_event")>(event.attr.event_type);
  r.Append<r.ColIndex("remote_addr")>(std::move(ip));
  r.Append<r.ColIndex("remote_port")>(port);
  r.Append<r.ColIndex("body")>(std::string(event.msg, event.attr.msg_size));
}

}  // namespace stirling
}  // namespace pl

#endif
