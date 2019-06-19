#ifdef __linux__

#include <algorithm>
#include <vector>

#include "absl/strings/match.h"
#include "src/common/base/base.h"
#include "src/stirling/http_parse.h"
#include "src/stirling/socket_trace_connector.h"

// TODO(yzhao): This is only for inclusion. We can add another flag for exclusion, or come up with a
// filter format that support exclusion in the same flag (for example, we can add '-' at the
// beginning of the filter to indicate it's a exclusion filter: -Content-Type:json, which means a
// HTTP response with the 'Content-Type' header contains 'json' should *not* be selected.
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
  auto perf_buffer = bpf_.get_perf_buffer(kPerfBufferSpecs[table_num].name);
  if (perf_buffer != nullptr) {
    perf_buffer->poll(1);
  }
}

void SocketTraceConnector::HandleHTTPProbeOutput(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  auto* event = static_cast<socket_data_event_t*>(data);

  connector->AcceptEvent(*event);
}

void SocketTraceConnector::HandleMySQLProbeOutput(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  auto* event = static_cast<socket_data_event_t*>(data);

  // TODO(oazizi): Use AcceptEvent() to handle reorderings.
  connector->TransferMySQLEvent(*event, connector->record_batch_);
}

void SocketTraceConnector::HandleHTTP2ProbeOutput(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  auto* event = static_cast<socket_data_event_t*>(data);
  connector->AcceptEvent(*event);
}

// This function is invoked by BCC runtime when a item in the perf buffer is not read and lost.
// For now we do nothing.
void SocketTraceConnector::HandleProbeLoss(void* /*cb_cookie*/, uint64_t lost) {
  VLOG(1) << "Possibly lost " << lost << " samples";
  // TODO(oazizi): Can we figure out which perf buffer lost the event?
}

//-----------------------------------------------------------------------------
// Stream Functions
//-----------------------------------------------------------------------------

namespace {

enum class StreamDirection { kUnknown, kSend, kRecv };

static StreamDirection EventStreamDirection(const uint32_t event_type) {
  switch (event_type) {
    case kEventTypeSyscallWriteEvent:
      return StreamDirection::kSend;
    case kEventTypeSyscallSendEvent:
      return StreamDirection::kSend;
    case kEventTypeSyscallReadEvent:
      return StreamDirection::kRecv;
    case kEventTypeSyscallRecvEvent:
      return StreamDirection::kRecv;
    default:
      LOG(ERROR) << "Unexpected event type: " << event_type;
      return StreamDirection::kUnknown;
  }
}

template <typename StreamType>
void AppendToStream(socket_data_event_t event, std::map<uint64_t, StreamType>* streams) {
  const uint64_t stream_id =
      (static_cast<uint64_t>(event.attr.tgid) << 32) | event.attr.conn_info.conn_id;
  const uint64_t seq_num = event.attr.seq_num;
  auto iter = streams->find(stream_id);

  // This is the first event of the stream, so create the stream object.
  if (iter == streams->end()) {
    StreamType new_stream;

    // TODO(yzhao): We should explicitly capture the accept() event into user space to determine the
    // time stamp when the stream is created.
    new_stream.conn.timestamp_ns = event.attr.conn_info.timestamp_ns;
    new_stream.conn.tgid = event.attr.tgid;
    new_stream.conn.fd = event.attr.fd;
    auto ip_endpoint_or = ParseSockAddr(event);
    if (ip_endpoint_or.ok()) {
      new_stream.conn.remote_addr = std::move(ip_endpoint_or.ValueOrDie().ip);
      new_stream.conn.remote_port = ip_endpoint_or.ValueOrDie().port;
    } else {
      LOG(WARNING) << "Could not parse IP address.";
    }
    const auto new_stream_ret = streams->emplace(stream_id, std::move(new_stream));
    DCHECK(new_stream_ret.second) << absl::StrFormat(
        "Tried to insert, but stream_id exists [stream_id = %d].", stream_id);
    iter = new_stream_ret.first;
  }

  StreamType& stream = iter->second;

  switch (EventStreamDirection(event.attr.event_type)) {
    case StreamDirection::kSend:
      stream.send_events.emplace(seq_num, std::move(event));
      break;
    case StreamDirection::kRecv:
      stream.recv_events.emplace(seq_num, std::move(event));
      break;
    default:
      LOG(ERROR) << "AppendStream() could not find StreamDirection";
  }
}

}  // namespace

void SocketTraceConnector::AcceptEvent(socket_data_event_t event) {
  // Need to adjust the clocks to convert to real time.
  event.attr.timestamp_ns += ClockRealTimeOffset();
  event.attr.conn_info.timestamp_ns += ClockRealTimeOffset();
  switch (event.attr.conn_info.traffic_class.protocol) {
    case kProtocolHTTP:
      AppendToStream(std::move(event), &http_streams_);
      break;
    case kProtocolHTTP2:
      AppendToStream(std::move(event), &http2_streams_);
      break;
    default:
      // TODO(oazizi/yzhao): Add MySQL when it goes through streams.
      LOG(WARNING) << "AcceptEvent ignored due to unknown protocol: "
                   << event.attr.conn_info.traffic_class.protocol;
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

//-----------------------------------------------------------------------------
// HTTP Specific TransferImpl Helpers
//-----------------------------------------------------------------------------

void SocketTraceConnector::TransferHTTPStreams(types::ColumnWrapperRecordBatch* record_batch) {
  for (auto& [id, stream] : http_streams_) {
    PL_UNUSED(id);
    HTTPParser& parser = stream.parser;

    // TODO(oazizi): I don't like this way of detecting requestor vs responder. But works
    // temporarily.
    bool is_requestor_side = (config_mask_[stream.protocol] & kSocketTraceSendReq) ||
                             (config_mask_[stream.protocol] & kSocketTraceRecvResp);
    bool is_responder_side = (config_mask_[stream.protocol] & kSocketTraceSendResp) ||
                             (config_mask_[stream.protocol] & kSocketTraceRecvReq);
    CHECK(is_requestor_side ^ is_responder_side)
        << absl::StrFormat("Must be either requestor or responder (and not both)");

    std::map<uint64_t, socket_data_event_t>& events =
        is_requestor_side ? stream.recv_events : stream.send_events;
    std::vector<uint64_t> seq_num_to_remove;

    // Parse all recorded events.
    for (const auto& [seq_num, event] : events) {
      const bool succeeded = parser.Append(seq_num, event.attr.timestamp_ns + ClockRealTimeOffset(),
                                           std::string_view(event.msg, event.attr.msg_size));
      if (!succeeded) {
        break;
      }
    }
    const std::pair<uint64_t, uint64_t> removed_seqs_range =
        parser.ParseMessages(kMessageTypeResponses);

    // Extract and output all complete messages.
    for (HTTPMessage& msg : parser.ExtractHTTPMessages()) {
      HTTPTraceRecord record{stream.conn, std::move(msg)};
      ConsumeHTTPMessage(std::move(record), record_batch);
    }

    // TODO(yzhao): Add the capability to remove events that are too old.
    // TODO(yzhao): Consider change the data structure to a vector, and use sorting to order events
    // before stitching. That might be faster (verify with benchmark).
    for (uint64_t s = removed_seqs_range.first; s < removed_seqs_range.second; ++s) {
      events.erase(s);
    }
  }
}

void SocketTraceConnector::ConsumeHTTPMessage(HTTPTraceRecord record,
                                              types::ColumnWrapperRecordBatch* record_batch) {
  // Only allow certain records to be transferred upstream.
  if (SelectHTTPMessage(record)) {
    // Currently decompresses gzip content, but could handle other transformations too.
    // Note that we do this after filtering to avoid burning CPU cycles unnecessarily.
    PreProcessHTTPRecord(&record);

    // Push data to the TableStore.
    AppendHTTPMessage(std::move(record), record_batch);
  }
}

bool SocketTraceConnector::SelectHTTPMessage(const HTTPTraceRecord& record) {
  // Some of this function is currently a placeholder for the demo.
  // TODO(oazizi/yzhao): update this function further.

  // Rule: Exclude any HTTP requests.
  // TODO(oazizi): Think about how requests should be handled by this function.
  if (record.message.type == SocketTraceEventType::kHTTPRequest) {
    return false;
  }

  // Rule: Exclude anything that doesn't specify its Content-Type.
  auto content_type_iter = record.message.http_headers.find(http_headers::kContentType);
  if (content_type_iter == record.message.http_headers.end()) {
    return false;
  }

  // Rule: Exclude anything that doesn't match the filter, if filter is active.
  if (record.message.type == SocketTraceEventType::kHTTPResponse &&
      (!http_response_header_filter_.inclusions.empty() ||
       !http_response_header_filter_.exclusions.empty())) {
    if (!MatchesHTTPTHeaders(record.message.http_headers, http_response_header_filter_)) {
      return false;
    }
  }

  return true;
}

void SocketTraceConnector::AppendHTTPMessage(HTTPTraceRecord record,
                                             types::ColumnWrapperRecordBatch* record_batch) {
  CHECK_EQ(kHTTPTable.elements().size(), record_batch->size());

  // Check for positive latencies.
  DCHECK_GE(record.message.timestamp_ns, record.conn.timestamp_ns);

  RecordBuilder<&kHTTPTable> r(record_batch);
  r.Append<r.ColIndex("time_")>(record.message.timestamp_ns);
  r.Append<r.ColIndex("tgid")>(record.conn.tgid);
  r.Append<r.ColIndex("fd")>(record.conn.fd);
  r.Append<r.ColIndex("event_type")>(EventTypeToString(record.message.type));
  r.Append<r.ColIndex("remote_addr")>(std::move(record.conn.remote_addr));
  r.Append<r.ColIndex("remote_port")>(record.conn.remote_port);
  r.Append<r.ColIndex("http_minor_version")>(record.message.http_minor_version);
  r.Append<r.ColIndex("http_headers")>(
      absl::StrJoin(record.message.http_headers, "\n", absl::PairFormatter(": ")));
  r.Append<r.ColIndex("http_req_method")>(std::move(record.message.http_req_method));
  r.Append<r.ColIndex("http_req_path")>(std::move(record.message.http_req_path));
  r.Append<r.ColIndex("http_resp_status")>(record.message.http_resp_status);
  r.Append<r.ColIndex("http_resp_message")>(std::move(record.message.http_resp_message));
  r.Append<r.ColIndex("http_resp_body")>(std::move(record.message.http_msg_body));
  r.Append<r.ColIndex("http_resp_latency_ns")>(record.message.timestamp_ns -
                                               record.conn.timestamp_ns);
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

  auto s = ParseSockAddr(event);
  IPEndpoint remote_sockaddr = s.ok() ? s.ConsumeValueOrDie() : IPEndpoint();

  RecordBuilder<&kMySQLTable> r(record_batch);
  r.Append<r.ColIndex("time_")>(event.attr.timestamp_ns + ClockRealTimeOffset());
  r.Append<r.ColIndex("tgid")>(event.attr.tgid);
  r.Append<r.ColIndex("fd")>(event.attr.fd);
  r.Append<r.ColIndex("bpf_event")>(event.attr.event_type);
  r.Append<r.ColIndex("remote_addr")>(std::move(remote_sockaddr.ip));
  r.Append<r.ColIndex("remote_port")>(remote_sockaddr.port);
  r.Append<r.ColIndex("body")>(std::string(event.msg, event.attr.msg_size));
}

}  // namespace stirling
}  // namespace pl

#endif
