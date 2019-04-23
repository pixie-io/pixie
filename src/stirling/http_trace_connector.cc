#ifdef __linux__

#include <arpa/inet.h>
#include <netinet/in.h>
#include <picohttpparser.h>

#include <deque>
#include <unordered_map>
#include <vector>

#include "absl/strings/str_replace.h"
#include "src/common/base/base.h"
#include "src/stirling/bcc_bpf/http_trace.h"
#include "src/stirling/http_trace_connector.h"

namespace pl {
namespace stirling {
namespace {

// The order here must be identical to HTTPTraceConnector::kElements, and it must start from 0.
// TODO(yzhao): We probably could have some form of template construct to offload part of the
// schema bookkeeping outside of kElements. Today we have a few major issues:
// - When changing field order, we need to update 2 data structures: kElements,
// DataElementsIndexes. Investigate if it's possible to use only one data structure.
// - When runtime check failed, the error information does not show the field index.
// Investigate if it's possible to enforce the check during compilation time.
enum DataElementsIndexes {
  kTimeStampNs = 0,
  kTgid,
  kPid,
  kFd,
  kEventType,
  kSrcAddr,
  kSrcPort,
  kDstAddr,
  kDstPort,
  kHttpMinorVersion,
  kHttpHeaders,
  kHttpReqMethod,
  kHttpReqPath,
  kHttpRespStatus,
  kHttpRespMessage,
};

void ConsumeRecord(HTTPTraceRecord record, types::ColumnWrapperRecordBatch* record_batch) {
  auto& columns = *record_batch;
  columns[kTimeStampNs]->Append<types::Time64NSValue>(record.time_stamp_ns);
  columns[kTgid]->Append<types::Int64Value>(record.tgid);
  columns[kPid]->Append<types::Int64Value>(record.pid);
  columns[kFd]->Append<types::Int64Value>(record.fd);
  columns[kEventType]->Append<types::StringValue>(std::move(record.event_type));
  columns[kSrcAddr]->Append<types::StringValue>(std::move(record.src_addr));
  columns[kSrcPort]->Append<types::Int64Value>(record.src_port);
  columns[kDstAddr]->Append<types::StringValue>(std::move(record.dst_addr));
  columns[kDstPort]->Append<types::Int64Value>(record.dst_port);
  columns[kHttpMinorVersion]->Append<types::Int64Value>(record.http_minor_version);
  columns[kHttpHeaders]->Append<types::StringValue>(std::move(record.http_headers));
  columns[kHttpReqMethod]->Append<types::StringValue>(std::move(record.http_req_method));
  columns[kHttpReqPath]->Append<types::StringValue>(std::move(record.http_req_path));
  columns[kHttpRespStatus]->Append<types::Int64Value>(record.http_resp_status);
  columns[kHttpRespMessage]->Append<types::StringValue>(std::move(record.http_resp_message));
}

void ParseEventAttr(const syscall_write_event_t& event, HTTPTraceRecord* record) {
  record->time_stamp_ns = event.attr.time_stamp_ns;
  record->tgid = event.attr.tgid;
  record->pid = event.attr.pid;
  record->fd = event.attr.fd;
}

std::string ConcatHTTPHeaders(const struct phr_header* headers, int size) {
  std::vector<std::pair<std::string_view, std::string_view>> results;
  results.reserve(size);
  for (int i = 0; i < size; i++) {
    auto name_value = std::make_pair(std::string_view{headers[i].name, headers[i].name_len},
                                     std::string_view{headers[i].value, headers[i].value_len});
    results.push_back(name_value);
  }
  return absl::StrJoin(results, "\n", absl::PairFormatter(":"));
}

bool ParseHTTPRequest(const syscall_write_event_t& event, HTTPTraceRecord* record) {
  const char* method = nullptr;
  size_t method_len = 0;
  const char* path = nullptr;
  size_t path_len = 0;
  int minor_version = 0;
  size_t num_headers = 10;
  struct phr_header headers[num_headers];
  const int retval =
      phr_parse_request(event.msg, event.attr.msg_size, &method, &method_len, &path, &path_len,
                        &minor_version, headers, &num_headers, /*last_len*/ 0);
  HTTPTraceRecord& result = *record;
  if (retval > 0) {
    ParseEventAttr(event, &result);
    result.event_type = "http_request";
    result.http_minor_version = minor_version;
    result.http_headers = ConcatHTTPHeaders(headers, num_headers);
    result.http_req_method = std::string(method, method_len);
    result.http_req_path = std::string(path, path_len);
    return true;
  }
  return false;
}

// TODO(PL-519): Now we discard anything of the response that are not http headers. This is because
// we cannot associate a write() call with the http response. The future work is to keep a list of
// captured data from write() and associate them with the same http response. The rough idea looks
// like as follows:
// time         event type
// t0           write() http response #1 header + body
// t1           write() http response #1 body
// t2           write() http response #1 body
// t3           write() http response #2 header + body
// t4           write() http response #2 body
// ...
//
// We then can squash events at t0, t1, t2 together and concatenate their bodies as the full http
// message. This works in http 1.1 because the responses and requests are not interleaved.
bool ParseHTTPResponse(const syscall_write_event_t& event, HTTPTraceRecord* record) {
  const char* msg = 0;
  size_t msg_len = 0;
  int minor_version = 0;
  int status = 0;
  size_t num_headers = 10;
  struct phr_header headers[num_headers];
  const int retval = phr_parse_response(event.msg, event.attr.msg_size, &minor_version, &status,
                                        &msg, &msg_len, headers, &num_headers, /*last_len*/ 0);
  HTTPTraceRecord& result = *record;
  if (retval > 0) {
    ParseEventAttr(event, &result);
    result.event_type = "http_response";
    result.http_minor_version = minor_version;
    result.http_headers = ConcatHTTPHeaders(headers, num_headers);
    result.http_resp_status = status;
    result.http_resp_message = std::string(msg, msg_len);
    return true;
  }
  return false;
}

// Returns an IP:port pair form parsing the input.
bool ParseSockAddr(const syscall_write_event_t& event, HTTPTraceRecord* record) {
  const auto* sa = reinterpret_cast<const struct sockaddr*>(event.msg);
  char s[INET6_ADDRSTRLEN] = "";
  const auto* sa_in = reinterpret_cast<const struct sockaddr_in*>(sa);
  const auto* sa_in6 = reinterpret_cast<const struct sockaddr_in6*>(sa);
  std::string ip;
  int port = -1;
  switch (sa->sa_family) {
    case AF_INET:
      port = sa_in->sin_port;
      if (inet_ntop(AF_INET, &sa_in->sin_addr, s, INET_ADDRSTRLEN) != nullptr) {
        ip.assign(s);
      }
      break;
    case AF_INET6:
      port = sa_in6->sin6_port;
      if (inet_ntop(AF_INET6, &sa_in6->sin6_addr, s, INET6_ADDRSTRLEN) != nullptr) {
        ip.assign(s);
      }
      break;
  }
  if (!ip.empty()) {
    record->dst_addr = std::move(ip);
    record->dst_port = port;
    return true;
  }
  return false;
}

// This holds the target buffer for recording the events captured in http tracing. It roughly works
// as follows:
// - The data is sent through perf ring buffer.
// - The perf ring buffer is opened with a callback that is executed inside kernel.
// - The callback will write data into this variable.
// - The callback is triggered when TransferDataImpl() calls BPFTable::poll() and there is items in
// the buffer.
// - TransferDataImpl() will assign its input record_batch to this variable, and block during the
// polling.
//
// We need to do this because the callback passed into BPF::open_perf_buffer() is a pure function
// pointer that cannot be customized to on the point to write to a different record batch.
//
// TODO(yzhao): BPF::open_perf_buffer() also accepts a void * cb_cookie that is then passed as the
// first argument to the callback. With that we can remove this global variable, by pass a
// SourceConnector* to BPF::open_perf_buffer() and in HandleProbeOutput(), write the data to
// SourceConnector*. That requires adding a data sink into SourceConnector, which is non-trivial.
//
// TODO(yzhao): A less-possible option: Let the BPF::open_perf_buffer() expose the underlying file
// descriptor, and let TransferDataImpl() directly poll that file descriptor.
types::ColumnWrapperRecordBatch* g_record_batch = nullptr;

}  // namespace

void HTTPTraceConnector::HandleProbeOutput(void* cb_cookie, void* data, int /*data_size*/) {
  if (g_record_batch == nullptr) {
    return;
  }
  auto* record_batch = g_record_batch;
  auto* event = static_cast<syscall_write_event_t*>(data);
  auto* connector = static_cast<HTTPTraceConnector*>(cb_cookie);
  const uint64_t time_offset_ns = connector->ClockRealTimeOffset();
  if (event->attr.event_type == kEventTypeSyscallAddrEvent) {
    HTTPTraceRecord record;
    bool succeeded = ParseSockAddr(*event, &record);
    if (succeeded) {
      record.time_stamp_ns += time_offset_ns;
      connector->UpdateFdRecordMap(event->attr.fd, std::move(record));
    } else {
      LOG(ERROR) << "Failed to parse sockaddr.";
    }
  } else if (event->attr.event_type == kEventTypeSyscallWriteEvent) {
    HTTPTraceRecord record = connector->GetRecordForFd(event->attr.fd);
    const bool http_parse_succeeded =
        ParseHTTPRequest(*event, &record) || ParseHTTPResponse(*event, &record);
    if (http_parse_succeeded) {
      record.time_stamp_ns += time_offset_ns;
      ConsumeRecord(record, record_batch);
    } else {
      LOG(INFO) << "Failed to parse http messages.";
    }
  } else {
    LOG(ERROR) << "Unknown event type: " << event->attr.event_type;
  }
}

// This function is invoked by BCC runtime when a item in the perf buffer is not read and lost.
// For now we do nothing.
// TODO(yzhao): Investigate what should be done here.
void HTTPTraceConnector::HandleProbeLoss(void* /*cb_cookie*/, uint64_t) {}

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
    {"write", "probe_write"},
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
      // real_time_offset_ is passed to the callback. Consider unifying them.
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

void HTTPTraceConnector::TransferDataImpl(types::ColumnWrapperRecordBatch* record_batch) {
  auto perf_buffer = bpf_.get_perf_buffer(kPerfBufferName);
  if (perf_buffer) {
    // Assign the data sink to the global variable accessed by the callback to the perf buffer.
    // See the comments on the g_record_batch for details.
    g_record_batch = record_batch;
    perf_buffer->poll(1);
  }
}

void HTTPTraceConnector::UpdateFdRecordMap(int fd, HTTPTraceRecord record) {
  fd_record_map_[fd] = std::move(record);
}

const HTTPTraceRecord& HTTPTraceConnector::GetRecordForFd(int fd) { return fd_record_map_[fd]; }

}  // namespace stirling
}  // namespace pl

#endif
