#include "src/stirling/bpf_logging.h"

#include "src/common/base/error.h"

DEFINE_bool(enable_bpf_logging, false, "If true, BPF logging facilities are taking effect.");

namespace pl {
namespace stirling {

namespace {

const char kBPFLogEventsPerfBufName[] = "log_events";
const char kBPFLogEventsPreamble[] = "BPF ";

void HandleLog(void* /*cb_cookie*/, void* data, int /*data_size*/) {
  const auto* event = static_cast<const log_event_t*>(data);
  LOG(INFO) << kBPFLogEventsPreamble << std::string_view(event->msg, event->attr.msg_size);
}

}  // namespace

Status InitBPFLogging(ebpf::BPF* bpf) {
  if (!FLAGS_enable_bpf_logging) {
    return Status::OK();
  }
  ebpf::StatusTuple open_status =
      bpf->open_perf_buffer(kBPFLogEventsPerfBufName, &HandleLog,
                            /* lost_cb */ nullptr, /* cb_cookie */ nullptr, /*page number*/ 32);
  if (open_status.code() != 0) {
    return error::Internal(absl::StrCat("Failed to open perf buffer: ", kBPFLogEventsPerfBufName,
                                        ", error message: ", open_status.msg()));
  }
  return Status::OK();
}

void DumpBPFLog(ebpf::BPF* bpf) {
  if (!FLAGS_enable_bpf_logging) {
    return;
  }
  auto perf_buffer = bpf->get_perf_buffer(kBPFLogEventsPerfBufName);
  if (perf_buffer != nullptr) {
    perf_buffer->poll(1);
  }
}

}  // namespace stirling
}  // namespace pl
