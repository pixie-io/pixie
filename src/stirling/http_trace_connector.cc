#ifdef __linux__

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

void ParseHTTPResponse(const syscall_write_event_t* event) {
  const char* msg = 0;
  size_t msg_len = 0;

  int minor_version = 0;
  size_t num_headers = 10;
  struct phr_header headers[num_headers];

  int status = 0;
  int retval = phr_parse_response(event->msg, event->attr.msg_size, &minor_version, &status, &msg,
                                  &msg_len, headers, &num_headers, 0 /*prevbuflen*/);
  if (retval > 0) {
    // TODO(yzhao): Record interesting data to ColumnWrapperRecordBatch or return to caller for
    // doing that.
  } else {
    // TODO(yzhao): Return error code so that the caller can proceed with parsing the data as HTTP
    // request.
  }
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

// We use cb_cookie (callback cookie) to attach the time offset.
void HandleProbeOutput(void* cb_cookie, void* data, int /*data_size*/) {
  if (g_record_batch == nullptr) {
    return;
  }
  auto* event = static_cast<syscall_write_event_t*>(data);
  auto& columns = *g_record_batch;
  columns[0]->Append<types::Time64NSValue>(event->attr.time_stamp_ns +
                                           *static_cast<uint64_t*>(cb_cookie));
  columns[1]->Append<types::Int64Value>(event->attr.tgid);
  columns[2]->Append<types::Int64Value>(event->attr.pid);
  columns[3]->Append<types::Int64Value>(event->attr.fd);
  if (event->attr.event_type == kEventTypeSyscallWriteEvent) {
    ParseHTTPResponse(event);
    // TODO(yzhao): Add ParseHTTPRequest().
  } else {
    // This has the socket info.
    // TODO(yzhao): Add ParseSockAddr().
  }
}

// This function is invoked by BCC runtime when a item in the perf buffer is not read and lost.
// For now we do nothing.
// TODO(yzhao): Investigate what should be done here.
void HandleProbeLoss(void* /*cb_cookie*/, uint64_t) {}

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
  ebpf::StatusTuple open_status =
      bpf_.open_perf_buffer(kPerfBufferName, &HandleProbeOutput, &HandleProbeLoss,
                            // TODO(yzhao): We sort of are not unified around how record_batch and
                            // real_time_offset_ is passed to the callback. Consider unifying them.
                            /*cb_cookie*/ &real_time_offset_, perf_buffer_page_num_);
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

}  // namespace stirling
}  // namespace pl

#endif
