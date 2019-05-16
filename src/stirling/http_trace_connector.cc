#ifdef __linux__

#include <algorithm>
#include <vector>

#include "absl/strings/match.h"
#include "src/common/base/base.h"
#include "src/stirling/http_parse.h"
#include "src/stirling/http_trace_connector.h"

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

bool HTTPTraceConnector::SelectForAppend(const HTTPTraceRecord& record) {
  // Some of this function is currently a placeholder for the demo.
  // TODO(oazizi/yzhao): update this function further.

  // Rule: Exclude any HTTP requests.
  if (record.event_type == HTTPTraceEventType::kHTTPRequest) {
    return false;
  }

  const auto content_type_iter = record.http_headers.find(http_header_keys::kContentType);

  // Rule: Exclude anything that doesn't specify its Content-Type.
  if (content_type_iter == record.http_headers.end()) {
    return false;
  }

  // Rule: Exclude anything that doesn't match the filter, if filter is active.
  if (record.event_type == HTTPTraceEventType::kHTTPResponse &&
      (!http_response_header_filter_.inclusions.empty() ||
       !http_response_header_filter_.exclusions.empty())) {
    if (!MatchesHTTPTHeaders(record.http_headers, http_response_header_filter_)) {
      return false;
    }
  }

  return true;
}

void HTTPTraceConnector::AppendToRecordBatch(HTTPTraceRecord record,
                                             types::ColumnWrapperRecordBatch* record_batch) {
  CHECK(record_batch->size() == HTTPTraceConnector::kElements[0].elements().size())
      << "HTTP trace record field count should be: "
      << HTTPTraceConnector::kElements[0].elements().size() << ", got " << record_batch->size();
  auto& columns = *record_batch;
  columns[kTimeStampNs]->Append<types::Time64NSValue>(record.time_stamp_ns);
  columns[kTgid]->Append<types::Int64Value>(record.tgid);
  columns[kPid]->Append<types::Int64Value>(record.pid);
  columns[kFd]->Append<types::Int64Value>(record.fd);
  columns[kEventType]->Append<types::StringValue>(EventTypeToString(record.event_type));
  columns[kSrcAddr]->Append<types::StringValue>(std::move(record.src_addr));
  columns[kSrcPort]->Append<types::Int64Value>(record.src_port);
  columns[kDstAddr]->Append<types::StringValue>(std::move(record.dst_addr));
  columns[kDstPort]->Append<types::Int64Value>(record.dst_port);
  columns[kHTTPMinorVersion]->Append<types::Int64Value>(record.http_minor_version);
  columns[kHTTPHeaders]->Append<types::StringValue>(
      absl::StrJoin(record.http_headers, "\n", absl::PairFormatter(": ")));
  columns[kHTTPReqMethod]->Append<types::StringValue>(std::move(record.http_req_method));
  columns[kHTTPReqPath]->Append<types::StringValue>(std::move(record.http_req_path));
  columns[kHTTPRespStatus]->Append<types::Int64Value>(record.http_resp_status);
  columns[kHTTPRespMessage]->Append<types::StringValue>(std::move(record.http_resp_message));
  columns[kHTTPRespBody]->Append<types::StringValue>(std::move(record.http_resp_body));
  columns[kHTTPRespLatencyNs]->Append<types::Int64Value>(record.time_stamp_ns -
                                                         record.http_start_time_stamp_ns);
  DCHECK_GE(record.time_stamp_ns, record.http_start_time_stamp_ns);
}

void HTTPTraceConnector::ConsumeRecord(HTTPTraceRecord record,
                                       types::ColumnWrapperRecordBatch* record_batch) {
  // Only allow certain records to be transferred upstream.
  if (SelectForAppend(record)) {
    // - Transfer-encoding has to be processed before gzip decompression.
    // - This either is inside SelectForAppend(), but we need to change SelectForAppend() to accept
    // a pointer.
    // - Or this instead of mutates the input, it copies the data to a separate buffer, which is not
    // optimal.
    // For now let it leave outside and for later optimization when more info is known about the
    // overall logical workflow of HTTP trace.
    //
    // TODO(yzhao): Revise with new understanding of the whole workflow.
    if (record.event_type == HTTPTraceEventType::kHTTPResponse) {
      auto iter = record.http_headers.find(http_header_keys::kTransferEncoding);
      if (iter != record.http_headers.end() && iter->second == "chunked") {
        ParseMessageBodyChunked(&record);
      }
      if (record.chunking_status == ChunkingStatus::kChunked) {
        // TODO(PL-519): Revise after message stitching is implemented.
        LOG(INFO) << "Discard chunked http response";
        return;
      }
    }

    // Currently decompresses gzip content, but could handle other transformations too.
    // Note that we do this after filtering to avoid burning CPU cycles unnecessarily.
    PreProcessRecord(&record);

    // Push data to the TableStore.
    AppendToRecordBatch(std::move(record), record_batch);
  }
}

void HTTPTraceConnector::HandleProbeOutput(void* cb_cookie, void* data, int /*data_size*/) {
  if (cb_cookie == nullptr) {
    return;
  }
  auto* event = static_cast<syscall_write_event_t*>(data);
  auto* connector = static_cast<HTTPTraceConnector*>(cb_cookie);
  if (event->attr.event_type == kEventTypeSyscallWriteEvent ||
      event->attr.event_type == kEventTypeSyscallSendEvent) {
    connector->AcceptEvent(*event);
  } else {
    LOG(ERROR) << "Unknown event type: " << event->attr.event_type;
  }
}

void HTTPTraceConnector::OutputEvent(const syscall_write_event_t& event,
                                     types::ColumnWrapperRecordBatch* record_batch) {
  bool succeeded;
  HTTPTraceRecord record;

  // Extract the IP and port.
  succeeded = ParseSockAddr(event, &record);
  if (!succeeded) {
    LOG(ERROR) << "Failed to parse SyscallWriteEvent (addr).";
    return;
  }
  record.http_start_time_stamp_ns = event.attr.accept_info.timestamp_ns + ClockRealTimeOffset();

  // Parse as either a Request, Response, or as Raw (if everything else fails).
  succeeded = ParseHTTPRequest(event, &record) || ParseHTTPResponse(event, &record) ||
              ParseRaw(event, &record);
  if (!succeeded) {
    LOG(ERROR) << "Failed to parse SyscallWriteEvent.";
    return;
  }
  record.time_stamp_ns = event.attr.time_stamp_ns + ClockRealTimeOffset();

  ConsumeRecord(std::move(record), record_batch);
}

// This function is invoked by BCC runtime when a item in the perf buffer is not read and lost.
// For now we do nothing.
void HTTPTraceConnector::HandleProbeLoss(void* /*cb_cookie*/, uint64_t lost) {
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

// This is same as the perf buffer inside bcc_bpf/http_trace.c.
const char kPerfBufferName[] = "syscall_write_events";

}  // namespace

Status HTTPTraceConnector::InitImpl() {
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
  ebpf::StatusTuple open_status = bpf_.open_perf_buffer(
      kPerfBufferName, &HTTPTraceConnector::HandleProbeOutput, &HTTPTraceConnector::HandleProbeLoss,
      // TODO(yzhao): We sort of are not unified around how record_batch and
      // cb_cookie is passed to the callback. Consider unifying them.
      /*cb_cookie*/ this, perf_buffer_page_num_);
  if (open_status.code() != 0) {
    return error::Internal(absl::StrCat("Failed to open perf buffer: ", kPerfBufferName,
                                        ", error message: ", open_status.msg()));
  }
  // TODO(oazizi): if machine is ever suspended, this would have to be called again.
  InitClockRealTimeOffset();
  return Status();
}

Status HTTPTraceConnector::StopImpl() {
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
  ebpf::StatusTuple close_status = bpf_.close_perf_buffer(kPerfBufferName);
  if (close_status.code() != 0) {
    return error::Internal(absl::StrCat("Failed to close perf buffer: ", kPerfBufferName,
                                        ", error message: ", close_status.msg()));
  }
  return Status();
}

void HTTPTraceConnector::PollPerfBuffer() {
  auto perf_buffer = bpf_.get_perf_buffer(kPerfBufferName);
  if (perf_buffer != nullptr) {
    perf_buffer->poll(1);
  }
}

void HTTPTraceConnector::TransferDataImpl(uint32_t table_num,
                                          types::ColumnWrapperRecordBatch* record_batch) {
  CHECK_EQ(table_num, 0ULL) << absl::StrFormat(
      "This connector has only one table, but access to table_num=%d", table_num);
  CHECK(record_batch != nullptr) << "record_batch cannot be nullptr";

  PollPerfBuffer();

  for (const auto& write_stream : write_stream_map_) {
    for (const auto& seq_and_event : write_stream.second) {
      OutputEvent(seq_and_event.second, record_batch);
    }
  }
  write_stream_map_.clear();
}

void HTTPTraceConnector::AcceptEvent(syscall_write_event_t event) {
  const uint64_t stream_id =
      (static_cast<uint64_t>(event.attr.tgid) << 32) | event.attr.accept_info.conn_id;
  // accept_info_t is packed, so we need cast its member to the right type.
  write_stream_map_[stream_id].emplace(static_cast<uint64_t>(event.attr.accept_info.seq_num),
                                       std::move(event));
}

}  // namespace stirling
}  // namespace pl

#endif
