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

bool SocketTraceConnector::SelectForAppend(const HTTPTraceRecord& record) {
  // Some of this function is currently a placeholder for the demo.
  // TODO(oazizi/yzhao): update this function further.

  // Rule: Exclude any HTTP requests.
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

void SocketTraceConnector::AppendToRecordBatch(HTTPTraceRecord record,
                                               types::ColumnWrapperRecordBatch* record_batch) {
  CHECK(record_batch->size() == SocketTraceConnector::kElements[0].elements().size())
      << "HTTP trace record field count should be: "
      << SocketTraceConnector::kElements[0].elements().size() << ", got " << record_batch->size();
  auto& columns = *record_batch;
  columns[kTimeStampNs]->Append<types::Time64NSValue>(record.message.time_stamp_ns);
  columns[kTgid]->Append<types::Int64Value>(record.tgid);
  columns[kFd]->Append<types::Int64Value>(record.fd);
  columns[kEventType]->Append<types::StringValue>(EventTypeToString(record.message.type));
  columns[kSrcAddr]->Append<types::StringValue>(std::move(record.src_addr));
  columns[kSrcPort]->Append<types::Int64Value>(record.src_port);
  columns[kDstAddr]->Append<types::StringValue>(std::move(record.dst_addr));
  columns[kDstPort]->Append<types::Int64Value>(record.dst_port);
  columns[kHTTPMinorVersion]->Append<types::Int64Value>(record.message.http_minor_version);
  columns[kHTTPHeaders]->Append<types::StringValue>(
      absl::StrJoin(record.message.http_headers, "\n", absl::PairFormatter(": ")));
  columns[kHTTPReqMethod]->Append<types::StringValue>(std::move(record.message.http_req_method));
  columns[kHTTPReqPath]->Append<types::StringValue>(std::move(record.message.http_req_path));
  columns[kHTTPRespStatus]->Append<types::Int64Value>(record.message.http_resp_status);
  columns[kHTTPRespMessage]->Append<types::StringValue>(
      std::move(record.message.http_resp_message));
  columns[kHTTPRespBody]->Append<types::StringValue>(std::move(record.message.http_resp_body));
  columns[kHTTPRespLatencyNs]->Append<types::Int64Value>(record.message.time_stamp_ns -
                                                         record.http_start_time_stamp_ns);
  DCHECK_GE(record.message.time_stamp_ns, record.http_start_time_stamp_ns);
}

void SocketTraceConnector::ConsumeRecord(HTTPTraceRecord record,
                                         types::ColumnWrapperRecordBatch* record_batch) {
  // Only allow certain records to be transferred upstream.
  if (SelectForAppend(record)) {
    // Currently decompresses gzip content, but could handle other transformations too.
    // Note that we do this after filtering to avoid burning CPU cycles unnecessarily.
    PreProcessRecord(&record);

    // Push data to the TableStore.
    AppendToRecordBatch(std::move(record), record_batch);
  }
}

void SocketTraceConnector::AcceptEvent(socket_data_event_t event) {
  const uint64_t stream_id =
      (static_cast<uint64_t>(event.attr.tgid) << 32) | event.attr.conn_info.conn_id;
  const uint64_t seq_num = static_cast<uint64_t>(event.attr.conn_info.seq_num);
  const auto iter = streams_.find(stream_id);
  if (iter == streams_.end()) {
    // This is the first event of the stream.
    Stream stream;
    // TODO(yzhao): We should explicitly capture the accept() event into user space to determine the
    // time stamp when the stream is created.
    stream.time_stamp_ns = event.attr.conn_info.timestamp_ns;
    stream.tgid = event.attr.tgid;
    stream.fd = event.attr.fd;
    auto ip_endpoint_or = ParseSockAddr(event);
    if (ip_endpoint_or.ok()) {
      stream.remote_ip = std::move(ip_endpoint_or.ValueOrDie().ip);
      stream.remote_port = ip_endpoint_or.ValueOrDie().port;
    } else {
      LOG(WARNING) << "Could not parse IP address.";
    }
    stream.data.emplace(seq_num, std::move(event));
    streams_.emplace(stream_id, std::move(stream));
  } else {
    iter->second.data.emplace(seq_num, std::move(event));
  }
}

void SocketTraceConnector::HandleProbeOutput(void* cb_cookie, void* data, int /*data_size*/) {
  if (cb_cookie == nullptr) {
    return;
  }
  auto* event = static_cast<socket_data_event_t*>(data);
  auto* connector = static_cast<SocketTraceConnector*>(cb_cookie);
  if (event->attr.event_type != kEventTypeSyscallWriteEvent &&
      event->attr.event_type != kEventTypeSyscallSendEvent) {
    LOG(ERROR) << "Unknown event type: " << event->attr.event_type;
    return;
  }

  connector->AcceptEvent(*event);
}

// This function is invoked by BCC runtime when a item in the perf buffer is not read and lost.
// For now we do nothing.
void SocketTraceConnector::HandleProbeLoss(void* /*cb_cookie*/, uint64_t lost) {
  // TODO(yzhao): Lower to VLOG(1) after HTTP tracing reaches production stability.
  VLOG(1) << "Possibly lost " << lost << " samples";
}

namespace {

// Describes a kprobe that should be attached with the BPF::attach_kprobe().
struct ProbeSpec {
  std::string kernel_fn_short_name;
  std::string trace_fn_name;
  int kernel_fn_offset = 0;
  bpf_probe_attach_type attach_type = bpf_probe_attach_type::BPF_PROBE_ENTRY;
};

struct PerfBufferSpec {
  // Name is same as the perf buffer inside bcc_bpf/socket_trace.c.
  std::string name;
  perf_reader_raw_cb probe_output_fn;
  perf_reader_lost_cb probe_loss_fn;
  uint32_t num_pages;
};

const std::vector<ProbeSpec> kProbeSpecs = {
    {"accept4", "probe_entry_accept4"},
    {"accept4", "probe_ret_accept4", 0, bpf_probe_attach_type::BPF_PROBE_RETURN},
    {"write", "probe_entry_write"},
    {"write", "probe_ret_write", 0, bpf_probe_attach_type::BPF_PROBE_RETURN},
    {"send", "probe_entry_send"},
    {"send", "probe_ret_send", 0, bpf_probe_attach_type::BPF_PROBE_RETURN},
    {"sendto", "probe_entry_sendto"},
    {"sendto", "probe_ret_sendto", 0, bpf_probe_attach_type::BPF_PROBE_RETURN},
    {"close", "probe_close"},
};

const std::vector<PerfBufferSpec> kPerfBufferSpecs = {{"socket_http_resp_events",
                                                       &SocketTraceConnector::HandleProbeOutput,
                                                       &SocketTraceConnector::HandleProbeLoss,
                                                       /* num_pages */ 8}};

}  // namespace

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
  // TODO(oazizi): if machine is ever suspended, this would have to be called again.
  InitClockRealTimeOffset();
  return Status();
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
  return Status();
}

void SocketTraceConnector::PollPerfBuffer(uint32_t table_num) {
  auto perf_buffer = bpf_.get_perf_buffer(kPerfBufferSpecs[table_num].name);
  if (perf_buffer != nullptr) {
    perf_buffer->poll(1);
  }
}

void SocketTraceConnector::TransferDataImpl(uint32_t table_num,
                                            types::ColumnWrapperRecordBatch* record_batch) {
  CHECK_LT(table_num, kElements.size())
      << absl::StrFormat("Trying to access unexpected table: table_num=%d", table_num);
  CHECK(record_batch != nullptr) << "record_batch cannot be nullptr";

  PollPerfBuffer(table_num);

  for (auto& [id, stream] : streams_) {
    PL_UNUSED(id);
    HTTPParser& parser = stream.parser;
    std::map<uint64_t, socket_data_event_t>& data = stream.data;
    std::vector<uint64_t> seq_num_to_remove;

    // Parse all recorded events.
    for (const auto& [seq_num, event] : data) {
      std::string_view msg(event.msg, std::min<uint32_t>(event.attr.msg_size, sizeof(event.msg)));
      if (parser.ParseResponse(event) != HTTPParser::ParseState::kUnknown) {
        seq_num_to_remove.push_back(seq_num);
      }
    }

    // Extract and output all complete messages.
    for (HTTPMessage& msg : parser.ExtractHTTPMessages()) {
      HTTPTraceRecord record;

      record.http_start_time_stamp_ns = stream.time_stamp_ns;
      record.tgid = stream.tgid;
      record.fd = stream.fd;
      record.dst_addr = stream.remote_ip;
      record.dst_port = stream.remote_port;
      record.message = std::move(msg);

      ConsumeRecord(std::move(record), record_batch);
    }

    // TODO(yzhao): Add the capability to remove events that are too old.
    // TODO(yzhao): Consider change the data structure to a vector, and use sorting to order events
    // before stitching. That might be faster (verify with benchmark).
    for (const uint64_t seq_num : seq_num_to_remove) {
      data.erase(seq_num);
    }
  }
}

}  // namespace stirling
}  // namespace pl

#endif
